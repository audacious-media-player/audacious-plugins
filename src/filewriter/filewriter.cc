/*  FileWriter-Plugin
 *  (C) copyright 2007 merging of Disk Writer and Out-Lame by Michael Färber
 *
 *  Original Out-Lame-Plugin:
 *  (C) copyright 2002 Lars Siebold <khandha5@gmx.net>
 *  (C) copyright 2006-2007 porting to audacious by Yoshiki Yazawa <yaz@cc.rim.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <stdlib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include "filewriter.h"
#include "plugins.h"
#include "convert.h"

class FileWriter : public OutputPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("FileWriter Plugin"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr FileWriter () : OutputPlugin (info, 0, true) {}

    bool init ();
    void cleanup ();

    StereoVolume get_volume () { return {0, 0}; }
    void set_volume (StereoVolume v) {}

    bool open_audio (int fmt, int rate, int nch);
    void close_audio ();

    void period_wait () {}
    int write_audio (const void * ptr, int length);
    void drain () {}

    int get_delay ()
        { return 0; }

    void pause (bool pause) {}
    void flush () {}
};

EXPORT FileWriter aud_plugin_instance;

struct format_info input;

static GtkWidget * path_hbox, * path_dirbrowser;
static GtkWidget * fileext_combo, * plugin_button;

enum fileext_t
{
    WAV = 0,
#ifdef FILEWRITER_MP3
    MP3,
#endif
#ifdef FILEWRITER_VORBIS
    VORBIS,
#endif
#ifdef FILEWRITER_FLAC
    FLAC,
#endif
    FILEEXT_MAX
};

static int fileext;
static const char *fileext_str[FILEEXT_MAX] =
{
    "wav",
#ifdef FILEWRITER_MP3
    "mp3",
#endif
#ifdef FILEWRITER_VORBIS
    "ogg",
#endif
#ifdef FILEWRITER_FLAC
    "flac"
#endif
};

static FileWriterImpl *plugin;

static gboolean save_original;

static GtkWidget *filenamefrom_hbox, *filenamefrom_label;
static gboolean filenamefromtags;

static GtkWidget *use_suffix_toggle = nullptr;
static gboolean use_suffix;

static GtkWidget *prependnumber_toggle;
static gboolean prependnumber;

static String file_path;

VFSFile output_file;
Tuple tuple;

FileWriterImpl *plugins[FILEEXT_MAX] = {
    &wav_plugin,
#ifdef FILEWRITER_MP3
    &mp3_plugin,
#endif
#ifdef FILEWRITER_VORBIS
    &vorbis_plugin,
#endif
#ifdef FILEWRITER_FLAC
    &flac_plugin,
#endif
};

static void set_plugin(void)
{
    if (fileext < 0 || fileext >= FILEEXT_MAX)
        fileext = 0;

    plugin = plugins[fileext];
}

static int file_write_output (void * data, int length)
{
    return output_file.fwrite (data, 1, length);
}

const char * const FileWriter::defaults[] = {
 "fileext", "0", /* WAV */
 "filenamefromtags", "TRUE",
 "prependnumber", "FALSE",
 "save_original", "TRUE",
 "use_suffix", "FALSE",
 nullptr};

bool FileWriter::init ()
{
    aud_config_set_defaults ("filewriter", defaults);

    fileext = aud_get_int ("filewriter", "fileext");
    filenamefromtags = aud_get_bool ("filewriter", "filenamefromtags");
    file_path = aud_get_str ("filewriter", "file_path");
    prependnumber = aud_get_bool ("filewriter", "prependnumber");
    save_original = aud_get_bool ("filewriter", "save_original");
    use_suffix = aud_get_bool ("filewriter", "use_suffix");

    if (! file_path[0])
    {
        file_path = String (filename_to_uri (g_get_home_dir ()));
        g_return_val_if_fail (file_path != nullptr, false);
    }

    set_plugin();
    if (plugin->init)
        plugin->init(&file_write_output);

    return true;
}

void FileWriter::cleanup ()
{
    file_path = String ();
}

static VFSFile safe_create (const char * filename)
{
    if (! VFSFile::test_file (filename, VFS_EXISTS))
        return VFSFile (filename, "w");

    const char * extension = strrchr (filename, '.');

    for (int count = 1; count < 100; count ++)
    {
        StringBuf scratch = extension ?
         str_printf ("%.*s-%d%s", (int) (extension - filename), filename, count, extension) :
         str_printf ("%s-%d", filename, count);

        if (! VFSFile::test_file (scratch, VFS_EXISTS))
            return VFSFile (scratch, "w");
    }

    return VFSFile ();
}

