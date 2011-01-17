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

#ifndef AUDACIOUS_ACTIONS_PLAYLIST_H
#define AUDACIOUS_ACTIONS_PLAYLIST_H

void action_playlist_load_list(void);
void action_playlist_save_list(void);
void action_playlist_save_default_list(void);
void action_playlist_save_all_playlists(void);
void action_playlist_refresh_list(void);
void action_playlist_refresh_selected(void);
void action_open_list_manager(void);

void action_playlist_prev(void);
void action_playlist_new(void);
void action_playlist_next(void);
void action_playlist_delete(void);

void action_playlist_search_and_select(void);
void action_playlist_invert_selection(void);
void action_playlist_select_all(void);
void action_playlist_select_none(void);

void action_playlist_clear_queue(void);
void action_playlist_remove_unavailable(void);
void action_playlist_remove_dupes_by_title(void);
void action_playlist_remove_dupes_by_filename(void);
void action_playlist_remove_dupes_by_full_path(void);
void action_playlist_remove_all(void);
void action_playlist_remove_selected(void);
void action_playlist_remove_unselected(void);

void action_playlist_copy(void);
void action_playlist_cut(void);
void action_playlist_paste(void);

void action_playlist_add_url(void);
void action_playlist_add_files(void);

void action_playlist_track_info(void);
void action_queue_toggle(void);

void playlist_sort_track (void);
void playlist_sort_title (void);
void playlist_sort_artist (void);
void playlist_sort_album (void);
void playlist_sort_path (void);
void playlist_sort_formatted_title (void);
void playlist_reverse (void);
void playlist_randomize (void);

#endif /* AUDACIOUS_ACTIONS_PLAYLIST_H */
