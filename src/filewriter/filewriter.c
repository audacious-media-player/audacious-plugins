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

#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/preferences.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

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

static gint fileext;
static const gchar *fileext_str[FILEEXT_MAX] =
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

static GtkWidget *use_suffix_toggle = NULL;
static gboolean use_suffix;

static GtkWidget *prependnumber_toggle;
static gboolean prependnumber;

static gchar *file_path;

VFSFile *output_file = NULL;
Tuple *tuple = NULL;

static gint64 samples_written;

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

static gint file_write_output (void * data, gint length)
{
    return vfs_fwrite (data, 1, length, output_file);
}

static const gchar * const filewriter_defaults[] = {
 "fileext", "0", /* WAV */
 "filenamefromtags", "TRUE",
 "prependnumber", "FALSE",
 "save_original", "TRUE",
 "use_suffix", "FALSE",
 NULL};

static gboolean file_init (void)
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
        str_unref (file_path);
        file_path = filename_to_uri (g_get_home_dir ());
        g_return_val_if_fail (file_path != NULL, FALSE);
    }

    set_plugin();
    if (plugin->init)
        plugin->init(&file_write_output);

    return TRUE;
}

static void file_cleanup (void)
{
    str_unref (file_path);
    file_path = NULL;
}

static VFSFile * safe_create (const gchar * filename)
{
    if (! vfs_file_test (filename, G_FILE_TEST_EXISTS))
        return vfs_fopen (filename, "w");

    const gchar * extension = strrchr (filename, '.');
    gint length = strlen (filename);
    gchar scratch[length + 3];
    gint count;

    for (count = 1; count < 100; count ++)
    {
        if (extension == NULL)
            sprintf (scratch, "%s-%d", filename, count);
        else
            sprintf (scratch, "%.*s-%d%s", (gint) (extension - filename),
             filename, count, extension);

        if (! vfs_file_test (scratch, G_FILE_TEST_EXISTS))
            return vfs_fopen (scratch, "w");
    }

    return NULL;
}

static gint file_open(gint fmt, gint rate, gint nch)
{
    gchar *filename = NULL, *temp = NULL;
    gchar * directory;
    gint pos;
    gint rv;
    gint playlist;

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    playlist = aud_playlist_get_playing ();
    if (playlist < 0)
        return 0;

    pos = aud_playlist_get_position(playlist);
    tuple = aud_playlist_entry_get_tuple (playlist, pos, FALSE);
    if (tuple == NULL)
        return 0;

    if (filenamefromtags)
    {
        gchar * utf8 = aud_playlist_entry_get_title (playlist, pos, FALSE);

        gchar buf[3 * strlen (utf8) + 1];
        str_encode_percent (utf8, -1, buf);
        str_replace_char (buf, '/', '-');
        filename = g_strdup (buf);

        str_unref (utf8);
    }
    else
    {
        temp = aud_playlist_entry_get_filename (playlist, pos);
        gchar * original = strrchr (temp, '/');
        g_return_val_if_fail (original != NULL, 0);
        filename = g_strdup (original + 1);
        str_unref (temp);

        if (!use_suffix)
            if ((temp = strrchr(filename, '.')) != NULL)
                *temp = '\0';
    }

    if (prependnumber)
    {
        gint number = tuple_get_int(tuple, FIELD_TRACK_NUMBER);
        if (number < 1)
            number = pos + 1;

        temp = g_strdup_printf ("%d%%20%s", number, filename);
        g_free(filename);
        filename = temp;
    }

    if (save_original)
    {
        temp = aud_playlist_entry_get_filename (playlist, pos);
        directory = g_strdup (temp);
        str_unref (temp);
        temp = strrchr (directory, '/');
        g_return_val_if_fail (temp != NULL, 0);
        temp[1] = 0;
    }
    else
    {
        g_return_val_if_fail (file_path[0], 0);
        if (file_path[strlen (file_path) - 1] == '/')
            directory = g_strdup (file_path);
        else
            directory = g_strdup_printf ("%s/", file_path);
    }

    temp = g_strdup_printf ("%s%s.%s", directory, filename, fileext_str[fileext]);
    g_free (directory);
    g_free (filename);
    filename = temp;

    output_file = safe_create (filename);
    g_free (filename);

    if (output_file == NULL)
        return 0;

    convert_init (fmt, plugin->format_required (fmt), nch);

    rv = (plugin->open)();

    samples_written = 0;

    return rv;
}

static void file_write(void *ptr, gint length)
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

    if (output_file != NULL)
        vfs_fclose(output_file);
    output_file = NULL;

    if (tuple)
    {
        tuple_unref (tuple);
        tuple = NULL;
    }
}