bool FileWriter::open_audio (int fmt, int rate, int nch)
{
    char *filename = nullptr, *temp = nullptr;
    char * directory;

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    // As of Audacious 3.6; aud_drct_get_tuple() is not really thread-safe, so
    // calling it from the output plugin could theoretically cause a crash.
    // However, since the likelihood of an actual crash is quite small, and
    // aud_drct_get_tuple() will be made thread-safe in Audacious 3.7, it's
    // okay to leave this code alone for now.
    tuple = aud_drct_get_tuple ();
    if (! tuple)
        return 0;

    if (filenamefromtags)
    {
        String title = tuple.get_str (Tuple::FormattedTitle);
        StringBuf buf = str_encode_percent (title);
        str_replace_char (buf, '/', '-');
        filename = g_strdup (buf);
    }
    else
    {
        String path = aud_drct_get_filename ();

        const char * original = strrchr (path, '/');
        g_return_val_if_fail (original != nullptr, 0);
        filename = g_strdup (original + 1);

        if (!use_suffix)
            if ((temp = strrchr(filename, '.')) != nullptr)
                *temp = '\0';
    }

    if (prependnumber)
    {
        int number = tuple.get_int (Tuple::Track);
        if (number < 0)
            number = aud_drct_get_position () + 1;

        temp = g_strdup_printf ("%d%%20%s", number, filename);
        g_free(filename);
        filename = temp;
    }

    if (save_original)
    {
        directory = g_strdup (aud_drct_get_filename ());
        temp = strrchr (directory, '/');
        g_return_val_if_fail (temp != nullptr, 0);
        temp[1] = 0;
    }
    else
    {
        g_return_val_if_fail (file_path[0], 0);
        if (file_path[strlen (file_path) - 1] == '/')
            directory = g_strdup (file_path);
        else
            directory = g_strdup_printf ("%s/", (const char *) file_path);
    }

    temp = g_strdup_printf ("%s%s.%s", directory, filename, fileext_str[fileext]);
    g_free (directory);
    g_free (filename);
    filename = temp;

    output_file = safe_create (filename);
    g_free (filename);

    if (! output_file)
        return 0;

    convert_init (fmt, plugin->format_required (fmt), nch);

    return plugin->open ();
}

int FileWriter::write_audio (const void * ptr, int length)
{
    int len = convert_process (ptr, length);

    plugin->write(convert_output, len);

    return length;
}

void FileWriter::close_audio ()
{
    plugin->close();
    convert_free();

    output_file = VFSFile ();
    tuple = Tuple ();
}

static void configure_response_cb (void)
{
    fileext = gtk_combo_box_get_active(GTK_COMBO_BOX(fileext_combo));

    char * temp = gtk_file_chooser_get_uri ((GtkFileChooser *) path_dirbrowser);
    file_path = String (temp);
    g_free (temp);

    use_suffix =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_suffix_toggle));

    prependnumber =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prependnumber_toggle));

    aud_set_int ("filewriter", "fileext", fileext);
    aud_set_bool ("filewriter", "filenamefromtags", filenamefromtags);
    aud_set_str ("filewriter", "file_path", file_path);
    aud_set_bool ("filewriter", "prependnumber", prependnumber);
    aud_set_bool ("filewriter", "save_original", save_original);
    aud_set_bool ("filewriter", "use_suffix", use_suffix);
}

static void fileext_cb(GtkWidget *combo, void * data)
{
    fileext = gtk_combo_box_get_active(GTK_COMBO_BOX(fileext_combo));
    set_plugin();
    if (plugin->init)
        plugin->init(&file_write_output);

    gtk_widget_set_sensitive(plugin_button, plugin->configure != nullptr);
}

static void plugin_configure_cb(GtkWidget *button, void * data)
{
    if (plugin->configure)
        plugin->configure();
}


static void saveplace_original_cb(GtkWidget *button, void * data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, FALSE);
        save_original = TRUE;
    }
}

static void saveplace_custom_cb(GtkWidget *button, void * data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, TRUE);
        save_original = FALSE;
    }
}

static void filenamefromtags_cb(GtkWidget *button, void * data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), FALSE);
        use_suffix = FALSE;
        filenamefromtags = TRUE;
    }
}

static void filenamefromfilename_cb(GtkWidget *button, void * data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, TRUE);
        filenamefromtags = FALSE;
    }
}

static void * file_configure (void)
{
        GtkWidget * configure_vbox = gtk_vbox_new (FALSE, 6);

        GtkWidget * fileext_hbox = gtk_hbox_new (FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), fileext_hbox, FALSE, FALSE, 0);

        GtkWidget * fileext_label = gtk_label_new (_("Output file format:"));
        gtk_box_pack_start(GTK_BOX(fileext_hbox), fileext_label, FALSE, FALSE, 0);

        fileext_combo = gtk_combo_box_text_new ();
        gtk_combo_box_text_append_text ((GtkComboBoxText *) fileext_combo, "WAV");
#ifdef FILEWRITER_MP3
        gtk_combo_box_text_append_text ((GtkComboBoxText *) fileext_combo, "MP3");
#endif
#ifdef FILEWRITER_VORBIS
        gtk_combo_box_text_append_text ((GtkComboBoxText *) fileext_combo, "Vorbis");
