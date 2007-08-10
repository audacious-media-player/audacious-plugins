#include "libmpc.h"

#define FORCED_THREAD_STACKSIZE 1024 * 1000
#define REMOVE_NONEXISTANT_TAG(x)   if (!*x) { x = NULL; }

using TagLib::MPC::File;
using TagLib::Tag;
using TagLib::String;
using TagLib::APE::ItemListMap;

const gchar *mpc_fmts[] = { "mpc", NULL };

InputPlugin MpcPlugin = {
    NULL,           //File Handle               FILE* handle
    NULL,           //Filename                  char* filename
    (gchar *)"Musepack Audio Plugin",
    mpcOpenPlugin,  //Open Plugin               [CALLBACK]
    mpcAboutBox,    //Show About box            [CALLBACK]
    mpcConfigBox,   //Show Configure box        [CALLBACK]
    mpcIsOurFile,   //Check if it's our file    [CALLBACK]
    NULL,           //Scan the directory        [UNUSED]
    mpcPlay,        //Play                      [CALLBACK]
    mpcStop,        //Stop                      [CALLBACK]
    mpcPause,       //Pause                     [CALLBACK]
    mpcSeek,        //Seek                      [CALLBACK]
    NULL,           //Set EQ                    [CALLBACK]
    mpcGetTime,     //Get Time                  [CALLBACK]
    NULL,           //Get Volume                [UNUSED]
    NULL,           //Set Volume                [UNUSED]
    NULL,           //Close Plugin              [UNUSED]
    NULL,           //Obsolete                  [UNUSED]
    NULL,           //Visual plugins            add_vis_pcm(int time, AFormat fmt, int nch, int length, void *ptr)
    NULL,           //Set Info Settings         set_info(char *title, int length, int rate, int freq, int nch)
    NULL,           //set Info Text             set_info_text(char* text)
    mpcGetSongInfo, //Get Title String callback [CALLBACK]
    mpcFileInfoBox, //Show File Info Box        [CALLBACK]
    NULL,           //Output Plugin Handle      OutputPlugin output
    mpcGetSongTuple,//Acquire tuple for song    [CALLBACK]
    NULL,
    NULL,
    mpcIsOurFD,
    (gchar **)mpc_fmts
};

InputPlugin *mpc_iplist[] = { &MpcPlugin, NULL };

DECLARE_PLUGIN(musepack, NULL, NULL, mpc_iplist, NULL, NULL, NULL, NULL,NULL);

static PluginConfig pluginConfig = {0};
static Widgets      widgets      = {0};
static MpcDecoder   mpcDecoder   = {0};
static TrackInfo    track        = {0};

static GThread            *threadHandle;
GStaticMutex threadMutex = G_STATIC_MUTEX_INIT;

/*
 * VFS callback implementation, adapted from mpc_reader.c.
 * This _IS_ very sick, but it works. -nenolod
 */
static mpc_int32_t
vfs_fread_impl(void *data, void *ptr, mpc_int32_t size)
{
    mpc_reader_file *d = (mpc_reader_file *) data;
    VFSFile *file = (VFSFile *) d->file;

    return (mpc_int32_t) vfs_fread(ptr, 1, size, file);
}

static mpc_bool_t
vfs_fseek_impl(void *data, mpc_int32_t offset)
{
    mpc_reader_file *d = (mpc_reader_file *) data;
    VFSFile *file = (VFSFile *) d->file;

    return d->is_seekable ? vfs_fseek(file, offset, SEEK_SET) == 0 : FALSE;
}

static mpc_int32_t
vfs_ftell_impl(void *data)
{
    mpc_reader_file *d = (mpc_reader_file *) data;
    VFSFile *file = (VFSFile *) d->file;

    return vfs_ftell(file);
}

static mpc_int32_t
vfs_getsize_impl(void *data)
{
    mpc_reader_file *d = (mpc_reader_file *) data;

    return d->file_size;
}

static mpc_bool_t
vfs_canseek_impl(void *data)
{
    mpc_reader_file *d = (mpc_reader_file *) data;

    return d->is_seekable;
}

/*
 * This sets up an mpc_reader_file object to read from VFS instead of libc.
 * Essentially, we use this instead of the normal constructor.
 *    - nenolod
 */
void
mpc_reader_setup_file_vfs(mpc_reader_file *p_reader, VFSFile *input)
{
    p_reader->reader.seek = vfs_fseek_impl;
    p_reader->reader.read = vfs_fread_impl;
    p_reader->reader.tell = vfs_ftell_impl;
    p_reader->reader.get_size = vfs_getsize_impl;
    p_reader->reader.canseek = vfs_canseek_impl;
    p_reader->reader.data = p_reader;

    p_reader->file = (FILE *) input; // no worries, it gets cast back -nenolod
    p_reader->is_seekable = TRUE;    // XXX streams

    vfs_fseek(input, 0, SEEK_END);
    p_reader->file_size = vfs_ftell(input);
    vfs_fseek(input, 0, SEEK_SET);
}

