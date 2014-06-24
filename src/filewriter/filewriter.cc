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

#include <libaudcore/runtime.h>
#include <libaudcore/playlist.h>
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>

#include "filewriter.h"
#include "plugins.h"
#include "convert.h"

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

static FileWriter *plugin;

static gboolean save_original;

static GtkWidget *filenamefrom_hbox, *filenamefrom_label;
static gboolean filenamefromtags;

static GtkWidget *use_suffix_toggle = nullptr;
static gboolean use_suffix;

static GtkWidget *prependnumber_toggle;
static gboolean prependnumber;

static String file_path;

VFSFile *output_file = nullptr;
Tuple tuple;

static int64_t samples_written;

FileWriter *plugins[FILEEXT_MAX] = {
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
    return vfs_fwrite (data, 1, length, output_file);
}

static const char * const filewriter_defaults[] = {
 "fileext", "0", /* WAV */
 "filenamefromtags", "TRUE",
 "prependnumber", "FALSE",
 "save_original", "TRUE",
 "use_suffix", "FALSE",
 nullptr};

static bool file_init (void)
{
    aud_config_set_defaults ("filewriter", filewriter_defaults);

    fileext = aud_get_int ("filewriter", "fileext");
    filenamefromtags = aud_get_bool ("filewriter", "filenamefromtags");
    file_path = aud_get_str ("filewriter", "file_path");
    prependnumber = aud_get_bool ("filewriter", "prependnumber");
    save_original = aud_get_bool ("filewriter", "save_original");
    use_suffix = aud_get_bool ("filewriter", "use_suffix");

    if (! file_path[0])
    {
        file_path = String (filename_to_uri (g_get_home_dir ()));
        g_return_val_if_fail (file_path != nullptr, FALSE);
    }

    set_plugin();
    if (plugin->init)
        plugin->init(&file_write_output);

    return TRUE;
}

static void file_cleanup (void)
{
    file_path = String ();
}

static VFSFile * safe_create (const char * filename)
{
    if (! vfs_file_test (filename, G_FILE_TEST_EXISTS))
        return vfs_fopen (filename, "w");

    const char * extension = strrchr (filename, '.');

    for (int count = 1; count < 100; count ++)
    {
        StringBuf scratch = extension ?
         str_printf ("%.*s-%d%s", (int) (extension - filename), filename, count, extension) :
         str_printf ("%s-%d", filename, count);

        if (! vfs_file_test (scratch, G_FILE_TEST_EXISTS))
            return vfs_fopen (scratch, "w");
    }

    return nullptr;
}

static bool file_open(int fmt, int rate, int nch)
{
    char *filename = nullptr, *temp = nullptr;
    char * directory;
    int pos;
    int rv;
    int playlist;

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    playlist = aud_playlist_get_playing ();
    if (playlist < 0)
        return 0;

    pos = aud_playlist_get_position(playlist);
    tuple = aud_playlist_entry_get_tuple (playlist, pos, FALSE);
    if (! tuple)
        return 0;

    if (filenamefromtags)
    {
        String title = aud_playlist_entry_get_title (playlist, pos, FALSE);
        StringBuf buf = str_encode_percent (title);
        str_replace_char (buf, '/', '-');
        filename = g_strdup (buf);
    }
    else
    {
        String path = aud_playlist_entry_get_filename (playlist, pos);

        const char * original = strrchr (path, '/');
        g_return_val_if_fail (original != nullptr, 0);
        filename = g_strdup (original + 1);

        if (!use_suffix)
            if ((temp = strrchr(filename, '.')) != nullptr)
                *temp = '\0';
    }

    if (prependnumber)
    {
        int number = tuple.get_int (FIELD_TRACK_NUMBER);
        if (number < 0)
            number = pos + 1;

        temp = g_strdup_printf ("%d%%20%s", number, filename);
        g_free(filename);
        filename = temp;
    }

    if (save_original)
    {
        directory = g_strdup (aud_playlist_entry_get_filename (playlist, pos));
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

    if (output_file == nullptr)
        return 0;

    convert_init (fmt, plugin->format_required (fmt), nch);

    rv = (plugin->open)();

    samples_written = 0;

    return rv;
}

static void file_write(void *ptr, int length)
{
    int len = convert_process (ptr, length);

    plugin->write(convert_output, len);

    samples_written += length / FMT_SIZEOF (input.format);
}

static void file_drain (void)
{
}

static void file_close(void)
{
    plugin->close();
    convert_free();

    if (output_file != nullptr)
        vfs_fclose(output_file);
    output_file = nullptr;

    tuple = Tuple ();
}

static void file_flush(int time)
{
    samples_written = time * (int64_t) input.channels * input.frequency / 1000;
}

static void file_pause (bool p)
{
}

static int file_get_time (void)
{
    return samples_written * 1000 / (input.channels * input.frequency);
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

        filenamefrom_label = gtk_label_new(_("Get filename from:"));
        gtk_box_pack_start(GTK_BOX(filenamefrom_hbox), filenamefrom_label, FALSE, FALSE, 0);

        GtkWidget * filenamefrom_toggle1 = gtk_radio_button_new_with_label
         (nullptr, _("original file tags"));
        gtk_box_pack_start ((GtkBox *) filenamefrom_hbox, filenamefrom_toggle1, FALSE, FALSE, 0);

        GtkWidget * filenamefrom_toggle2 =
         gtk_radio_button_new_with_label_from_widget
         ((GtkRadioButton *) filenamefrom_toggle1, _("original filename"));
        gtk_box_pack_start ((GtkBox *) filenamefrom_hbox, filenamefrom_toggle2, FALSE, FALSE, 0);

        if (!filenamefromtags)
            gtk_toggle_button_set_active ((GtkToggleButton *) filenamefrom_toggle2, TRUE);

        use_suffix_toggle = gtk_check_button_new_with_label(_("Don't strip file name extension"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), use_suffix);
        gtk_box_pack_start(GTK_BOX(configure_vbox), use_suffix_toggle, FALSE, FALSE, 0);

        if (filenamefromtags)
            gtk_widget_set_sensitive(use_suffix_toggle, FALSE);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        prependnumber_toggle = gtk_check_button_new_with_label(_("Prepend track number to filename"));
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

static const char file_about[] =
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

static const PreferencesWidget file_widgets[] = {
    WidgetCustom (file_configure)
};

static const PluginPreferences file_prefs = {
    file_widgets,
    ARRAY_LEN (file_widgets),
    nullptr,  // init
    configure_response_cb
};

#define AUD_PLUGIN_NAME        N_("FileWriter Plugin")
#define AUD_PLUGIN_ABOUT       file_about
#define AUD_PLUGIN_INIT        file_init
#define AUD_PLUGIN_CLEANUP     file_cleanup
#define AUD_PLUGIN_PREFS       & file_prefs
#define AUD_OUTPUT_PRIORITY    0
#define AUD_OUTPUT_OPEN        file_open
#define AUD_OUTPUT_CLOSE       file_close
#define AUD_OUTPUT_GET_FREE    nullptr
#define AUD_OUTPUT_WAIT_FREE   nullptr
#define AUD_OUTPUT_WRITE       file_write
#define AUD_OUTPUT_DRAIN       file_drain
#define AUD_OUTPUT_GET_TIME    file_get_time
#define AUD_OUTPUT_PAUSE       file_pause
#define AUD_OUTPUT_FLUSH       file_flush
#define AUD_OUTPUT_REOPEN      TRUE

#define AUD_DECLARE_OUTPUT
#include <libaudcore/plugin-declare.h>
