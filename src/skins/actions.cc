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

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "main.h"
#include "playlist.h"
#include "plugin-window.h"

#include <gtk/gtk.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugins.h>
#include <libaudgui/libaudgui.h>

void action_ab_clear ()
{
    mainwin_show_status_message (_("Repeat points cleared."));
    aud_drct_set_ab_repeat (-1, -1);
}

void action_ab_set ()
{
    if (aud_drct_get_length () > 0)
    {
        int a, b;
        aud_drct_get_ab_repeat (a, b);

        if (a < 0 || b >= 0)
        {
            a = aud_drct_get_time ();
            b = -1;
            mainwin_show_status_message (_("Repeat point A set."));
        }
        else
        {
            b = aud_drct_get_time ();
            mainwin_show_status_message (_("Repeat point B set."));
        }

        aud_drct_set_ab_repeat (a, b);
    }
}

void action_play_file ()
    { audgui_run_filebrowser (true); }
void action_play_location ()
    { audgui_show_add_url_window (true); }

void action_playlist_manager ()
{
    PluginHandle * manager = aud_plugin_lookup_basename ("playlist-manager");
    if (manager)
    {
        aud_plugin_enable (manager, true);
        focus_plugin_window (manager);
    }
}

void action_search_tool ()
{
    PluginHandle * search = aud_plugin_lookup_basename ("search-tool");
    if (search)
    {
        aud_plugin_enable (search, true);
        focus_plugin_window (search);
    }
}

void action_playlist_refresh_list ()
    { aud_playlist_rescan (active_playlist); }
void action_playlist_play ()
    { aud_playlist_play (active_playlist); }

void action_playlist_new ()
{
    int playlist = aud_playlist_count ();
    aud_playlist_insert (playlist);
    aud_playlist_set_active (playlist);
}

void action_playlist_prev ()
{
    if (active_playlist > 0)
        aud_playlist_set_active (active_playlist - 1);
    else
    {
        int count = aud_playlist_count ();
        if (count > 1)
            aud_playlist_set_active (count - 1);
    }
}

void action_playlist_next ()
{
    int count = aud_playlist_count ();
    if (active_playlist + 1 < count)
        aud_playlist_set_active (active_playlist + 1);
    else if (count > 1)
        aud_playlist_set_active (0);
}

void action_playlist_rename ()
    { audgui_show_playlist_rename (active_playlist); }
void action_playlist_delete ()
    { audgui_confirm_playlist_delete (active_playlist); }

void action_playlist_invert_selection ()
{
    int entries = aud_playlist_entry_count (active_playlist);
    for (int entry = 0; entry < entries; entry ++)
        aud_playlist_entry_set_selected (active_playlist, entry,
         ! aud_playlist_entry_get_selected (active_playlist, entry));
}

void action_playlist_select_all ()
    { aud_playlist_select_all (active_playlist, true); }
void action_playlist_select_none ()
    { aud_playlist_select_all (active_playlist, false); }

void action_playlist_clear_queue ()
    { aud_playlist_queue_delete (active_playlist, 0, aud_playlist_queue_count (active_playlist)); }
void action_playlist_remove_unavailable ()
    { aud_playlist_remove_failed (active_playlist); }
void action_playlist_remove_dupes_by_title ()
    { aud_playlist_remove_duplicates_by_scheme (active_playlist, Playlist::Title); }
void action_playlist_remove_dupes_by_filename ()
    { aud_playlist_remove_duplicates_by_scheme (active_playlist, Playlist::Filename); }
void action_playlist_remove_dupes_by_full_path ()
    { aud_playlist_remove_duplicates_by_scheme (active_playlist, Playlist::Path); }
void action_playlist_remove_all ()
    { aud_playlist_entry_delete (active_playlist, 0, aud_playlist_entry_count (active_playlist)); }
void action_playlist_remove_selected ()
    { aud_playlist_delete_selected (active_playlist); }

void action_playlist_remove_unselected ()
{
    action_playlist_invert_selection ();
    action_playlist_remove_selected ();
    action_playlist_select_all ();
}

void action_playlist_copy ()
{
    GtkClipboard * clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    Index<char> list = audgui_urilist_create_from_selected (active_playlist);

    if (list.len ())
        gtk_clipboard_set_text (clip, list.begin (), list.len ());
}

void action_playlist_cut ()
{
    action_playlist_copy ();
    action_playlist_remove_selected ();
}

void action_playlist_paste ()
{
    GtkClipboard * clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    char * list = gtk_clipboard_wait_for_text (clip);

    if (list)
    {
        audgui_urilist_insert (active_playlist, aud_playlist_get_focus (active_playlist), list);
        g_free (list);
    }
}

void action_playlist_add_url ()
    { audgui_show_add_url_window (false); }
void action_playlist_add_files ()
    { audgui_run_filebrowser (false); }

void action_playlist_randomize_list ()
    { aud_playlist_randomize (active_playlist); }
void action_playlist_reverse_list ()
    { aud_playlist_reverse (active_playlist); }

void action_playlist_sort_by_title ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Title); }
void action_playlist_sort_by_album ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Album); }
void action_playlist_sort_by_artist ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Artist); }
void action_playlist_sort_by_album_artist ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::AlbumArtist); }
void action_playlist_sort_by_length ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Length); }
void action_playlist_sort_by_genre ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Genre); }
void action_playlist_sort_by_filename ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Filename); }
void action_playlist_sort_by_full_path ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Path); }
void action_playlist_sort_by_date ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Date); }
void action_playlist_sort_by_track_number ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::Track); }
void action_playlist_sort_by_custom_title ()
    { aud_playlist_sort_by_scheme (active_playlist, Playlist::FormattedTitle); }

void action_playlist_sort_selected_by_title ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Title); }
void action_playlist_sort_selected_by_album ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Album); }
void action_playlist_sort_selected_by_artist ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Artist); }
void action_playlist_sort_selected_by_album_artist ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::AlbumArtist); }
void action_playlist_sort_selected_by_length ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Length); }
void action_playlist_sort_selected_by_genre ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Genre); }
void action_playlist_sort_selected_by_filename ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Filename); }
void action_playlist_sort_selected_by_full_path ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Path); }
void action_playlist_sort_selected_by_date ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Date); }
void action_playlist_sort_selected_by_track_number ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::Track); }
void action_playlist_sort_selected_by_custom_title ()
    { aud_playlist_sort_selected_by_scheme (active_playlist, Playlist::FormattedTitle); }

void action_playlist_track_info ()
    { audgui_infowin_show (active_playlist, aud_playlist_get_focus (active_playlist)); }

void action_queue_toggle ()
{
    int focus = aud_playlist_get_focus (active_playlist);
    if (focus == -1)
        return;

    /* make sure focused row is selected */
    if (! aud_playlist_entry_get_selected (active_playlist, focus))
    {
        aud_playlist_select_all (active_playlist, false);
        aud_playlist_entry_set_selected (active_playlist, focus, true);
    }

    int at = aud_playlist_queue_find_entry (active_playlist, focus);
    if (at == -1)
        aud_playlist_queue_insert_selected (active_playlist, -1);
    else
        aud_playlist_queue_delete_selected (active_playlist);
}
