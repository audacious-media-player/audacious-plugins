/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2010  Audacious development team
 *  Copyright (C) 2010 Michał Lipski <tallica@o2.pl>
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
void playlist_shift_selected(gint playlist_num, gint old_pos, gint new_pos, gint selected_boundary_range);
GtkTreeView *playlist_get_treeview_from_page(GtkWidget *page);
GtkTreeView *playlist_get_treeview(gint playlist);
GtkTreeView *playlist_get_active_treeview(void);
GtkTreeView *playlist_get_playing_treeview(void);
gint playlist_get_playlist_from_treeview(GtkTreeView *treeview);
gchar *create_drag_list(gint playlist);
void playlist_scroll_to_row(GtkTreeView *treeview, gint position, gboolean only_if_focused);
GList *playlist_get_selected_list(GtkTreeView *treeview);
gint playlist_get_selected_length(GtkTreeView *treeview);
gint playlist_get_first_selected_index(GtkTreeView *treeview);
GtkTreePath *playlist_get_first_selected_path(GtkTreeView *treeview);
gint playlist_get_index_from_path(GtkTreePath * path);
gint calculate_column_width(GtkWidget *widget, gint num);

static inline void insert_drag_list_into_active(gint position, const gchar *list)
{
    gint active_playlist_nr = aud_playlist_get_active();
    insert_drag_list(active_playlist_nr, position, list);
}

#endif

