
#include "config.h"
#include "common.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>

static GtkWidget *window = NULL,
        *title_entry, *album_entry, *performer_entry, *tracknumber_entry,
        *date_entry, *genre_entry, *user_comment_entry;

static void
label_set_text(GtkWidget * label, const char *str, ...)
{
    va_list args;
    gchar *tempstr;

    va_start(args, str);
    tempstr = g_strdup_vprintf(str, args);
    va_end(args);

    gtk_label_set_text(GTK_LABEL(label), tempstr);
    g_free(tempstr);
}

static void
tag_remove_cb(GtkWidget * w, gpointer filename)
{
//    DeleteTag((gchar *) filename);
    g_free(filename);
    gtk_widget_destroy(window);
}

static void
tag_save_cb(GtkWidget * w, gpointer filename)
{
    WavPackTag Tag;

    strncpy(Tag.title, gtk_entry_get_text(GTK_ENTRY(title_entry)), MAX_LEN);
    strncpy(Tag.artist, gtk_entry_get_text(GTK_ENTRY(performer_entry)), MAX_LEN);
    strncpy(Tag.album, gtk_entry_get_text(GTK_ENTRY(album_entry)), MAX_LEN);
    strncpy(Tag.comment, gtk_entry_get_text(GTK_ENTRY(user_comment_entry)), MAX_LEN);
    strncpy(Tag.track, gtk_entry_get_text(GTK_ENTRY(tracknumber_entry)), MAX_LEN2);
    strncpy(Tag.year, gtk_entry_get_text(GTK_ENTRY(date_entry)), MAX_LEN2);
    strncpy(Tag.genre, gtk_entry_get_text(GTK_ENTRY(genre_entry)), MAX_LEN);
//    WriteAPE2Tag((gchar *) filename, &Tag);
    g_free(filename);
    gtk_widget_destroy(window);
}

static void
fileinfo_close_window(GtkWidget * w, gpointer filename)
{
    g_free(filename);
    gtk_widget_destroy(window);
}

