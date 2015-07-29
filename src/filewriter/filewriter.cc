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

#include <string.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
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

    void set_info (const char * filename, const Tuple & tuple);
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

static String in_filename;
static Tuple in_tuple;

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

static VFSFile output_file;

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

const char * const FileWriter::defaults[] = {
#ifdef FILEWRITER_MP3
 "fileext", aud::numeric_string<MP3>::str,
#else
 "fileext", aud::numeric_string<WAV>::str,
#endif
 "filenamefromtags", "TRUE",
 "prependnumber", "FALSE",
 "save_original", "FALSE",
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
        plugin->init();

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

void FileWriter::set_info (const char * filename, const Tuple & tuple)
{
    in_filename = String (filename);
    in_tuple = tuple.ref ();
}

bool FileWriter::open_audio (int fmt, int rate, int nch)
{
    String filename, directory;

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    if (filenamefromtags)
    {
        String title = in_tuple.get_str (Tuple::FormattedTitle);

        /* truncate title at 200 characters to avoid hitting filesystem limits */
        StringBuf buf = str_copy (title, aud::min ((int) strlen (title), 200));

        /* replace non-portable characters */
        const char * reserved = "<>:\"/\\|?*";
        for (char * c = buf; * c; c ++)
        {
            if (strchr (reserved, * c))
                * c = ' ';
        }

        /* URI-encode */
        filename = String (str_encode_percent (buf));
    }
    else
    {
        const char * original = strrchr (in_filename, '/');
        g_return_val_if_fail (original != nullptr, 0);
        filename = String (original + 1);

        if (! use_suffix)
        {
            const char * temp;
            if ((temp = strrchr (filename, '.')))
                filename = String (str_copy (filename, temp - filename));
        }
    }

    if (prependnumber)
    {
        int number = in_tuple.get_int (Tuple::Track);
        if (number >= 0)
            filename = String (str_printf ("%d%%20%s", number, (const char *) filename));
    }

    if (save_original)
    {
        const char * temp = strrchr (in_filename, '/');
        g_return_val_if_fail (temp != nullptr, 0);
        directory = String (str_copy (in_filename, temp + 1 - in_filename));
    }
    else
    {
        g_return_val_if_fail (file_path[0], 0);
        if (file_path[strlen (file_path) - 1] == '/')
            directory = String (file_path);
        else
            directory = String (str_concat ({file_path, "/"}));
    }

    filename = String (str_printf ("%s%s.%s", (const char *) directory,
     (const char *) filename, fileext_str[fileext]));

    output_file = safe_create (filename);
    if (! output_file)
        goto err;

    convert_init (fmt, plugin->format_required (fmt));

    if (plugin->open (output_file, input, in_tuple))
        return true;

err:
    in_filename = String ();
    in_tuple = Tuple ();
    return false;
}

int FileWriter::write_audio (const void * ptr, int length)
{
    auto & buf = convert_process (ptr, length);
    plugin->write (output_file, buf.begin (), buf.len ());

    return length;
}

void FileWriter::close_audio ()
{
    plugin->close (output_file);
    convert_free ();

    output_file = VFSFile ();
    in_filename = String ();
    in_tuple = Tuple ();
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
        plugin->init();

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
        gtk_widget_set_sensitive(path_hbox, false);
        save_original = true;
    }
}

static void saveplace_custom_cb(GtkWidget *button, void * data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, true);
        save_original = false;
    }
}

static void filenamefromtags_cb(GtkWidget *button, void * data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, false);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), false);
        use_suffix = false;
        filenamefromtags = true;
    }
}

static void filenamefromfilename_cb(GtkWidget *button, void * data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, true);
        filenamefromtags = false;
    }
}

