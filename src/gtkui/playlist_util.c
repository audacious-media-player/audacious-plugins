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

#include <glib.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>

#include "playlist_util.h"
#include "ui_playlist_model.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

GtkTreeView *playlist_get_treeview_from_page(GtkWidget *page)
{
    if (!page)
        return NULL;

    return GTK_TREE_VIEW(g_object_get_data(G_OBJECT(page), "treeview"));
}

GtkTreeView *playlist_get_treeview(gint playlist)
{
    GtkWidget *page = gtk_notebook_get_nth_page(UI_PLAYLIST_NOTEBOOK, playlist);

    if (!page)
        return NULL;

    return GTK_TREE_VIEW(g_object_get_data(G_OBJECT(page), "treeview"));
}

GtkTreeView *playlist_get_active_treeview(void)
{
    return playlist_get_treeview(aud_playlist_get_active());
}

GtkTreeView *playlist_get_playing_treeview(void)
{
    return playlist_get_treeview(aud_playlist_get_playing());
}

gint treeview_get_playlist (GtkTreeView * tree)
{
    return ui_playlist_model_get_playlist ((UiPlaylistModel *)
     gtk_tree_view_get_model (tree));
}

static inline void _gtk_tree_selection_select_path(GtkTreePath *path, GtkTreeSelection *sel)
{
    gtk_tree_selection_select_path(sel, path);
}

void playlist_set_selected(GtkTreeView *treeview, GtkTreePath *path)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);

    gtk_tree_selection_unselect_all(sel);
    gtk_tree_selection_select_path(sel, path);
    gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
}

void playlist_set_selected_list(GtkTreeView *treeview, GList *list, gint distance)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    GtkTreePath *path;
    gint pos;

    gtk_tree_selection_unselect_all(sel);

    if (distance == 0)
    {
        gtk_tree_view_set_cursor(treeview, g_list_first(list)->data, NULL, FALSE);
        g_list_foreach(list, (GFunc) _gtk_tree_selection_select_path, sel);
        return;
    }

    for (GList *target = g_list_first(list); target; target = target->next)
    {
        if (!target->data)
            continue;

        pos = playlist_get_index_from_path(target->data) + distance;
        path = gtk_tree_path_new_from_indices(pos, -1);

        if (path)
        {
            gtk_tree_selection_select_path(sel, path);

            if (target->prev == NULL)
                gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);

            gtk_tree_path_free(path);
        }
    }
}

GList *playlist_get_selected_list(GtkTreeView *treeview)
{
    GtkTreeModel *treemodel;
    GtkTreeSelection *sel;

    g_return_val_if_fail(treeview != NULL, NULL);

    treemodel = gtk_tree_view_get_model(treeview);

    sel = gtk_tree_view_get_selection(treeview);
    g_return_val_if_fail(sel != NULL, NULL);

    return gtk_tree_selection_get_selected_rows(sel, &treemodel);
}

gint playlist_get_selected_length(GtkTreeView *treeview)
{
    GList *list = playlist_get_selected_list(treeview);
    gint selected_length;

    selected_length = g_list_length(list);

    g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(list);

    return selected_length;
}

GtkTreePath *playlist_get_first_selected_path(GtkTreeView *treeview)
{
    GList *list;
    GtkTreePath *path;

    if (! (list = playlist_get_selected_list (treeview)))
        return NULL;

    path = gtk_tree_path_copy(list->data);

    g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(list);

    return path;
}

gint playlist_get_first_selected_index(GtkTreeView *treeview)
{
    GtkTreePath *path = playlist_get_first_selected_path(treeview);

    if (! path)
        return -1;

    gint selected = playlist_get_index_from_path(path);
    gtk_tree_path_free(path);

    return selected;
}

gint playlist_get_index_from_path(GtkTreePath * path)
{
    gint *pos;

    g_return_val_if_fail(path != NULL, -1);

    if (!(pos = gtk_tree_path_get_indices(path)))
        return -1;

    return pos[0];
}

void playlist_select_range (gint list, gint top, gint length)
{
    gint count;

    aud_playlist_select_all (list, FALSE);

    for (count = 0; count < length; count ++)
        aud_playlist_entry_set_selected (list, top + count, TRUE);
}