static void mpcOpenPlugin()
{
    ConfigDb *cfg;
    cfg = bmp_cfg_db_open();
    bmp_cfg_db_get_bool(cfg, "musepack", "clipPrevention", &pluginConfig.clipPrevention);
    bmp_cfg_db_get_bool(cfg, "musepack", "albumGain",      &pluginConfig.albumGain);
    bmp_cfg_db_get_bool(cfg, "musepack", "dynamicBitrate", &pluginConfig.dynamicBitrate);
    bmp_cfg_db_get_bool(cfg, "musepack", "replaygain",     &pluginConfig.replaygain);
    bmp_cfg_db_close(cfg);
}

static void mpcAboutBox()
{
    GtkWidget* aboutBox = widgets.aboutBox;
    if (aboutBox)
        gdk_window_raise(aboutBox->window);
    else
    {
        char* titleText      = g_strdup_printf(_("Musepack Decoder Plugin 1.2"));
        const char* contentText = _("Plugin code by\nBenoit Amiaux\nMartin Spuler\nKuniklo\n\nGet latest version at http://musepack.net\n");
        const char* buttonText  = _("Nevermind");
        aboutBox = xmms_show_message(titleText, contentText, buttonText, FALSE, NULL, NULL);
        widgets.aboutBox = aboutBox;
        g_signal_connect(G_OBJECT(aboutBox), "destroy", G_CALLBACK(gtk_widget_destroyed), &widgets.aboutBox);
    }
}

static void mpcConfigBox()
{
    GtkWidget* configBox = widgets.configBox;
    if(configBox)
        gdk_window_raise(configBox->window);
    else
    {
        configBox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint(GTK_WINDOW(configBox), GDK_WINDOW_TYPE_HINT_DIALOG);
        widgets.configBox = configBox;
        g_signal_connect(G_OBJECT(configBox), "destroy", G_CALLBACK(gtk_widget_destroyed), &widgets.configBox);
        gtk_window_set_title(GTK_WINDOW(configBox), _("Musepack Decoder Configuration"));
        gtk_window_set_policy(GTK_WINDOW(configBox), FALSE, FALSE, FALSE);
        gtk_container_border_width(GTK_CONTAINER(configBox), 10);

        GtkWidget* notebook = gtk_notebook_new();
        GtkWidget* vbox = gtk_vbox_new(FALSE, 10);
        gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(configBox), vbox);

        //General Settings Tab
        GtkWidget* generalSet = gtk_frame_new(_("General Settings"));
        gtk_container_border_width(GTK_CONTAINER(generalSet), 5);

        GtkWidget* gSvbox = gtk_vbox_new(FALSE, 10);
        gtk_container_border_width(GTK_CONTAINER(gSvbox), 5);
        gtk_container_add(GTK_CONTAINER(generalSet), gSvbox);

        GtkWidget* bitrateCheck = gtk_check_button_new_with_label(_("Enable Dynamic Bitrate Display"));
        widgets.bitrateCheck = bitrateCheck;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bitrateCheck), pluginConfig.dynamicBitrate);
        gtk_box_pack_start(GTK_BOX(gSvbox), bitrateCheck, FALSE, FALSE, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), generalSet, gtk_label_new(_("Plugin")));

        //ReplayGain Settings Tab
        GtkWidget* replaygainSet = gtk_frame_new(_("ReplayGain Settings"));
        gtk_container_border_width(GTK_CONTAINER(replaygainSet), 5);

        GtkWidget* rSVbox = gtk_vbox_new(FALSE, 10);
        gtk_container_border_width(GTK_CONTAINER(rSVbox), 5);
        gtk_container_add(GTK_CONTAINER(replaygainSet), rSVbox);

        GtkWidget* clippingCheck = gtk_check_button_new_with_label(_("Enable Clipping Prevention"));
        widgets.clippingCheck = clippingCheck;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(clippingCheck), pluginConfig.clipPrevention);
        gtk_box_pack_start(GTK_BOX(rSVbox), clippingCheck, FALSE, FALSE, 0);

        GtkWidget* replaygainCheck = gtk_check_button_new_with_label(_("Enable ReplayGain"));
        widgets.replaygainCheck = replaygainCheck;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(replaygainCheck), pluginConfig.replaygain);
        gtk_box_pack_start(GTK_BOX(rSVbox), replaygainCheck, FALSE, FALSE, 0);

        GtkWidget* replaygainType = gtk_frame_new(_("ReplayGain Type"));
        gtk_box_pack_start(GTK_BOX(rSVbox), replaygainType, FALSE, FALSE, 0);
        g_signal_connect(G_OBJECT(replaygainCheck), "toggled", G_CALLBACK(toggleSwitch), replaygainType);

        GtkWidget* rgVbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(rgVbox), 5);
        gtk_container_add(GTK_CONTAINER(replaygainType), rgVbox);

        GtkWidget* trackCheck = gtk_radio_button_new_with_label(NULL, _("Use Track Gain"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trackCheck), !pluginConfig.albumGain);
        gtk_box_pack_start(GTK_BOX(rgVbox), trackCheck, FALSE, FALSE, 0);

        GtkWidget* albumCheck = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(trackCheck)), _("Use Album Gain"));
        widgets.albumCheck = albumCheck;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(albumCheck), pluginConfig.albumGain);
        gtk_box_pack_start(GTK_BOX(rgVbox), albumCheck, FALSE, FALSE, 0);
        gtk_widget_set_sensitive(replaygainType, pluginConfig.replaygain);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), replaygainSet, gtk_label_new(_("ReplayGain")));

        //Buttons
        GtkWidget* buttonBox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
        gtk_button_box_set_spacing(GTK_BUTTON_BOX(buttonBox), 5);
        gtk_box_pack_start(GTK_BOX(vbox), buttonBox, FALSE, FALSE, 0);

        GtkWidget* okButton = gtk_button_new_with_label(_("Ok"));
        g_signal_connect(G_OBJECT(okButton), "clicked", G_CALLBACK(saveConfigBox), NULL);
        GTK_WIDGET_SET_FLAGS(okButton, GTK_CAN_DEFAULT);
        gtk_box_pack_start(GTK_BOX(buttonBox), okButton, TRUE, TRUE, 0);

        GtkWidget* cancelButton = gtk_button_new_with_label(_("Cancel"));
        g_signal_connect_swapped(G_OBJECT(cancelButton), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(widgets.configBox));
        GTK_WIDGET_SET_FLAGS(cancelButton, GTK_CAN_DEFAULT);
        gtk_widget_grab_default(cancelButton);
        gtk_box_pack_start(GTK_BOX(buttonBox), cancelButton, TRUE, TRUE, 0);

        gtk_widget_show_all(configBox);
    }
}

