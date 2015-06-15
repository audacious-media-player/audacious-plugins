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
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(called_cbt)))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(other_cbt), false);
}

static gboolean search_kp_cb (GtkWidget * entry, GdkEventKey * event, GtkWidget * searchdlg_win)
{
    if (event->keyval != GDK_KEY_Return)
        return false;

    gtk_dialog_response(GTK_DIALOG(searchdlg_win), GTK_RESPONSE_ACCEPT);
    return true;
}

static void copy_selected_to_new (int playlist)
{
    int entries = aud_playlist_entry_count (playlist);
    int new_list = aud_playlist_count ();
    Index<PlaylistAddItem> items;

    aud_playlist_insert (new_list);

    for (int entry = 0; entry < entries; entry ++)
    {
        if (aud_playlist_entry_get_selected (playlist, entry))
        {
            items.append
             (aud_playlist_entry_get_filename (playlist, entry),
              aud_playlist_entry_get_tuple (playlist, entry, Playlist::Nothing));
        }
    }

    aud_playlist_entry_insert_batch (new_list, 0, std::move (items), false);
    aud_playlist_set_active (new_list);
}

void action_playlist_search_and_select ()
{
    GtkWidget *searchdlg_win, *searchdlg_grid;
    GtkWidget *searchdlg_hbox, *searchdlg_logo, *searchdlg_helptext;
    GtkWidget *searchdlg_entry_title, *searchdlg_label_title;
    GtkWidget *searchdlg_entry_album, *searchdlg_label_album;
    GtkWidget *searchdlg_entry_file_name, *searchdlg_label_file_name;
    GtkWidget *searchdlg_entry_performer, *searchdlg_label_performer;
    GtkWidget *searchdlg_checkbt_clearprevsel;
    GtkWidget *searchdlg_checkbt_newplaylist;
    GtkWidget *searchdlg_checkbt_autoenqueue;

    /* create dialog */
    searchdlg_win = gtk_dialog_new_with_buttons(
      _("Search entries in active playlist"), nullptr, (GtkDialogFlags) 0 ,
      _("Cancel"), GTK_RESPONSE_REJECT, _("Search"), GTK_RESPONSE_ACCEPT, nullptr);

    /* help text and logo */
    searchdlg_hbox = gtk_hbox_new (false, 6);
    searchdlg_logo = gtk_image_new_from_icon_name ("edit-find", GTK_ICON_SIZE_DIALOG);
    searchdlg_helptext = gtk_label_new (_("Select entries in playlist by filling one or more "
      "fields. Fields use regular expressions syntax, case-insensitive. If you don't know how "
      "regular expressions work, simply insert a literal portion of what you're searching for."));
    gtk_label_set_line_wrap (GTK_LABEL(searchdlg_helptext), true);
    gtk_box_pack_start (GTK_BOX(searchdlg_hbox), searchdlg_logo, false, false, 0);
    gtk_box_pack_start (GTK_BOX(searchdlg_hbox), searchdlg_helptext, false, false, 0);

    /* title */
    searchdlg_label_title = gtk_label_new (_("Title:"));
    gtk_misc_set_alignment ((GtkMisc *) searchdlg_label_title, 1, 0.5);
    searchdlg_entry_title = gtk_entry_new();
    g_signal_connect (searchdlg_entry_title, "key-press-event" ,
      G_CALLBACK(search_kp_cb), searchdlg_win);

    /* album */
    searchdlg_label_album = gtk_label_new (_("Album:"));
    gtk_misc_set_alignment ((GtkMisc *) searchdlg_label_album, 1, 0.5);
    searchdlg_entry_album = gtk_entry_new();
    g_signal_connect (searchdlg_entry_album, "key-press-event" ,
      G_CALLBACK(search_kp_cb), searchdlg_win);

    /* artist */
    searchdlg_label_performer = gtk_label_new (_("Artist:"));
    gtk_misc_set_alignment ((GtkMisc *) searchdlg_label_performer, 1, 0.5);
    searchdlg_entry_performer = gtk_entry_new();
    g_signal_connect (searchdlg_entry_performer, "key-press-event" ,
      G_CALLBACK(search_kp_cb), searchdlg_win);

    /* file name */
    searchdlg_label_file_name = gtk_label_new (_("File Name:"));
    gtk_misc_set_alignment ((GtkMisc *) searchdlg_label_file_name, 1, 0.5);
    searchdlg_entry_file_name = gtk_entry_new();
    g_signal_connect (searchdlg_entry_file_name, "key-press-event" ,
      G_CALLBACK(search_kp_cb), searchdlg_win);

    /* some options that control behaviour */
    searchdlg_checkbt_clearprevsel = gtk_check_button_new_with_label(
      _("Clear previous selection before searching"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(searchdlg_checkbt_clearprevsel), true);
    searchdlg_checkbt_autoenqueue = gtk_check_button_new_with_label(
      _("Automatically toggle queue for matching entries"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(searchdlg_checkbt_autoenqueue), false);
    searchdlg_checkbt_newplaylist = gtk_check_button_new_with_label(
      _("Create a new playlist with matching entries"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(searchdlg_checkbt_newplaylist), false);
    g_signal_connect (searchdlg_checkbt_autoenqueue, "clicked" ,
      G_CALLBACK(search_cbt_cb), searchdlg_checkbt_newplaylist);
    g_signal_connect (searchdlg_checkbt_newplaylist, "clicked" ,
      G_CALLBACK(search_cbt_cb), searchdlg_checkbt_autoenqueue);

    /* place fields in searchdlg_grid */
    searchdlg_grid = gtk_table_new (0, 0, false);
    gtk_table_set_row_spacings (GTK_TABLE (searchdlg_grid), 6);
    gtk_table_set_col_spacings (GTK_TABLE (searchdlg_grid), 6);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_hbox, 0, 2, 0, 1);
    gtk_table_attach (GTK_TABLE (searchdlg_grid), searchdlg_label_title, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_entry_title, 1, 2, 1, 2);
    gtk_table_attach (GTK_TABLE (searchdlg_grid), searchdlg_label_album, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_entry_album, 1, 2, 2, 3);
    gtk_table_attach (GTK_TABLE (searchdlg_grid), searchdlg_label_performer, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_entry_performer, 1, 2, 3, 4);
    gtk_table_attach (GTK_TABLE (searchdlg_grid), searchdlg_label_file_name, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_entry_file_name, 1, 2, 4, 5);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_checkbt_clearprevsel, 0, 2, 5, 6);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_checkbt_autoenqueue, 0, 2, 6, 7);
    gtk_table_attach_defaults (GTK_TABLE (searchdlg_grid), searchdlg_checkbt_newplaylist, 0, 2, 7, 8);

    gtk_container_set_border_width (GTK_CONTAINER(searchdlg_grid), 5);
    gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area
     (GTK_DIALOG(searchdlg_win))), searchdlg_grid);
    gtk_widget_show_all (searchdlg_win);

    if (gtk_dialog_run (GTK_DIALOG(searchdlg_win)) == GTK_RESPONSE_ACCEPT)
    {
        /* create a TitleInput tuple with user search data */
        Tuple tuple;
        char *searchdata = nullptr;

        searchdata = (char*)gtk_entry_get_text (GTK_ENTRY(searchdlg_entry_title));
        AUDDBG("title=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Title, searchdata);

        searchdata = (char*)gtk_entry_get_text (GTK_ENTRY(searchdlg_entry_album));
        AUDDBG("album=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Album, searchdata);

        searchdata = (char*)gtk_entry_get_text (GTK_ENTRY(searchdlg_entry_performer));
        AUDDBG("performer=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Artist, searchdata);

        searchdata = (char*)gtk_entry_get_text (GTK_ENTRY(searchdlg_entry_file_name));
        AUDDBG("filename=\"%s\"\n", searchdata);
        tuple.set_str (Tuple::Basename, searchdata);

        /* check if previous selection should be cleared before searching */
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(searchdlg_checkbt_clearprevsel)))
            aud_playlist_select_all (active_playlist, false);

        aud_playlist_select_by_patterns (active_playlist, tuple);

        /* check if a new playlist should be created after searching */
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(searchdlg_checkbt_newplaylist)))
            copy_selected_to_new (active_playlist);
        else
        {
            /* set focus on the first entry found */
            int entries = aud_playlist_entry_count (active_playlist);
            for (int count = 0; count < entries; count ++)
            {
                if (aud_playlist_entry_get_selected (active_playlist, count))
                {
                    playlistwin_list->set_focused (count);
                    break;
                }
            }

            /* check if matched entries should be queued */
            if (gtk_toggle_button_get_active ((GtkToggleButton *)
             searchdlg_checkbt_autoenqueue))
                aud_playlist_queue_insert_selected (active_playlist, -1);
        }

        playlistwin_update ();
    }

    /* done here :) */
    gtk_widget_destroy (searchdlg_win);
}
