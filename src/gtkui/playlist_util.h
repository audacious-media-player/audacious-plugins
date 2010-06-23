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

void playlist_set_selected(GtkTreeView *treeview, GtkTreePath *path);
void playlist_set_selected_list(GtkTreeView *treeview, GList *list, gint distance);
GtkTreeView *playlist_get_treeview_from_page(GtkWidget *page);
GtkTreeView *playlist_get_treeview(gint playlist);
GtkTreeView *playlist_get_active_treeview(void);
GtkTreeView *playlist_get_playing_treeview(void);
gint playlist_get_playlist_from_treeview(GtkTreeView *treeview);
void playlist_scroll_to_row(GtkTreeView *treeview, gint position);
GList *playlist_get_selected_list(GtkTreeView *treeview);
gint playlist_get_selected_length(GtkTreeView *treeview);
gint playlist_get_first_selected_index(GtkTreeView *treeview);
GtkTreePath *playlist_get_first_selected_path(GtkTreeView *treeview);
gint playlist_get_index_from_path(GtkTreePath * path);
gint calculate_column_width(GtkWidget *widget, gint num);
void playlist_select_range(GtkTreeView *treeview, GtkTreePath *start_path, GtkTreePath *end_path);
void playlist_pending_selection_set(GtkTreeView *treeview, GtkTreePath *start_path, GtkTreePath *end_path);
void playlist_pending_selection_free(void);
void playlist_pending_selection_apply(void);
gboolean playlist_is_pending_selection(void);
void playlist_block_selection(GtkTreeView *treeview);
void playlist_unblock_selection(GtkTreeView *treeview);

gint playlist_count_selected_in_range (gint list, gint top, gint length);

gint treeview_get_focus (GtkTreeView * tree);
void treeview_set_focus (GtkTreeView * tree, gint focus);
void treeview_clear_selection (GtkTreeView * tree);
void treeview_set_selection_from_playlist (GtkTreeView * tree, gint list);

#endif