gint playlist_count_selected_in_range (gint list, gint top, gint length)
{
    gint selected = 0;
    gint count;

    for (count = 0; count < length; count ++)
    {
        if (aud_playlist_entry_get_selected (list, top + count))
            selected ++;
    }

    return selected;
}

void playlist_selected_to_indexes (gint list, struct index * * namesp,
 struct index * * tuplesp)
{
    gint entries = aud_playlist_entry_count (list);
    gint count;
    const Tuple * tuple;

    * namesp = index_new ();
    * tuplesp = index_new ();

    for (count = 0; count < entries; count ++)
    {
        if (! aud_playlist_entry_get_selected (list, count))
            continue;

        index_append (* namesp, g_strdup (aud_playlist_entry_get_filename (list,
         count)));

        if ((tuple = aud_playlist_entry_get_tuple (list, count, FALSE)))
            mowgli_object_ref ((Tuple *) tuple);

        index_append (* tuplesp, (Tuple *) tuple);
    }
}

gint treeview_get_focus (GtkTreeView * tree)
{
    GtkTreePath * path;
    gint focus = -1;

    gtk_tree_view_get_cursor (tree, & path, NULL);
    if (path)
    {
        focus = gtk_tree_path_get_indices (path)[0];
        gtk_tree_path_free (path);
    }

    return focus;
}

void treeview_set_focus (GtkTreeView * tree, gint focus)
{
    if (aud_playlist_update_pending ())
        treeview_set_focus_on_update (tree, focus);
    else
    {
        ui_playlist_widget_block_updates ((GtkWidget *) tree, TRUE);
        treeview_set_focus_now (tree, focus);
        ui_playlist_widget_block_updates ((GtkWidget *) tree, FALSE);
    }
}

void treeview_set_focus_now (GtkTreeView * tree, gint focus)
{
    gint entries = aud_playlist_entry_count (treeview_get_playlist (tree));

    if (focus < 0)
    {
        if (! entries)
            return;
        focus = 0;
    }

    GtkTreePath * path = gtk_tree_path_new_from_indices (focus, -1);

    ui_playlist_widget_freeze_selection ((GtkWidget *) tree, TRUE);
    gtk_tree_view_set_cursor (tree, path, NULL, FALSE);
    ui_playlist_widget_freeze_selection ((GtkWidget *) tree, FALSE);

    gtk_tree_view_scroll_to_cell (tree, path, NULL, FALSE, 0, 0);
    gtk_tree_path_free (path);
}

void treeview_refresh_selected (GtkTreeView * tree, gint at, gint count)
{
    UiPlaylistModel * model = (UiPlaylistModel *) gtk_tree_view_get_model (tree);
    gint playlist = ui_playlist_model_get_playlist (model);
    GtkTreeSelection * sel = gtk_tree_view_get_selection (tree);

    GtkTreeIter iter;
    gtk_tree_model_iter_nth_child ((GtkTreeModel *) model, & iter, NULL, at);

    for (gint i = at; i < at + count; i ++)
    {
        if (aud_playlist_entry_get_selected (playlist, i))
            gtk_tree_selection_select_iter (sel, & iter);
        else
            gtk_tree_selection_unselect_iter (sel, & iter);

        gtk_tree_model_iter_next ((GtkTreeModel *) model, & iter);
    }
}

void treeview_add_indexes (GtkTreeView * tree, gint row, struct index * names,
 struct index * tuples)
{
    gint playlist = treeview_get_playlist (tree);
    gint entries = aud_playlist_entry_count (playlist);
    gint new;

    if (row < 0)
        row = entries;

    aud_playlist_entry_insert_batch (playlist, row, names, tuples);
    new = aud_playlist_entry_count (playlist);
    playlist_select_range (playlist, row, new - entries);
    treeview_set_focus (tree, MIN (row, new - 1));
}

void treeview_add_urilist (GtkTreeView * tree, gint row, const gchar * list)
{
    gint playlist = treeview_get_playlist (tree);
    gint entries = aud_playlist_entry_count (playlist);
    gint new;

    if (row < 0)
        row = entries;

    audgui_urilist_insert (playlist, row, list);
    new = aud_playlist_entry_count (playlist);
    playlist_select_range (playlist, row, new - entries);
    treeview_set_focus (tree, MIN (row, new - 1));
}
