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

#include <glib.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <audacious/plugin.h>

#include "ui_playlist_notebook.h"

static void find_first_selected_forearch(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer userdata)
{
    gint *first_one = (gint*) userdata;
    gint ret = *gtk_tree_path_get_indices(path);

    if (*first_one == -1)
        *first_one = ret;
    else if (*first_one > ret)
        *first_one = ret;
}

static void find_last_selected_forearch(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer userdata)
{
    gint *last_one = (gint*) userdata;
    gint *ret = gtk_tree_path_get_indices(path);

    if (!ret)
        return;

    if (*last_one < *ret)
        *last_one = *ret;
}

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

gint get_first_selected_pos(GtkTreeView *tv)
{
    GtkTreeView *treeview = tv;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    gint selected_pos = -1;

    gtk_tree_selection_selected_foreach(sel,
        &find_first_selected_forearch, &selected_pos);
    return selected_pos;

}

gint get_last_selected_pos(GtkTreeView *tv)
{
    GtkTreeView *treeview = tv;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeview);
    gint selected_pos = -1;

    gtk_tree_selection_selected_foreach(sel,
        &find_last_selected_forearch, &selected_pos);

    return selected_pos;

}

static gint need_update_range_start = -1;
static gint need_update_range_end = -1;

void playlist_get_changed_range(gint *start, gint *end)
{
    *end = need_update_range_end;
    *start = need_update_range_start;

    need_update_range_start = need_update_range_end = -1;
}

void playlist_shift_selected(gint playlist_num, gint old_pos, gint new_pos, gint selected_length)
{
    gint delta = new_pos - old_pos;
    need_update_range_start = MIN(old_pos, new_pos);
    need_update_range_end = MAX(new_pos, old_pos) + selected_length;

    aud_playlist_shift_selected(playlist_num, delta);
}
/**
 * To retrieve the first row number (based 0) within the selection if any.
 */

gint get_active_selected_pos(void)
{
    return get_first_selected_pos(playlist_get_active_treeview());
}

/**
 * FIXME:
 */
void treeview_select_pos(GtkTreeView *tv, gint pos)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(tv);
    GtkTreePath *path;

    path = gtk_tree_path_new_from_indices(pos, -1);
    gtk_tree_selection_unselect_all(sel);
    gtk_tree_selection_select_range(sel, path, path);
    gtk_tree_path_free(path);
}


void insert_drag_list(gint playlist, gint position, const gchar *list)
{
    struct index *add = index_new();
    const gchar * end, * next;
    gchar * filename;

    while (list[0])
    {
        if ((end = strstr (list, "\r\n")) != NULL)
            next = end + 2;
        else if ((end = strchr (list, '\n')) != NULL)
            next = end + 1;
        else
            next = end = strchr (list, 0);

        filename = g_strndup (list, end - list);

        if (vfs_file_test (filename, G_FILE_TEST_IS_DIR))
        {
            aud_playlist_insert_folder (playlist, position, filename);
            g_free (filename);
        }
        else if (aud_filename_is_playlist (filename))
        {
            gint entries = aud_playlist_entry_count (playlist);

            aud_playlist_insert_playlist (playlist, position, filename);
            position += aud_playlist_entry_count (playlist) - entries;
        }
        else
            index_append (add, filename);

        list = next;
    }

    aud_playlist_entry_insert_batch(playlist, position, add, NULL);
}

gchar *create_drag_list(gint playlist)
{
    gint entries = aud_playlist_entry_count(playlist);
    gint length = 0;
    gint count, slength;
    const gchar *filename;
    gchar *buffer, *set;

    for (count = 0; count < entries; count++)
    {
        if (!aud_playlist_entry_get_selected(playlist, count))
            continue;

        filename = aud_playlist_entry_get_filename(playlist, count);
        g_return_val_if_fail(filename != NULL, NULL);
        length += strlen(filename) + 1;
    }

    if (!length)
        return NULL;

    buffer = g_malloc(length);
    set = buffer;

    for (count = 0; count < entries; count++)
    {
        if (!aud_playlist_entry_get_selected(playlist, count))
            continue;

        filename = aud_playlist_entry_get_filename(playlist, count);
        g_return_val_if_fail(filename != NULL, NULL);
        slength = strlen (filename);
        g_return_val_if_fail(slength + 1 <= length, NULL);
        memcpy(set, filename, slength);
        set += slength;
        *set++ = '\n';
        length -= slength + 1;
    }

    /* replace the last newline with a null */
    *--set = 0;

    return buffer;
}