static void file_flush(gint time)
{
    samples_written = time * (gint64) input.channels * input.frequency / 1000;
}

static void file_pause (gboolean p)
{
}

static gint file_get_time (void)
{
    return samples_written * 1000 / (input.channels * input.frequency);
}

static void configure_response_cb (void)
{
    fileext = gtk_combo_box_get_active(GTK_COMBO_BOX(fileext_combo));

    char * temp = gtk_file_chooser_get_uri ((GtkFileChooser *) path_dirbrowser);
    str_unref (file_path);
    file_path = str_get (temp);
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

static void fileext_cb(GtkWidget *combo, gpointer data)
{
    fileext = gtk_combo_box_get_active(GTK_COMBO_BOX(fileext_combo));
    set_plugin();
    if (plugin->init)
        plugin->init(&file_write_output);

    gtk_widget_set_sensitive(plugin_button, plugin->configure != NULL);
}

static void plugin_configure_cb(GtkWidget *button, gpointer data)
{
    if (plugin->configure)
        plugin->configure();
}


static void saveplace_original_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, FALSE);
        save_original = TRUE;
    }
}

static void saveplace_custom_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, TRUE);
        save_original = FALSE;
    }
}

static void filenamefromtags_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), FALSE);
        use_suffix = FALSE;
        filenamefromtags = TRUE;
    }
}

static void filenamefromfilename_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, TRUE);
        filenamefromtags = FALSE;
    }
}

static void * file_configure (void)
{
        GtkWidget * configure_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

        GtkWidget * fileext_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
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
        gtk_widget_set_sensitive(plugin_button, plugin->configure != NULL);
        gtk_box_pack_end(GTK_BOX(fileext_hbox), plugin_button, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

        GtkWidget * saveplace_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), saveplace_hbox);

        GtkWidget * saveplace1 = gtk_radio_button_new_with_label (NULL,
         _("Save into original directory"));
        gtk_box_pack_start ((GtkBox *) saveplace_hbox, saveplace1, FALSE, FALSE, 0);

        GtkWidget * saveplace2 = gtk_radio_button_new_with_label_from_widget
         ((GtkRadioButton *) saveplace1, _("Save into custom directory"));

        if (!save_original)
            gtk_toggle_button_set_active ((GtkToggleButton *) saveplace2, TRUE);

        gtk_box_pack_start ((GtkBox *) saveplace_hbox, saveplace2, FALSE, FALSE, 0);

        path_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
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

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

        filenamefrom_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), filenamefrom_hbox);

        filenamefrom_label = gtk_label_new(_("Get filename from:"));
        gtk_box_pack_start(GTK_BOX(filenamefrom_hbox), filenamefrom_label, FALSE, FALSE, 0);

        GtkWidget * filenamefrom_toggle1 = gtk_radio_button_new_with_label
         (NULL, _("original file tags"));
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

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

        prependnumber_toggle = gtk_check_button_new_with_label(_("Prepend track number to filename"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prependnumber_toggle), prependnumber);
        gtk_box_pack_start(GTK_BOX(configure_vbox), prependnumber_toggle, FALSE, FALSE, 0);

        g_signal_connect (fileext_combo, "changed", (GCallback) fileext_cb, NULL);
        g_signal_connect (plugin_button, "clicked", (GCallback) plugin_configure_cb, NULL);
        g_signal_connect (saveplace1, "toggled", (GCallback) saveplace_original_cb, NULL);
        g_signal_connect (saveplace2, "toggled", (GCallback) saveplace_custom_cb, NULL);
        g_signal_connect (filenamefrom_toggle1, "toggled", (GCallback) filenamefromtags_cb, NULL);
        g_signal_connect (filenamefrom_toggle2, "toggled",
         (GCallback) filenamefromfilename_cb, NULL);

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
 {WIDGET_CUSTOM, .data = {.populate = file_configure}}};

static const PluginPreferences file_prefs = {
 .widgets = file_widgets,
 .n_widgets = ARRAY_LEN (file_widgets),
 .apply = configure_response_cb};

AUD_OUTPUT_PLUGIN
(
    .name = N_("FileWriter Plugin"),
    .domain = PACKAGE,
    .about_text = file_about,
    .init = file_init,
    .cleanup = file_cleanup,
    .prefs = & file_prefs,
    .probe_priority = 0,
    .open_audio = file_open,
    .close_audio = file_close,
    .write_audio = file_write,
    .drain = file_drain,
    .output_time = file_get_time,
    .pause = file_pause,
    .flush = file_flush,
    .force_reopen = TRUE
)
