/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2010  Audacious development team
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

#ifndef __PLAYLISTUTIL_H__
#define __PLAYLISTUTIL_H__

void insert_drag_list(gint playlist, gint position, const gchar *list);
gint get_active_selected_pos(void);
gint reselect_selected(void);
void treeview_select_pos(GtkTreeView* tv, gint pos);
gint get_first_selected_pos(GtkTreeView *tv);
gint get_last_selected_pos(GtkTreeView *tv);
void playlist_shift_selected(gint playlist_num, gint old_pos, gint new_pos, gint selected_boundary_range);
GtkTreeView *get_active_playlist_treeview(void);
void playlist_get_changed_range(gint *start, gint *end);
gchar *create_drag_list(gint playlist);

static inline void insert_drag_list_into_active(gint position, const gchar *list)
{
    gint active_playlist_nr = aud_playlist_get_active();
    insert_drag_list(active_playlist_nr, position, list);
}

#endif