static void toggleSwitch(GtkWidget* p_Widget, gpointer p_Data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(p_Data), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_Widget)));
}

static void saveConfigBox(GtkWidget* p_Widget, gpointer p_Data)
{
    ConfigDb* cfg;
    GtkToggleButton* tb;

    tb = GTK_TOGGLE_BUTTON(widgets.replaygainCheck);
    pluginConfig.replaygain = gtk_toggle_button_get_active(tb);
    tb = GTK_TOGGLE_BUTTON(widgets.clippingCheck);
    pluginConfig.clipPrevention = gtk_toggle_button_get_active(tb);
    tb = GTK_TOGGLE_BUTTON(widgets.bitrateCheck);
    pluginConfig.dynamicBitrate = gtk_toggle_button_get_active(tb);
    tb = GTK_TOGGLE_BUTTON(widgets.albumCheck);
    pluginConfig.albumGain = gtk_toggle_button_get_active(tb);

    cfg = bmp_cfg_db_open();

    bmp_cfg_db_set_bool(cfg, "musepack", "clipPrevention", pluginConfig.clipPrevention);
    bmp_cfg_db_set_bool(cfg, "musepack", "albumGain",      pluginConfig.albumGain);
    bmp_cfg_db_set_bool(cfg, "musepack", "dynamicBitrate", pluginConfig.dynamicBitrate);
    bmp_cfg_db_set_bool(cfg, "musepack", "replaygain",     pluginConfig.replaygain);

    bmp_cfg_db_close(cfg);

    gtk_widget_destroy (widgets.configBox);
}

static int mpcIsOurFile(char* p_Filename)
{
    VFSFile *file;
    gchar magic[3];
    if ((file = vfs_fopen(p_Filename, "rb"))) {
        vfs_fread(magic, 1, 3, file);
        if (!memcmp(magic, "MP+", 3)) {
             vfs_fclose(file);
             return 1;
        }
        vfs_fclose(file);
    }
    return 0;
}

static int mpcIsOurFD(char* p_Filename, VFSFile* file)
{
    gchar magic[3];
    vfs_fread(magic, 1, 3, file);
    if (!memcmp(magic, "MP+", 3))
        return 1;
    return 0;
}

static void mpcPlay(InputPlayback *data)
{
    mpcDecoder.offset   = -1;
    mpcDecoder.isAlive  = true;
    mpcDecoder.isOutput = false;
    mpcDecoder.isPause  = false;
    threadHandle = g_thread_self();
    data->set_pb_ready(data);
    decodeStream((void *) g_strdup(data->filename));
}

static void mpcStop(InputPlayback *data)
{
    setAlive(false);
    if (threadHandle)
    {
        g_thread_join(threadHandle);
        if (mpcDecoder.isOutput)
        {
            data->output->buffer_free();
            data->output->close_audio();
            mpcDecoder.isOutput = false;
        }
    }
}

