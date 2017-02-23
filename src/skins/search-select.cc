/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include "actions-playlist.h"
#include "playlist.h"
#include "playlist-widget.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

static void search_cbt_cb (GtkWidget * called_cbt, GtkWidget * other_cbt)
{
    if (gtk_toggle_button_get_active ((GtkToggleButton *) called_cbt))
        gtk_toggle_button_set_active ((GtkToggleButton *) other_cbt, false);
}

static gboolean search_kp_cb (GtkWidget * entry, GdkEventKey * event, GtkWidget * dialog)
{
    if (event->keyval != GDK_KEY_Return)
        return false;

    gtk_dialog_response ((GtkDialog *) dialog, GTK_RESPONSE_ACCEPT);
    return true;
}

static void copy_selected_to_new (Playlist playlist)
{
    int entries = playlist.n_entries ();
    Index<PlaylistAddItem> items;

    for (int entry = 0; entry < entries; entry ++)
    {
        if (playlist.entry_selected (entry))
        {
            items.append
             (playlist.entry_filename (entry),
              playlist.entry_tuple (entry, Playlist::NoWait));
        }
    }

    auto new_list = Playlist::new_playlist ();
    new_list.insert_items (0, std::move (items), false);
}

void action_playlist_search_and_select ()
{
    /* create dialog */
    GtkWidget * dialog = gtk_dialog_new_with_buttons (
     _("Search entries in active playlist"), nullptr, (GtkDialogFlags) 0 ,
     _("Cancel"), GTK_RESPONSE_REJECT, _("Search"), GTK_RESPONSE_ACCEPT, nullptr);

    /* help text and logo */
    GtkWidget * hbox = gtk_hbox_new (false, 6);
    GtkWidget * logo = gtk_image_new_from_icon_name ("edit-find", GTK_ICON_SIZE_DIALOG);
    GtkWidget * helptext = gtk_label_new (_("Select entries in playlist by filling one or more "
     "fields. Fields use regular expressions syntax, case-insensitive. If you don't know how "
     "regular expressions work, simply insert a literal portion of what you're searching for."));
    gtk_label_set_line_wrap ((GtkLabel *) helptext, true);
    gtk_box_pack_start ((GtkBox *) hbox, logo, false, false, 0);
    gtk_box_pack_start ((GtkBox *) hbox, helptext, false, false, 0);

    /* title */
    GtkWidget * label_title = gtk_label_new (_("Title:"));
    gtk_misc_set_alignment ((GtkMisc *) label_title, 1, 0.5);
    GtkWidget * entry_title = gtk_entry_new ();
    g_signal_connect (entry_title, "key-press-event", (GCallback) search_kp_cb, dialog);

    /* album */
    GtkWidget * label_album = gtk_label_new (_("Album:"));
    gtk_misc_set_alignment ((GtkMisc *) label_album, 1, 0.5);
    GtkWidget * entry_album = gtk_entry_new ();
    g_signal_connect (entry_album, "key-press-event", (GCallback) search_kp_cb, dialog);

    /* artist */
    GtkWidget * label_performer = gtk_label_new (_("Artist:"));
    gtk_misc_set_alignment ((GtkMisc *) label_performer, 1, 0.5);
    GtkWidget * entry_performer = gtk_entry_new ();
    g_signal_connect (entry_performer, "key-press-event", (GCallback) search_kp_cb, dialog);

    /* file name */
    GtkWidget * label_file_name = gtk_label_new (_("File Name:"));
    gtk_misc_set_alignment ((GtkMisc *) label_file_name, 1, 0.5);
    GtkWidget * entry_file_name = gtk_entry_new ();
    g_signal_connect (entry_file_name, "key-press-event",
     (GCallback) search_kp_cb, dialog);

    /* some options that control behaviour */
    GtkWidget * checkbt_clearprevsel = gtk_check_button_new_with_label (
     _("Clear previous selection before searching"));
    gtk_toggle_button_set_active ((GtkToggleButton *) checkbt_clearprevsel, true);
    GtkWidget * checkbt_autoenqueue = gtk_check_button_new_with_label (
     _("Automatically toggle queue for matching entries"));
    gtk_toggle_button_set_active ((GtkToggleButton *) checkbt_autoenqueue, false);
    GtkWidget * checkbt_newplaylist = gtk_check_button_new_with_label (
     _("Create a new playlist with matching entries"));
    gtk_toggle_button_set_active ((GtkToggleButton *) checkbt_newplaylist, false);

    g_signal_connect (checkbt_autoenqueue, "clicked",
     (GCallback) search_cbt_cb, checkbt_newplaylist);
    g_signal_connect (checkbt_newplaylist, "clicked",
     (GCallback) search_cbt_cb, checkbt_autoenqueue);

    /* place fields in grid */
    GtkTable * grid = (GtkTable *) gtk_table_new (0, 0, false);
    gtk_table_set_row_spacings (grid, 6);
    gtk_table_set_col_spacings (grid, 6);
    gtk_table_attach_defaults (grid, hbox, 0, 2, 0, 1);
    gtk_table_attach (grid, label_title, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (grid, entry_title, 1, 2, 1, 2);
    gtk_table_attach (grid, label_album, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (grid, entry_album, 1, 2, 2, 3);
    gtk_table_attach (grid, label_performer, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (grid, entry_performer, 1, 2, 3, 4);
    gtk_table_attach (grid, label_file_name, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (grid, entry_file_name, 1, 2, 4, 5);
    gtk_table_attach_defaults (grid, checkbt_clearprevsel, 0, 2, 5, 6);
    gtk_table_attach_defaults (grid, checkbt_autoenqueue, 0, 2, 6, 7);
    gtk_table_attach_defaults (grid, checkbt_newplaylist, 0, 2, 7, 8);

    gtk_container_set_border_width ((GtkContainer *) grid, 5);
    gtk_container_add ((GtkContainer *) gtk_dialog_get_content_area
     ((GtkDialog *) dialog), (GtkWidget *) grid);
    gtk_widget_show_all (dialog);

    if (gtk_dialog_run ((GtkDialog *) dialog) == GTK_RESPONSE_ACCEPT)
    {
        /* create a TitleInput tuple with user search data */
        Tuple tuple;
        const char * searchdata = nullptr;
        auto playlist = Playlist::active_playlist ();

        searchdata = gtk_entry_get_text ((GtkEntry *) entry_title);
        AUDDBG ("title=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Title, searchdata);

        searchdata = gtk_entry_get_text ((GtkEntry *) entry_album);
        AUDDBG ("album=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Album, searchdata);

        searchdata = gtk_entry_get_text ((GtkEntry *) entry_performer);
        AUDDBG ("performer=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Artist, searchdata);

        searchdata = gtk_entry_get_text ((GtkEntry *) entry_file_name);
        AUDDBG ("filename=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Basename, searchdata);

        /* check if previous selection should be cleared before searching */
        if (gtk_toggle_button_get_active ((GtkToggleButton *) checkbt_clearprevsel))
            playlist.select_all (false);

        playlist.select_by_patterns (tuple);

        /* check if a new playlist should be created after searching */
        if (gtk_toggle_button_get_active ((GtkToggleButton *) checkbt_newplaylist))
            copy_selected_to_new (playlist);
        else
        {
            /* set focus on the first entry found */
            int entries = playlist.n_entries ();
            for (int i = 0; i < entries; i ++)
            {
                if (playlist.entry_selected (i))
                {
                    playlistwin_list->set_focused (i);
                    break;
                }
            }

            /* check if matched entries should be queued */
            if (gtk_toggle_button_get_active ((GtkToggleButton *) checkbt_autoenqueue))
                playlist.queue_insert_selected (-1);
        }
    }

    /* done here :) */
    gtk_widget_destroy (dialog);
}
