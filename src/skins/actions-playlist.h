/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team.
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

#ifndef SKINS_ACTIONS_PLAYLIST_H
#define SKINS_ACTIONS_PLAYLIST_H

void action_playlist_refresh_list ();

void action_playlist_play ();
void action_playlist_new ();
void action_playlist_prev ();
void action_playlist_next ();
void action_playlist_rename ();
void action_playlist_delete ();

void action_playlist_search_and_select ();
void action_playlist_invert_selection ();
void action_playlist_select_all ();
void action_playlist_select_none ();

void action_playlist_clear_queue ();
void action_playlist_remove_unavailable ();
void action_playlist_remove_dupes_by_title ();
void action_playlist_remove_dupes_by_filename ();
void action_playlist_remove_dupes_by_full_path ();
void action_playlist_remove_all ();
void action_playlist_remove_selected ();
void action_playlist_remove_unselected ();

void action_playlist_copy ();
void action_playlist_cut ();
void action_playlist_paste ();

void action_playlist_add_url ();
void action_playlist_add_files ();

void action_playlist_randomize_list ();
void action_playlist_reverse_list ();

void action_playlist_sort_by_title ();
void action_playlist_sort_by_album ();
void action_playlist_sort_by_artist ();
void action_playlist_sort_by_album_artist ();
void action_playlist_sort_by_length ();
void action_playlist_sort_by_genre ();
void action_playlist_sort_by_filename ();
void action_playlist_sort_by_full_path ();
void action_playlist_sort_by_date ();
void action_playlist_sort_by_track_number ();
void action_playlist_sort_by_custom_title ();

void action_playlist_sort_selected_by_title ();
void action_playlist_sort_selected_by_album ();
void action_playlist_sort_selected_by_artist ();
void action_playlist_sort_selected_by_album_artist ();
void action_playlist_sort_selected_by_length ();
void action_playlist_sort_selected_by_genre ();
void action_playlist_sort_selected_by_filename ();
void action_playlist_sort_selected_by_full_path ();
void action_playlist_sort_selected_by_date ();
void action_playlist_sort_selected_by_track_number ();
void action_playlist_sort_selected_by_custom_title ();

void action_playlist_track_info ();
void action_queue_toggle ();

#endif /* SKINS_ACTIONS_PLAYLIST_H */