static void mpcPause(InputPlayback *data, short p_Pause)
{
    lockAcquire();
    mpcDecoder.isPause = p_Pause;
    data->output->pause(p_Pause);
    lockRelease();
}

static void mpcSeek(InputPlayback *data, int p_Offset)
{
    lockAcquire();
    mpcDecoder.offset = static_cast<double> (p_Offset);
    data->output->flush(1000 * p_Offset);
    lockRelease();
}

static int mpcGetTime(InputPlayback *data)
{
    if(!isAlive())
        return -1;
    return data->output->output_time();
}

static Tuple *mpcGetSongTuple(char* p_Filename)
{
    VFSFile *input = vfs_fopen(p_Filename, "rb");
    Tuple *tuple = NULL;

    if(input)
    {
        tuple = tuple_new_from_filename(p_Filename);

        MpcInfo tags = getTags(p_Filename);

        tuple_associate_string(tuple, "date", tags.date);
        tuple_associate_string(tuple, "title", tags.title);
        tuple_associate_string(tuple, "artist", tags.artist);
        tuple_associate_string(tuple, "album", tags.album);
        tuple_associate_int(tuple, "track-number", tags.track);
        tuple_associate_int(tuple, "year", tags.year);
        tuple_associate_string(tuple, "genre", tags.genre);
        tuple_associate_string(tuple, "comment", tags.comment);

        tuple_associate_string(tuple, "codec", "Musepack");
        tuple_associate_string(tuple, "quality", "lossy");

        freeTags(tags);

        mpc_streaminfo info;
        mpc_reader_file reader;
        mpc_reader_setup_file_vfs(&reader, input);
        mpc_streaminfo_read(&info, &reader.reader);

        tuple_associate_int(tuple, "length", static_cast<int> (1000 * mpc_streaminfo_get_length(&info)));
        vfs_fclose(input);
    }
    else
    {
        char* temp = g_strdup_printf("[xmms-musepack] mpcGetSongInfo is unable to open %s\n", p_Filename);
        perror(temp);
        free(temp);
    }

    return tuple;
}

static void mpcGetSongInfo(char* p_Filename, char** p_Title, int* p_Length)
{
    VFSFile *input = vfs_fopen(p_Filename, "rb");
    if(input)
    {
        MpcInfo tags = getTags(p_Filename);
        *p_Title = mpcGenerateTitle(tags, p_Filename);
        freeTags(tags);
        mpc_streaminfo info;
        mpc_reader_file reader;
        mpc_reader_setup_file_vfs(&reader, input);
        mpc_streaminfo_read(&info, &reader.reader);
        *p_Length = static_cast<int> (1000 * mpc_streaminfo_get_length(&info));
        vfs_fclose(input);
    }
    else
    {
        char* temp = g_strdup_printf("[xmms-musepack] mpcGetSongInfo is unable to open %s\n", p_Filename);
        perror(temp);
        free(temp);
    }
}

static void freeTags(MpcInfo& tags)
{
    free(tags.title);
    free(tags.artist);
    free(tags.album);
    free(tags.comment);
    free(tags.genre);
    free(tags.date);
}

static MpcInfo getTags(const char* p_Filename)
{
    File oFile(p_Filename, false);
    Tag* poTag = oFile.tag();
    MpcInfo tags = {0};
    tags.title   = g_strdup(poTag->title().toCString(true));
    REMOVE_NONEXISTANT_TAG(tags.title);
    tags.artist  = g_strdup(poTag->artist().toCString(true));
    REMOVE_NONEXISTANT_TAG(tags.artist);
    tags.album   = g_strdup(poTag->album().toCString(true));
    REMOVE_NONEXISTANT_TAG(tags.album);
    tags.genre   = g_strdup(poTag->genre().toCString(true));
    REMOVE_NONEXISTANT_TAG(tags.genre);
    tags.comment = g_strdup(poTag->comment().toCString(true));
    REMOVE_NONEXISTANT_TAG(tags.comment);
    tags.year    = poTag->year();
    tags.track   = poTag->track();
#if 0
    TagLib::APE::Tag* ape = oFile.APETag(false);
    if(ape)
    {
        ItemListMap map = ape->itemListMap();
        if(map.contains("YEAR"))
        {
            tags.date = g_strdup(map["YEAR"].toString().toCString(true));
        }
        else
        {
            tags.date = g_strdup_printf("%d", tags.year);
        }
    }
#endif
    return tags;
}