static void * file_configure (void)
{
        GtkWidget * configure_vbox = gtk_vbox_new (false, 6);

        GtkWidget * fileext_hbox = gtk_hbox_new (false, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), fileext_hbox, false, false, 0);

        GtkWidget * fileext_label = gtk_label_new (_("Output file format:"));
        gtk_box_pack_start(GTK_BOX(fileext_hbox), fileext_label, false, false, 0);

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
        gtk_box_pack_start(GTK_BOX(fileext_hbox), fileext_combo, false, false, 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(fileext_combo), fileext);

        plugin_button = gtk_button_new_with_label(_("Configure"));
        gtk_widget_set_sensitive(plugin_button, plugin->configure != nullptr);
        gtk_box_pack_end(GTK_BOX(fileext_hbox), plugin_button, false, false, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), false, false, 0);

        GtkWidget * saveplace_hbox = gtk_hbox_new (false, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), saveplace_hbox);

        GtkWidget * saveplace1 = gtk_radio_button_new_with_label (nullptr,
         _("Save into original directory"));
        gtk_box_pack_start ((GtkBox *) saveplace_hbox, saveplace1, false, false, 0);

        GtkWidget * saveplace2 = gtk_radio_button_new_with_label_from_widget
         ((GtkRadioButton *) saveplace1, _("Save into custom directory"));

        if (!save_original)
            gtk_toggle_button_set_active ((GtkToggleButton *) saveplace2, true);

        gtk_box_pack_start ((GtkBox *) saveplace_hbox, saveplace2, false, false, 0);

        path_hbox = gtk_hbox_new (false, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), path_hbox, false, false, 0);

        GtkWidget * path_label = gtk_label_new (_("Output file folder:"));
        gtk_box_pack_start ((GtkBox *) path_hbox, path_label, false, false, 0);

        path_dirbrowser =
            gtk_file_chooser_button_new (_("Pick a folder"),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        gtk_file_chooser_set_uri ((GtkFileChooser *) path_dirbrowser, file_path);
        gtk_box_pack_start(GTK_BOX(path_hbox), path_dirbrowser, true, true, 0);

        if (save_original)
            gtk_widget_set_sensitive(path_hbox, false);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), false, false, 0);

        filenamefrom_hbox = gtk_hbox_new (false, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), filenamefrom_hbox);

        filenamefrom_label = gtk_label_new(_("Generate file name from:"));
        gtk_box_pack_start(GTK_BOX(filenamefrom_hbox), filenamefrom_label, false, false, 0);

        GtkWidget * filenamefrom_toggle1 = gtk_radio_button_new_with_label
         (nullptr, _("Original file tag"));
        gtk_box_pack_start ((GtkBox *) filenamefrom_hbox, filenamefrom_toggle1, false, false, 0);

        GtkWidget * filenamefrom_toggle2 =
         gtk_radio_button_new_with_label_from_widget
         ((GtkRadioButton *) filenamefrom_toggle1, _("Original file name"));
        gtk_box_pack_start ((GtkBox *) filenamefrom_hbox, filenamefrom_toggle2, false, false, 0);

        if (!filenamefromtags)
            gtk_toggle_button_set_active ((GtkToggleButton *) filenamefrom_toggle2, true);

        use_suffix_toggle = gtk_check_button_new_with_label(_("Include original file name extension"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), use_suffix);
        gtk_box_pack_start(GTK_BOX(configure_vbox), use_suffix_toggle, false, false, 0);

        if (filenamefromtags)
            gtk_widget_set_sensitive(use_suffix_toggle, false);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), false, false, 0);

        prependnumber_toggle = gtk_check_button_new_with_label(_("Prepend track number to file name"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prependnumber_toggle), prependnumber);
        gtk_box_pack_start(GTK_BOX(configure_vbox), prependnumber_toggle, false, false, 0);

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
 N_("This program is free software; you can redistribute it and/or modify "
    "it under the terms of the GNU General Public License as published by "
    "the Free Software Foundation; either version 2 of the License, or "
    "(at your option) any later version.\n\n"
    "This program is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License "
    "along with this program; if not, write to the Free Software "
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, "
    "USA.");

const PreferencesWidget FileWriter::widgets[] = {
    WidgetCustomGTK (file_configure)
};

const PluginPreferences FileWriter::prefs = {
    {widgets},
    nullptr,  // init
    configure_response_cb
};