void
wv_file_info_box(const gchar * fn)
{
    static GtkWidget *info_frame, *info_box, *bitrate_label, *rate_label;
    static GtkWidget *version_label, *bits_per_sample_label;
    static GtkWidget *channel_label, *length_label, *filesize_label;
    static GtkWidget *peakTitle_label, *peakAlbum_label, *gainTitle_label;
    static GtkWidget *gainAlbum_label, *filename_entry, *tag_frame;
    gint time, minutes, seconds;
    gchar *tmp, error[1024]; // TODO: fixme!
    WavpackContext *ctx;
    WavPackTag tag;
    VFSFile *fd;

    if (fn == NULL)
        return;

    fd = aud_vfs_fopen(fn, "rb");
    if (fd == NULL)
        return;

    ctx = WavpackOpenFileInputEx(&wv_readers, fd, NULL, error, OPEN_TAGS, 0);

    if (ctx == NULL)
    {
        aud_vfs_fclose(fd);
        g_warning("Error opening file: \"%s: %s\"\n", fn, error);
        return;
    }

#ifdef DEBUG
    gint sample_rate = WavpackGetSampleRate(ctx);
    gint num_channels = WavpackGetNumChannels(ctx);
#endif

    wv_get_tags(&tag, ctx);
    AUDDBG("opened %s at %d rate with %d channels\n", fn, sample_rate, num_channels);


    if (!window)
    {
        GtkWidget *hbox, *label, *filename_hbox, *vbox, *left_vbox;
        GtkWidget *table, *bbox, *cancel_button;
        GtkWidget *save_button, *remove_button;

        gchar *filename = g_strdup(fn);

        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
        g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_widget_destroyed), &window);
        gtk_container_set_border_width(GTK_CONTAINER(window), 10);

        vbox = gtk_vbox_new(FALSE, 10);
        gtk_container_add(GTK_CONTAINER(window), vbox);

        filename_hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), filename_hbox, FALSE, TRUE, 0);

        label = gtk_label_new(_("Filename:"));
        gtk_box_pack_start(GTK_BOX(filename_hbox), label, FALSE, TRUE, 0);
        filename_entry = gtk_entry_new();
        gtk_editable_set_editable(GTK_EDITABLE(filename_entry), FALSE);
        gtk_box_pack_start(GTK_BOX(filename_hbox), filename_entry, TRUE, TRUE, 0);

        hbox = gtk_hbox_new(FALSE, 10);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

        left_vbox = gtk_vbox_new(FALSE, 10);
        gtk_box_pack_start(GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);

        tag_frame = gtk_frame_new(_("Ape2 Tag"));
        gtk_box_pack_start(GTK_BOX(left_vbox), tag_frame, FALSE, FALSE, 0);

        table = gtk_table_new(5, 5, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table), 5);
        gtk_container_add(GTK_CONTAINER(tag_frame), table);

        label = gtk_label_new(_("Title:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        title_entry = gtk_entry_new();
        gtk_table_attach(GTK_TABLE(table), title_entry, 1, 4, 0, 1, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);

        label = gtk_label_new(_("Artist:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        performer_entry = gtk_entry_new();
        gtk_table_attach(GTK_TABLE(table), performer_entry, 1, 4, 1, 2, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);

        label = gtk_label_new(_("Album:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 5);

        album_entry = gtk_entry_new();
        gtk_table_attach(GTK_TABLE(table), album_entry, 1, 4, 2, 3, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);

        label = gtk_label_new(_("Comment:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 5);

        user_comment_entry = gtk_entry_new();
        gtk_table_attach(GTK_TABLE(table), user_comment_entry, 1, 4, 3, 4, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);

        label = gtk_label_new(_("Year:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 5);

        date_entry = gtk_entry_new();
        gtk_widget_set_size_request(date_entry, 60, -1);
        gtk_table_attach(GTK_TABLE(table), date_entry, 1, 2, 4, 5, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);

        label = gtk_label_new(_("Track number:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, 4, 5, GTK_FILL, GTK_FILL, 5, 5);

        tracknumber_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(tracknumber_entry), 4);
        gtk_widget_set_size_request(tracknumber_entry, 20, -1);
        gtk_table_attach(GTK_TABLE(table), tracknumber_entry, 3, 4, 4, 5, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);

        label = gtk_label_new(_("Genre:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 5);

        genre_entry = gtk_entry_new();
        gtk_widget_set_size_request(genre_entry, 20, -1);
        gtk_table_attach(GTK_TABLE(table), genre_entry, 1, 4, 5, 6, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), (GtkAttachOptions) (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 5);

        bbox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing(GTK_BOX(bbox), 5);
        gtk_box_pack_start(GTK_BOX(left_vbox), bbox, FALSE, FALSE, 0);

        save_button = gtk_button_new_with_label(_("Save"));
        g_signal_connect(G_OBJECT(save_button), "clicked", G_CALLBACK(tag_save_cb), filename);
        GTK_WIDGET_SET_FLAGS(save_button, GTK_CAN_DEFAULT);
        gtk_box_pack_start(GTK_BOX(bbox), save_button, TRUE, TRUE, 0);

        remove_button = gtk_button_new_with_label(_("Remove Tag"));
        g_signal_connect(G_OBJECT(remove_button), "clicked", G_CALLBACK(tag_remove_cb), filename);
        GTK_WIDGET_SET_FLAGS(remove_button, GTK_CAN_DEFAULT);
        gtk_box_pack_start(GTK_BOX(bbox), remove_button, TRUE, TRUE, 0);

        cancel_button = gtk_button_new_with_label(_("Cancel"));
        g_signal_connect(G_OBJECT(cancel_button), "clicked", G_CALLBACK(fileinfo_close_window), filename);
        GTK_WIDGET_SET_FLAGS(cancel_button, GTK_CAN_DEFAULT);
        gtk_box_pack_start(GTK_BOX(bbox), cancel_button, TRUE, TRUE, 0);
        gtk_widget_grab_default(cancel_button);

        info_frame = gtk_frame_new(_("Wavpack Info:"));
        gtk_box_pack_start(GTK_BOX(hbox), info_frame, FALSE, FALSE, 0);

        info_box = gtk_vbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(info_frame), info_box);
        gtk_container_set_border_width(GTK_CONTAINER(info_box), 10);
        gtk_box_set_spacing(GTK_BOX(info_box), 0);

        version_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(version_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(version_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), version_label, FALSE, FALSE, 0);

        bits_per_sample_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(bits_per_sample_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(bits_per_sample_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), bits_per_sample_label, FALSE, FALSE, 0);

        bitrate_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(bitrate_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(bitrate_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), bitrate_label, FALSE, FALSE, 0);

        rate_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(rate_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(rate_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), rate_label, FALSE, FALSE, 0);

        channel_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(channel_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(channel_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), channel_label, FALSE, FALSE, 0);

        length_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(length_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(length_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), length_label, FALSE, FALSE, 0);

        filesize_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(filesize_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(filesize_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), filesize_label, FALSE, FALSE, 0);

        peakTitle_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(peakTitle_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(peakTitle_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), peakTitle_label, FALSE, FALSE, 0);

        peakAlbum_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(peakAlbum_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(peakAlbum_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), peakAlbum_label, FALSE, FALSE, 0);

        gainTitle_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(gainTitle_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(gainTitle_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), gainTitle_label, FALSE, FALSE, 0);

        gainAlbum_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(gainAlbum_label), 0, 0);
        gtk_label_set_justify(GTK_LABEL(gainAlbum_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(info_box), gainAlbum_label, FALSE, FALSE, 0);

        gtk_widget_show_all(window);
    }
    else
        gdk_window_raise(window->window);

    gtk_widget_set_sensitive(tag_frame, TRUE);

    gtk_label_set_text(GTK_LABEL(version_label), "");
    gtk_label_set_text(GTK_LABEL(bits_per_sample_label), "");
    gtk_label_set_text(GTK_LABEL(bitrate_label), "");
    gtk_label_set_text(GTK_LABEL(rate_label), "");
    gtk_label_set_text(GTK_LABEL(channel_label), "");
    gtk_label_set_text(GTK_LABEL(length_label), "");
    gtk_label_set_text(GTK_LABEL(filesize_label), "");
    gtk_label_set_text(GTK_LABEL(peakTitle_label), "");
    gtk_label_set_text(GTK_LABEL(peakAlbum_label), "");
    gtk_label_set_text(GTK_LABEL(gainTitle_label), "");
    gtk_label_set_text(GTK_LABEL(gainAlbum_label), "");

    time = WavpackGetNumSamples(ctx) / WavpackGetSampleRate(ctx);
    minutes = time / 60;
    seconds = time % 60;

    label_set_text(version_label, _("version %d"), WavpackGetVersion(ctx));
    label_set_text(bitrate_label, _("average bitrate: %6.1f kbps"), WavpackGetAverageBitrate(ctx, 0) / 1000);
    label_set_text(rate_label, _("samplerate: %d Hz"), WavpackGetSampleRate(ctx));
    label_set_text(bits_per_sample_label, _("bits per sample: %d"), WavpackGetBitsPerSample(ctx));
    label_set_text(channel_label, _("channels: %d"), WavpackGetNumChannels(ctx));
    label_set_text(length_label, _("length: %d:%.2d"), minutes, seconds);
    label_set_text(filesize_label, _("file size: %d Bytes"), WavpackGetFileSize(ctx));
    /*
       label_set_text(peakTitle_label, _("Title Peak: %5u"), 100);
       label_set_text(peakAlbum_label, _("Album Peak: %5u"), 100);
       label_set_text(gainTitle_label, _("Title Gain: %-+5.2f dB"), 100.0);
       label_set_text(gainAlbum_label, _("Album Gain: %-+5.2f dB"), 100.0);
     */
    label_set_text(peakTitle_label, _("Title Peak: ?"));
    label_set_text(peakAlbum_label, _("Album Peak: ?"));
    label_set_text(gainTitle_label, _("Title Gain: ?"));
    label_set_text(gainAlbum_label, _("Album Gain: ?"));

    gtk_entry_set_text(GTK_ENTRY(title_entry), tag.title);
    gtk_entry_set_text(GTK_ENTRY(performer_entry), tag.artist);
    gtk_entry_set_text(GTK_ENTRY(album_entry), tag.album);
    gtk_entry_set_text(GTK_ENTRY(user_comment_entry), tag.comment);
    gtk_entry_set_text(GTK_ENTRY(genre_entry), tag.genre);
    gtk_entry_set_text(GTK_ENTRY(tracknumber_entry), tag.track);
    gtk_entry_set_text(GTK_ENTRY(date_entry), tag.year);
    gtk_entry_set_text(GTK_ENTRY(filename_entry), fn);
    gtk_editable_set_position(GTK_EDITABLE(filename_entry), -1);

    tmp = g_strdup_printf(_("File Info - %s"), g_basename(fn));
    gtk_window_set_title(GTK_WINDOW(window), tmp);
    g_free(tmp);

    aud_vfs_fclose(fd);
}

void
wv_read_config(void)
{
    mcs_handle_t *cfg;

    cfg = aud_cfg_db_open();
    aud_cfg_db_get_bool(cfg, "wavpack", "clip_prevention",
                          &wv_cfg.clipPreventionEnabled);
    aud_cfg_db_get_bool(cfg, "wavpack", "album_replaygain",
                          &wv_cfg.albumReplaygainEnabled);
    aud_cfg_db_get_bool(cfg, "wavpack", "dyn_bitrate", &wv_cfg.dynBitrateEnabled);
    aud_cfg_db_get_bool(cfg, "wavpack", "replaygain", &wv_cfg.replaygainEnabled);
    aud_cfg_db_close(cfg);
}

static void
wv_save_config(void)
{
    mcs_handle_t *cfg;

    cfg = aud_cfg_db_open();
    aud_cfg_db_set_bool(cfg, "wavpack", "clip_prevention", wv_cfg.clipPreventionEnabled);
    aud_cfg_db_set_bool(cfg, "wavpack", "album_replaygain", wv_cfg.albumReplaygainEnabled);
    aud_cfg_db_set_bool(cfg, "wavpack", "dyn_bitrate", wv_cfg.dynBitrateEnabled);
    aud_cfg_db_set_bool(cfg, "wavpack", "replaygain", wv_cfg.replaygainEnabled);
    aud_cfg_db_close(cfg);
}

static WavPackConfig config;

static PreferencesWidget plugin_settings_elements[] = {
    {WIDGET_CHK_BTN, N_("Enable Dynamic Bitrate Display"), &config.dynBitrateEnabled, NULL, NULL, FALSE},
};

static PreferencesWidget plugin_settings[] = {
    {WIDGET_BOX, N_("General Plugin Settings"), NULL, NULL, NULL, FALSE,
        {.box = {plugin_settings_elements,
                 G_N_ELEMENTS(plugin_settings_elements),
                 FALSE /* vertical */, TRUE /* frame */}}},
};

static gboolean dummy;

static PreferencesWidget replaygain_type_elements[] = {
    {WIDGET_RADIO_BTN, N_("use Track Gain/Peak"), &dummy /* if it's false, then album replay gain is true and vice versa */, NULL, NULL, FALSE},
    {WIDGET_RADIO_BTN, N_("use Album Gain/Peak"), &config.albumReplaygainEnabled, NULL, NULL, FALSE},
};

static PreferencesWidget replaygain_settings_elements[] = {
    {WIDGET_CHK_BTN, N_("Enable Clipping Prevention"), &config.clipPreventionEnabled, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Enable ReplayGain"), &config.replaygainEnabled, NULL, NULL, FALSE},
    {WIDGET_BOX, N_("ReplayGain Type"), NULL, NULL, NULL, TRUE,
        {.box = {replaygain_type_elements,
                 G_N_ELEMENTS(replaygain_type_elements),
                 FALSE /* vertical */, TRUE /* frame */}}},
};

static PreferencesWidget replaygain_settings[] = {
    {WIDGET_BOX, N_("ReplayGain Settings"), NULL, NULL, NULL, FALSE,
        {.box = {replaygain_settings_elements,
                 G_N_ELEMENTS(replaygain_settings_elements),
                 FALSE /* vertical */, TRUE /* frame */}}},
};

static NotebookTab preferences_tabs[] = {
    {N_("Plugin"), plugin_settings, G_N_ELEMENTS(plugin_settings)},
    {N_("ReplayGain"), replaygain_settings, G_N_ELEMENTS(replaygain_settings)},
};

static PreferencesWidget prefs[] = {
    {WIDGET_NOTEBOOK, NULL, NULL, NULL, NULL, FALSE,
        {.notebook = {preferences_tabs, G_N_ELEMENTS(preferences_tabs)}}},
};

static void
configure_apply()
{
    memcpy(&wv_cfg, &config, sizeof(config));
    wv_save_config();
}

static void
configure_init(void)
{
    wv_read_config();
    memcpy(&config, &wv_cfg, sizeof(config));
    dummy = !config.albumReplaygainEnabled;
}

PluginPreferences preferences = {
    .title = N_("Wavpack Configuration"),
    .prefs = prefs,
    .n_prefs = G_N_ELEMENTS(prefs),
    .type = PREFERENCES_WINDOW,
    .init = configure_init,
    .apply = configure_apply,
};