static void mpcFileInfoBox(char* p_Filename)
{
    GtkWidget* infoBox = widgets.infoBox;

    if(infoBox)
        gdk_window_raise(infoBox->window);
    else
    {
        infoBox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint(GTK_WINDOW(infoBox), GDK_WINDOW_TYPE_HINT_DIALOG);
        widgets.infoBox = infoBox;
        gtk_window_set_policy(GTK_WINDOW(infoBox), FALSE, FALSE, FALSE);
        g_signal_connect(G_OBJECT(infoBox), "destroy", G_CALLBACK(closeInfoBox), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(infoBox), 10);

        GtkWidget* iVbox = gtk_vbox_new(FALSE, 10);
        gtk_container_add(GTK_CONTAINER(infoBox), iVbox);

        GtkWidget* filenameHbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(iVbox), filenameHbox, FALSE, TRUE, 0);

        GtkWidget* fileLabel = gtk_label_new(_("Filename:"));
        gtk_box_pack_start(GTK_BOX(filenameHbox), fileLabel, FALSE, TRUE, 0);

        GtkWidget* fileEntry = gtk_entry_new();
        widgets.fileEntry = fileEntry;
        gtk_editable_set_editable(GTK_EDITABLE(fileEntry), FALSE);
        gtk_box_pack_start(GTK_BOX(filenameHbox), fileEntry, TRUE, TRUE, 0);

        GtkWidget* iHbox = gtk_hbox_new(FALSE, 10);
        gtk_box_pack_start(GTK_BOX(iVbox), iHbox, FALSE, TRUE, 0);

        GtkWidget* leftBox = gtk_vbox_new(FALSE, 10);
        gtk_box_pack_start(GTK_BOX(iHbox), leftBox, FALSE, FALSE, 0);

        //Tag labels
        GtkWidget* tagFrame = gtk_frame_new(_("Musepack Tag"));
        gtk_box_pack_start(GTK_BOX(leftBox), tagFrame, FALSE, FALSE, 0);
        gtk_widget_set_sensitive(tagFrame, TRUE);

        GtkWidget* iTable = gtk_table_new(5, 5, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(iTable), 5);
        gtk_container_add(GTK_CONTAINER(tagFrame), iTable);

        mpcGtkTagLabel(_("Title:"), 0, 1, 0, 1, iTable);
        GtkWidget* titleEntry = mpcGtkTagEntry(1, 4, 0, 1, 0, iTable);
        widgets.titleEntry = titleEntry;

        mpcGtkTagLabel(_("Artist:"), 0, 1, 1, 2, iTable);
        GtkWidget* artistEntry = mpcGtkTagEntry(1, 4, 1, 2, 0, iTable);
        widgets.artistEntry = artistEntry;

        mpcGtkTagLabel(_("Album:"), 0, 1, 2, 3, iTable);
        GtkWidget* albumEntry = mpcGtkTagEntry(1, 4, 2, 3, 0, iTable);
        widgets.albumEntry = albumEntry;

        mpcGtkTagLabel(_("Comment:"), 0, 1, 3, 4, iTable);
        GtkWidget* commentEntry = mpcGtkTagEntry(1, 4, 3, 4, 0, iTable);
        widgets.commentEntry = commentEntry;

        mpcGtkTagLabel(_("Year:"), 0, 1, 4, 5, iTable);
        GtkWidget* yearEntry = mpcGtkTagEntry(1, 2, 4, 5, 4, iTable);
        widgets.yearEntry = yearEntry;
        gtk_widget_set_usize(yearEntry, 4, -1);

        mpcGtkTagLabel(_("Track:"), 2, 3, 4, 5, iTable);
        GtkWidget* trackEntry = mpcGtkTagEntry(3, 4, 4, 5, 4, iTable);
        widgets.trackEntry = trackEntry;
        gtk_widget_set_usize(trackEntry, 3, -1);

        mpcGtkTagLabel(_("Genre:"), 0, 1, 5, 6, iTable);
        GtkWidget* genreEntry = mpcGtkTagEntry(1, 4, 5, 6, 0, iTable);
        widgets.genreEntry = genreEntry;
        gtk_widget_set_usize(genreEntry, 20, -1);

        //Buttons
        GtkWidget* buttonBox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
        gtk_button_box_set_spacing(GTK_BUTTON_BOX(buttonBox), 5);
        gtk_box_pack_start(GTK_BOX(leftBox), buttonBox, FALSE, FALSE, 0);

        GtkWidget* saveButton = mpcGtkButton(_("Save"), buttonBox);
        g_signal_connect(G_OBJECT(saveButton), "clicked", G_CALLBACK(saveTags), NULL);

        GtkWidget* removeButton = mpcGtkButton(_("Remove Tag"), buttonBox);
        g_signal_connect_swapped(G_OBJECT(removeButton), "clicked", G_CALLBACK(removeTags), NULL);

        GtkWidget* cancelButton = mpcGtkButton(_("Cancel"), buttonBox);
        g_signal_connect_swapped(G_OBJECT(cancelButton), "clicked", G_CALLBACK(closeInfoBox), NULL);
        gtk_widget_grab_default(cancelButton);

        //File information
        GtkWidget* infoFrame = gtk_frame_new(_("Musepack Info"));
        gtk_box_pack_start(GTK_BOX(iHbox), infoFrame, FALSE, FALSE, 0);

        GtkWidget* infoVbox = gtk_vbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(infoFrame), infoVbox);
        gtk_container_set_border_width(GTK_CONTAINER(infoVbox), 10);
        gtk_box_set_spacing(GTK_BOX(infoVbox), 0);

        GtkWidget* streamLabel    = mpcGtkLabel(infoVbox);
        GtkWidget* encoderLabel   = mpcGtkLabel(infoVbox);
        GtkWidget* profileLabel   = mpcGtkLabel(infoVbox);
        GtkWidget* bitrateLabel   = mpcGtkLabel(infoVbox);
        GtkWidget* rateLabel      = mpcGtkLabel(infoVbox);
        GtkWidget* channelsLabel  = mpcGtkLabel(infoVbox);
        GtkWidget* lengthLabel    = mpcGtkLabel(infoVbox);
        GtkWidget* fileSizeLabel  = mpcGtkLabel(infoVbox);
        GtkWidget* trackPeakLabel = mpcGtkLabel(infoVbox);
        GtkWidget* trackGainLabel = mpcGtkLabel(infoVbox);
        GtkWidget* albumPeakLabel = mpcGtkLabel(infoVbox);
        GtkWidget* albumGainLabel = mpcGtkLabel(infoVbox);

        VFSFile *input = vfs_fopen(p_Filename, "rb");
        if(input)
        {
            mpc_streaminfo info;
            mpc_reader_file reader;
            mpc_reader_setup_file_vfs(&reader, input);
            mpc_streaminfo_read(&info, &reader.reader);

            int time = static_cast<int> (mpc_streaminfo_get_length(&info));
            int minutes = time / 60;
            int seconds = time % 60;

            mpcGtkPrintLabel(streamLabel,    _("Streamversion %d"), info.stream_version);
            mpcGtkPrintLabel(encoderLabel,   _("Encoder: %s"), info.encoder);
            mpcGtkPrintLabel(profileLabel,   _("Profile: %s"), info.profile_name);
            mpcGtkPrintLabel(bitrateLabel,   _("Average bitrate: %6.1f kbps"), info.average_bitrate * 1.e-3);
            mpcGtkPrintLabel(rateLabel,      _("Samplerate: %d Hz"), info.sample_freq);
            mpcGtkPrintLabel(channelsLabel,  _("Channels: %d"), info.channels);
            mpcGtkPrintLabel(lengthLabel,    _("Length: %d:\%.2d"), minutes, seconds);
            mpcGtkPrintLabel(fileSizeLabel,  _("File size: %d Bytes"), info.total_file_length);
            mpcGtkPrintLabel(trackPeakLabel, _("Track Peak: %5u"), info.peak_title);
            mpcGtkPrintLabel(trackGainLabel, _("Track Gain: %-+2.2f dB"), 0.01 * info.gain_title);
            mpcGtkPrintLabel(albumPeakLabel, _("Album Peak: %5u"), info.peak_album);
            mpcGtkPrintLabel(albumGainLabel, _("Album Gain: %-+5.2f dB"), 0.01 * info.gain_album);

            MpcInfo tags = getTags(p_Filename);
            gtk_entry_set_text(GTK_ENTRY(titleEntry),   tags.title);
            gtk_entry_set_text(GTK_ENTRY(artistEntry),  tags.artist);
            gtk_entry_set_text(GTK_ENTRY(albumEntry),   tags.album);
            gtk_entry_set_text(GTK_ENTRY(commentEntry), tags.comment);
            gtk_entry_set_text(GTK_ENTRY(genreEntry),   tags.genre);
            char* entry = g_strdup_printf ("%d", tags.track);
            gtk_entry_set_text(GTK_ENTRY(trackEntry), entry);
            free(entry);
            entry = g_strdup_printf ("%d", tags.year);
            gtk_entry_set_text(GTK_ENTRY(yearEntry), entry);
            free(entry);
            entry = g_filename_display_name(p_Filename);
            gtk_entry_set_text(GTK_ENTRY(fileEntry), entry);
            free(entry);
            freeTags(tags);
            vfs_fclose(input);
        }
        else
        {
            char* temp = g_strdup_printf("[xmms-musepack] mpcFileInfoBox is unable to read tags from %s", p_Filename);
            perror(temp);
            free(temp);
        }

	char* name = g_filename_display_basename(p_Filename);
        char* text = g_strdup_printf(_("File Info - %s"), name);
        free(name);
        gtk_window_set_title(GTK_WINDOW(infoBox), text);
        free(text);

        gtk_widget_show_all(infoBox);
    }
}

