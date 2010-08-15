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
gint treeview_get_playlist (GtkTreeView * tree);
void playlist_scroll_to_row(GtkTreeView *treeview, gint position);
GList *playlist_get_selected_list(GtkTreeView *treeview);
gint playlist_get_selected_length(GtkTreeView *treeview);
gint playlist_get_first_selected_index(GtkTreeView *treeview);
GtkTreePath *playlist_get_first_selected_path(GtkTreeView *treeview);
gint playlist_get_index_from_path(GtkTreePath * path);

void playlist_select_range (gint list, gint top, gint length);
gint playlist_count_selected_in_range (gint list, gint top, gint length);
void playlist_selected_to_indexes (gint list, struct index * * namesp,
 struct index * * tuplesp);

gint treeview_get_focus (GtkTreeView * tree);
void treeview_set_focus (GtkTreeView * tree, gint focus);
void treeview_set_focus_now (GtkTreeView * tree, gint focus);

void treeview_refresh_selected (GtkTreeView * tree, gint at, gint count);

void treeview_add_indexes (GtkTreeView * tree, gint row, struct index * names,
 struct index * tuples);
void treeview_add_urilist (GtkTreeView * tree, gint row, const gchar * list);
void treeview_remove_selected (GtkTreeView * tree);

#endif
