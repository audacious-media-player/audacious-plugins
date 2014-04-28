/*
 * preset-browser.c
 * Copyright 2014 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include "preset-browser.h"

#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/vfs.h>

#include "ui_equalizer.h"

typedef void (* FilebrowserCallback) (const char * filename);

static GtkWidget * preset_browser;

static void browser_response (GtkWidget * dialog, int response, void * data)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        char * filename = gtk_file_chooser_get_uri ((GtkFileChooser *) dialog);
        ((FilebrowserCallback) data) (filename);
        g_free (filename);
    }

    gtk_widget_destroy (dialog);
}

static void show_preset_browser (const char * title, bool_t save,
 const char * default_filename, FilebrowserCallback callback)
{
    if (preset_browser)
        gtk_widget_destroy (preset_browser);

    preset_browser = gtk_file_chooser_dialog_new (title, NULL, save ?
     GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN, _("Cancel"),
     GTK_RESPONSE_CANCEL, save ? _("Save") : _("Load"), GTK_RESPONSE_ACCEPT,
     NULL);

    if (default_filename)
        gtk_file_chooser_set_current_name ((GtkFileChooser *) preset_browser, default_filename);

    g_signal_connect (preset_browser, "response", (GCallback) browser_response, (void *) callback);
    g_signal_connect (preset_browser, "destroy", (GCallback)
     gtk_widget_destroyed, & preset_browser);

    gtk_window_present ((GtkWindow *) preset_browser);
}

static void do_load_file (const char * filename)
{
    EqualizerPreset preset;
    if (! aud_load_preset_file (preset, filename))
        return;

    equalizerwin_apply_preset (preset);
}

void eq_preset_load_file (void)
{
    show_preset_browser (_("Load Preset File"), FALSE, NULL, do_load_file);
}

static void do_load_eqf (const char * filename)
{
    VFSFile * file = vfs_fopen (filename, "r");
    if (! file)
        return;

    Index<EqualizerPreset> presets = aud_import_winamp_presets (file);

    if (presets.len ())
        equalizerwin_apply_preset (presets[0]);

    vfs_fclose (file);
}

void eq_preset_load_eqf (void)
{
    show_preset_browser (_("Load EQF File"), FALSE, NULL, do_load_eqf);
}

static void do_save_file (const char * filename)
{
    EqualizerPreset preset;
    equalizerwin_update_preset (preset);
    aud_save_preset_file (preset, filename);
}

void eq_preset_save_file (void)
{
    char * title = aud_drct_get_title ();
    char * name = title ? str_printf ("%s.%s", title, EQUALIZER_DEFAULT_PRESET_EXT) : NULL;

    show_preset_browser (_("Save Preset File"), TRUE, name, do_save_file);

    str_unref (title);
    str_unref (name);
}

static void do_save_eqf (const char * filename)
{
    VFSFile * file = vfs_fopen (filename, "w");
    if (! file)
        return;

    EqualizerPreset preset ("Preset1");
    equalizerwin_update_preset (preset);
    aud_export_winamp_preset (preset, file);

    vfs_fclose (file);
}

void eq_preset_save_eqf (void)
{
    show_preset_browser (_("Save EQF File"), TRUE, NULL, do_save_eqf);
}

static void do_import_winamp (const char * filename)
{
    VFSFile * file = vfs_fopen (filename, "r");
    if (! file)
        return;

    equalizerwin_import_presets (aud_import_winamp_presets (file));

    vfs_fclose (file);
}

void eq_preset_import_winamp (void)
{
    show_preset_browser (_("Import Winamp Presets"), FALSE, NULL, do_import_winamp);
}

void eq_preset_browser_cleanup (void)
{
    if (preset_browser)
        gtk_widget_destroy (preset_browser);
}