static void mpcGtkPrintLabel(GtkWidget* widget, const char* format,...)
{
    va_list args;

    va_start(args, format);
    char* temp = g_strdup_vprintf(format, args);
    va_end(args);

    gtk_label_set_text(GTK_LABEL(widget), temp);
    free(temp);
}

static GtkWidget* mpcGtkTagLabel(const char* p_Text, int a, int b, int c, int d, GtkWidget* p_Box)
{
    GtkWidget* label = gtk_label_new(p_Text);
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(p_Box), label, a, b, c, d, GTK_FILL, GTK_FILL, 5, 5);
    return label;
}

static GtkWidget* mpcGtkTagEntry(int a, int b, int c, int d, int p_Size, GtkWidget* p_Box)
{
    GtkWidget* entry;
    if(p_Size == 0)
        entry = gtk_entry_new();
    else
        entry = gtk_entry_new_with_max_length(p_Size);
    gtk_table_attach(GTK_TABLE(p_Box), entry, a, b, c, d,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);
    return entry;
}

static GtkWidget* mpcGtkLabel(GtkWidget* p_Box)
{
    GtkWidget* label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(p_Box), label, FALSE, FALSE, 0);
    return label;
}

static GtkWidget* mpcGtkButton(const char* p_Text, GtkWidget* p_Box)
{
    GtkWidget* button = gtk_button_new_with_label(p_Text);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(p_Box), button, TRUE, TRUE, 0);
    return button;
}

