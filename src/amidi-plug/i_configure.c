/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#include "i_configure.h"

#include <stdlib.h>
#include <string.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>

#include "i_common.h"
#include "i_backend.h"
#include "i_configure-ap.h"
#include "i_configure-fluidsynth.h"

/* from amidi-plug.c */
extern amidiplug_sequencer_backend_t * backend;

amidiplug_cfg_ap_t * amidiplug_cfg_ap;
amidiplug_cfg_backend_t * amidiplug_cfg_backend;

static void i_configure_commit (void);

void i_configure_ev_browse_for_entry (GtkWidget * target_entry)
{
    GtkWidget * parent_window = gtk_widget_get_toplevel (target_entry);
    GtkFileChooserAction act = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (target_entry),"fc-act"));

    if (gtk_widget_is_toplevel (parent_window))
    {
        GtkWidget * browse_dialog = gtk_file_chooser_dialog_new (_("AMIDI-Plug - select file"),
                                    GTK_WINDOW (parent_window), act,
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

        if (strcmp (gtk_entry_get_text (GTK_ENTRY (target_entry)), ""))
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (browse_dialog),
                                           gtk_entry_get_text (GTK_ENTRY (target_entry)));

        if (gtk_dialog_run (GTK_DIALOG (browse_dialog)) == GTK_RESPONSE_ACCEPT)
        {
            char * filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (browse_dialog));
            gtk_entry_set_text (GTK_ENTRY (target_entry), filename);
            DEBUGMSG ("selected file: %s\n", filename);
            free (filename);
        }

        gtk_widget_destroy (browse_dialog);
    }
}

static void response_cb (GtkWidget * window, int response)
{
    if (response == GTK_RESPONSE_OK)
    {
        if (aud_drct_get_playing ())
            aud_drct_stop ();

        g_signal_emit_by_name (window, "ap-commit");
        i_configure_commit ();
    }

    gtk_widget_destroy (window);
}