#endif
#ifdef FILEWRITER_FLAC
        gtk_combo_box_text_append_text ((GtkComboBoxText *) fileext_combo, "FLAC");
#endif
        gtk_box_pack_start(GTK_BOX(fileext_hbox), fileext_combo, FALSE, FALSE, 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(fileext_combo), fileext);

        plugin_button = gtk_button_new_with_label(_("Configure"));
        gtk_widget_set_sensitive(plugin_button, plugin->configure != nullptr);
        gtk_box_pack_end(GTK_BOX(fileext_hbox), plugin_button, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        GtkWidget * saveplace_hbox = gtk_hbox_new (FALSE, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), saveplace_hbox);

        GtkWidget * saveplace1 = gtk_radio_button_new_with_label (nullptr,
         _("Save into original directory"));
        gtk_box_pack_start ((GtkBox *) saveplace_hbox, saveplace1, FALSE, FALSE, 0);

        GtkWidget * saveplace2 = gtk_radio_button_new_with_label_from_widget
         ((GtkRadioButton *) saveplace1, _("Save into custom directory"));

        if (!save_original)
            gtk_toggle_button_set_active ((GtkToggleButton *) saveplace2, TRUE);

        gtk_box_pack_start ((GtkBox *) saveplace_hbox, saveplace2, FALSE, FALSE, 0);

        path_hbox = gtk_hbox_new (FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), path_hbox, FALSE, FALSE, 0);

        GtkWidget * path_label = gtk_label_new (_("Output file folder:"));
        gtk_box_pack_start ((GtkBox *) path_hbox, path_label, FALSE, FALSE, 0);

        path_dirbrowser =
            gtk_file_chooser_button_new (_("Pick a folder"),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        gtk_file_chooser_set_uri ((GtkFileChooser *) path_dirbrowser, file_path);
        gtk_box_pack_start(GTK_BOX(path_hbox), path_dirbrowser, TRUE, TRUE, 0);

        if (save_original)
            gtk_widget_set_sensitive(path_hbox, FALSE);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        filenamefrom_hbox = gtk_hbox_new (FALSE, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), filenamefrom_hbox);

        filenamefrom_label = gtk_label_new(_("Generate file name from:"));
        gtk_box_pack_start(GTK_BOX(filenamefrom_hbox), filenamefrom_label, FALSE, FALSE, 0);

        GtkWidget * filenamefrom_toggle1 = gtk_radio_button_new_with_label
         (nullptr, _("Original file tag"));
        gtk_box_pack_start ((GtkBox *) filenamefrom_hbox, filenamefrom_toggle1, FALSE, FALSE, 0);

        GtkWidget * filenamefrom_toggle2 =
         gtk_radio_button_new_with_label_from_widget
         ((GtkRadioButton *) filenamefrom_toggle1, _("Original file name"));
        gtk_box_pack_start ((GtkBox *) filenamefrom_hbox, filenamefrom_toggle2, FALSE, FALSE, 0);

        if (!filenamefromtags)
            gtk_toggle_button_set_active ((GtkToggleButton *) filenamefrom_toggle2, TRUE);

        use_suffix_toggle = gtk_check_button_new_with_label(_("Include original file name extension"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), use_suffix);
        gtk_box_pack_start(GTK_BOX(configure_vbox), use_suffix_toggle, FALSE, FALSE, 0);

        if (filenamefromtags)
            gtk_widget_set_sensitive(use_suffix_toggle, FALSE);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        prependnumber_toggle = gtk_check_button_new_with_label(_("Prepend track number to file name"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prependnumber_toggle), prependnumber);
        gtk_box_pack_start(GTK_BOX(configure_vbox), prependnumber_toggle, FALSE, FALSE, 0);

        g_signal_connect (fileext_combo, "changed", (GCallback) fileext_cb, nullptr);
        g_signal_connect (plugin_button, "clicked", (GCallback) plugin_configure_cb, nullptr);
        g_signal_connect (saveplace1, "toggled", (GCallback) saveplace_original_cb, nullptr);
        g_signal_connect (saveplace2, "toggled", (GCallback) saveplace_custom_cb, nullptr);
        g_signal_connect (filenamefrom_toggle1, "toggled", (GCallback) filenamefromtags_cb, nullptr);
        g_signal_connect (filenamefrom_toggle2, "toggled",
         (GCallback) filenamefromfilename_cb, nullptr);

        return configure_vbox;
}

const char FileWriter::about[] =
 N_("This program is free software; you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation; either version 2 of the License, or\n"
    "(at your option) any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n"
    "\n"
    "You should have received a copy of the GNU General Public License\n"
    "along with this program; if not, write to the Free Software\n"
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,\n"
    "USA.");

const PreferencesWidget FileWriter::widgets[] = {
    WidgetCustomGTK (file_configure)
};

const PluginPreferences FileWriter::prefs = {
    {widgets},
    nullptr,  // init
    configure_response_cb
};