static void removeTags(GtkWidget * w, gpointer data)
{
    File oFile(gtk_entry_get_text(GTK_ENTRY(widgets.fileEntry)));
    oFile.remove();
    oFile.save();
    closeInfoBox(NULL, NULL);
}

static void saveTags(GtkWidget* w, gpointer data)
{
    File oFile(gtk_entry_get_text(GTK_ENTRY(widgets.fileEntry)));
    Tag* poTag = oFile.tag();

    char* cAlbum   = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.albumEntry)));
    char* cArtist  = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.artistEntry)));
    char* cTitle   = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.titleEntry)));
    char* cGenre   = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.genreEntry)));
    char* cComment = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets.commentEntry)));

    const String album   = String(cAlbum,   TagLib::String::UTF8);
    const String artist  = String(cArtist,  TagLib::String::UTF8);
    const String title   = String(cTitle,   TagLib::String::UTF8);
    const String genre   = String(cGenre,   TagLib::String::UTF8);
    const String comment = String(cComment, TagLib::String::UTF8);

    poTag->setAlbum(album);
    poTag->setArtist(artist);
    poTag->setTitle(title);
    poTag->setGenre(genre);
    poTag->setComment(comment);
    poTag->setYear(atoi(gtk_entry_get_text(GTK_ENTRY(widgets.yearEntry))));
    poTag->setTrack(atoi(gtk_entry_get_text(GTK_ENTRY(widgets.trackEntry))));

    free(cAlbum);
    free(cArtist);
    free(cTitle);
    free(cGenre);
    free(cComment);

    oFile.save();
    closeInfoBox(NULL, NULL);
}

static void closeInfoBox(GtkWidget* w, gpointer data)
{
    gtk_widget_destroy(widgets.infoBox);
    widgets.infoBox = NULL;
}

static char* mpcGenerateTitle(const MpcInfo& p_Tags, char* p_Filename)
{
    Tuple* tuple = mpcGetSongTuple(p_Filename);

    char* title = tuple_formatter_process_string(tuple, cfg.gentitle_format);

    if (!*title)
        title = g_strdup(tuple_get_string(tuple, "file-name"));

    mowgli_object_unref((void *) tuple);
    return title;
}

static void* endThread(char* p_FileName, VFSFile * p_FileHandle, bool release)
{
    free(p_FileName);
    if(release)
        lockRelease();
    if(mpcDecoder.isError)
    {
        perror(mpcDecoder.isError);
        free(mpcDecoder.isError);
        mpcDecoder.isError = NULL;
    }
    setAlive(false);
    if(p_FileHandle)
        vfs_fclose(p_FileHandle);
    if(track.display)
    {
        free(track.display);
        track.display = NULL;
    }
    return 0;
}