void i_configure_gui (void)
{
    static GtkWidget * configwin = NULL;

    if (configwin != NULL)
    {
        DEBUGMSG ("config window is already open!\n");
        return;
    }

    configwin = gtk_dialog_new_with_buttons (_("AMIDI-Plug Settings"), NULL, 0,
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

    if (! g_signal_lookup ("ap-commit", G_OBJECT_TYPE (configwin)))
        g_signal_new ("ap-commit", G_OBJECT_TYPE (configwin), G_SIGNAL_ACTION, 0,
                      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_signal_connect (configwin, "response", (GCallback) response_cb, NULL);
    g_signal_connect (configwin, "destroy", (GCallback) gtk_widget_destroyed, & configwin);

    GtkWidget * configwin_vbox = gtk_dialog_get_content_area ((GtkDialog *) configwin);

    GtkWidget * configwin_notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (configwin_notebook), GTK_POS_LEFT);
    gtk_box_pack_start (GTK_BOX (configwin_vbox), configwin_notebook, TRUE, TRUE, 2);

    /* AMIDI-PLUG PREFERENCES TAB */
    GtkWidget * ap_pagelabel_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
    GtkWidget * ap_page_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_alignment_set_padding (GTK_ALIGNMENT (ap_page_alignment), 3, 3, 8, 3);
    i_configure_gui_tab_ap (ap_page_alignment, configwin);
    i_configure_gui_tablabel_ap (ap_pagelabel_alignment, configwin);
    gtk_notebook_append_page (GTK_NOTEBOOK (configwin_notebook),
                              ap_page_alignment, ap_pagelabel_alignment);

#if AMIDIPLUG_FLUIDSYNTH
    /* FLUIDSYNTH BACKEND CONFIGURATION TAB */
    GtkWidget * fsyn_pagelabel_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
    GtkWidget * fsyn_page_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_alignment_set_padding (GTK_ALIGNMENT (fsyn_page_alignment), 3, 3, 8, 3);
    i_configure_gui_tab_fsyn (fsyn_page_alignment, configwin);
    i_configure_gui_tablabel_fsyn (fsyn_pagelabel_alignment, configwin);
    gtk_notebook_append_page (GTK_NOTEBOOK (configwin_notebook),
                              fsyn_page_alignment, fsyn_pagelabel_alignment);
#endif

    gtk_widget_show_all (configwin);
}

static void i_configure_commit (void)
{
    DEBUGMSG ("saving configuration...\n");
    i_configure_cfg_ap_save(); /* save amidiplug settings */
    i_configure_cfg_backend_save(); /* save backend settings */
    DEBUGMSG ("configuration saved\n");

    /* stop playback before reloading backend
     * not pretty, but it's better than crashing */
    if (aud_drct_get_playing ())
        aud_drct_stop ();

    i_backend_unload (backend);
    backend = i_backend_load ();

    /* quit if new backend fails to load
     * again, it's better than crashing */
    if (! backend)
        aud_drct_quit ();
}


void i_configure_cfg_backend_free (void)
{
#ifdef AMIDIPLUG_FLUIDSYNTH
    i_configure_cfg_fsyn_free(); /* free fluidsynth backend configuration */
#endif

    free (amidiplug_cfg_backend);
}


void i_configure_cfg_backend_read (void)
{
    amidiplug_cfg_backend = malloc (sizeof (amidiplug_cfg_backend_t));
    memset (amidiplug_cfg_backend, 0, sizeof (amidiplug_cfg_backend_t));

#ifdef AMIDIPLUG_FLUIDSYNTH
    i_configure_cfg_fsyn_read ();
#endif
}


void i_configure_cfg_backend_save (void)
{
#ifdef AMIDIPLUG_FLUIDSYNTH
    i_configure_cfg_fsyn_save (); /* save fluidsynth backend configuration */
#endif
}


/* read only the amidi-plug part of configuration */
void i_configure_cfg_ap_read (void)
{
    static const char * const defaults[] =
    {
        "ap_opts_transpose_value", "0",
        "ap_opts_drumshift_value", "0",
        "ap_opts_length_precalc", "0",
        "ap_opts_lyrics_extract", "0",
        "ap_opts_comments_extract", "0",
        NULL
    };

    aud_config_set_defaults ("amidiplug", defaults);

    amidiplug_cfg_ap = malloc (sizeof (amidiplug_cfg_ap_t));
    amidiplug_cfg_ap->ap_opts_transpose_value = aud_get_int ("amidiplug", "ap_opts_transpose_value");
    amidiplug_cfg_ap->ap_opts_drumshift_value = aud_get_int ("amidiplug", "ap_opts_drumshift_value");
    amidiplug_cfg_ap->ap_opts_length_precalc = aud_get_int ("amidiplug", "ap_opts_length_precalc");
    amidiplug_cfg_ap->ap_opts_lyrics_extract = aud_get_int ("amidiplug", "ap_opts_lyrics_extract");
    amidiplug_cfg_ap->ap_opts_comments_extract = aud_get_int ("amidiplug", "ap_opts_comments_extract");
}


void i_configure_cfg_ap_save (void)
{
    aud_set_int ("amidiplug", "ap_opts_transpose_value", amidiplug_cfg_ap->ap_opts_transpose_value);
    aud_set_int ("amidiplug", "ap_opts_drumshift_value", amidiplug_cfg_ap->ap_opts_drumshift_value);
    aud_set_int ("amidiplug", "ap_opts_length_precalc", amidiplug_cfg_ap->ap_opts_length_precalc);
    aud_set_int ("amidiplug", "ap_opts_lyrics_extract", amidiplug_cfg_ap->ap_opts_lyrics_extract);
    aud_set_int ("amidiplug", "ap_opts_comments_extract", amidiplug_cfg_ap->ap_opts_comments_extract);
}


void i_configure_cfg_ap_free (void)
{
    free (amidiplug_cfg_ap);
    amidiplug_cfg_ap = NULL;
}


char * i_configure_cfg_get_file (void)
{
    return g_build_filename (aud_get_path (AUD_PATH_USER_DIR), "amidi-plug.conf", NULL);
}