static void* decodeStream(void* data)
{
    lockAcquire();
    char* filename = static_cast<char*> (data);
    VFSFile *input = vfs_fopen(filename, "rb");
    if (!input)
    {
        mpcDecoder.isError = g_strdup_printf("[xmms-musepack] decodeStream is unable to open %s", filename);
        return endThread(filename, input, true);
    }

    mpc_reader_file reader;
    mpc_reader_setup_file_vfs(&reader, input);

    mpc_streaminfo info;
    if (mpc_streaminfo_read(&info, &reader.reader) != ERROR_CODE_OK)
    {
        mpcDecoder.isError = g_strdup_printf("[xmms-musepack] decodeStream is unable to read %s", filename);
        return endThread(filename, input, true);
    }

    MpcInfo tags     = getTags(filename);
    track.display    = mpcGenerateTitle(tags, filename);
    track.length     = static_cast<int> (1000 * mpc_streaminfo_get_length(&info));
    track.bitrate    = static_cast<int> (info.average_bitrate);
    track.sampleFreq = info.sample_freq;
    track.channels   = info.channels;
    freeTags(tags);

    MpcPlugin.set_info(track.display, track.length, track.bitrate, track.sampleFreq, track.channels);

    mpc_decoder decoder;
    mpc_decoder_setup(&decoder, &reader.reader);
    if (!mpc_decoder_initialize(&decoder, &info))
    {
        mpcDecoder.isError = g_strdup_printf("[xmms-musepack] decodeStream is unable to initialize decoder on %s", filename);
        return endThread(filename, input, true);
    }

    setReplaygain(info, decoder);

    MPC_SAMPLE_FORMAT sampleBuffer[MPC_DECODER_BUFFER_LENGTH];
    char xmmsBuffer[MPC_DECODER_BUFFER_LENGTH * 4];

    if (!MpcPlugin.output->open_audio(FMT_S16_LE, track.sampleFreq, track.channels))
    {
        mpcDecoder.isError = g_strdup_printf("[xmms-musepack] decodeStream is unable to open an audio output");
        return endThread(filename, input, true);
    }
    else
    {
        mpcDecoder.isOutput = true;
    }

    lockRelease();

    int counter = 2 * track.sampleFreq / 3;
    while (isAlive())
    {
        if (getOffset() != -1)
        {
            mpc_decoder_seek_seconds(&decoder, mpcDecoder.offset);
            setOffset(-1);
        }

        lockAcquire();
        short iPlaying = MpcPlugin.output->buffer_playing()? 1 : 0;
        int iFree = MpcPlugin.output->buffer_free();
        if (!mpcDecoder.isPause &&  iFree >= ((1152 * 4) << iPlaying))
        {
            unsigned status = processBuffer(sampleBuffer, xmmsBuffer, decoder);
            if (status == (unsigned) (-1))
            {
                mpcDecoder.isError = g_strdup_printf("[xmms-musepack] error from internal decoder on %s", filename);
                return endThread(filename, input, true);
            }
            if (status == 0 && iPlaying == 0)
                return endThread(filename, input, true);

            lockRelease();

            if(pluginConfig.dynamicBitrate)
            {
                counter -= status;
                if(counter < 0)
                {
                    MpcPlugin.set_info(track.display, track.length, track.bitrate, track.sampleFreq, track.channels);
                    counter = 2 * track.sampleFreq / 3;
                }
            }
        }
        else
        {
            lockRelease();
            xmms_usleep(10000);
        }
    }
    return endThread(filename, input, false);
}

static int processBuffer(MPC_SAMPLE_FORMAT* sampleBuffer, char* xmmsBuffer, mpc_decoder& decoder)
{
    mpc_uint32_t vbrAcc = 0;
    mpc_uint32_t vbrUpd = 0;

    unsigned status  = mpc_decoder_decode(&decoder, sampleBuffer, &vbrAcc, &vbrUpd);
    copyBuffer(sampleBuffer, xmmsBuffer, status);

    if (pluginConfig.dynamicBitrate)
    {
        track.bitrate = static_cast<int> (vbrUpd * track.sampleFreq / 1152);
    }

    produce_audio(MpcPlugin.output->written_time(), FMT_S16_LE, track.channels, status * 4, xmmsBuffer, NULL);
    return status;
}

static void setReplaygain(mpc_streaminfo& info, mpc_decoder& decoder)
{
    if(!pluginConfig.replaygain && !pluginConfig.clipPrevention)
        return;

    int peak    = pluginConfig.albumGain ? info.peak_album : info.peak_title;
    double gain = pluginConfig.albumGain ? info.gain_album : info.gain_title;

    if(!peak)
        peak = 32767;
    if(!gain)
        gain = 1.;

    double clip = 32767. / peak;
    gain = exp((M_LN10 / 2000.) * gain);

    if(pluginConfig.clipPrevention && !pluginConfig.replaygain)
        gain = clip;
    else if(pluginConfig.replaygain && pluginConfig.clipPrevention)
        if(clip < gain)
            gain = clip;

    mpc_decoder_scale_output(&decoder, gain);
}

inline static void lockAcquire()
{
    g_static_mutex_lock(&threadMutex);
}

inline static void lockRelease()
{
    g_static_mutex_unlock(&threadMutex);
}

inline static bool isAlive()
{
    lockAcquire();
    bool isAlive = mpcDecoder.isAlive;
    lockRelease();
    return isAlive;
}

inline static bool isPause()
{
    lockAcquire();
    bool isPause = mpcDecoder.isPause;
    lockRelease();
    return isPause;
}

inline static void setAlive(bool isAlive)
{
    lockAcquire();
    mpcDecoder.isAlive = isAlive;
    lockRelease();
}

inline static double getOffset()
{
    lockAcquire();
    double offset = mpcDecoder.offset;
    lockRelease();
    return offset;
}

inline static void setOffset(double offset)
{
    lockAcquire();
    mpcDecoder.offset = offset;
    lockRelease();
}
