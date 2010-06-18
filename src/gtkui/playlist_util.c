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
#include <stdlib.h>
#include <string.h>
#include <audacious/plugin.h>

#include "playlist_util.h"
#include "ui_playlist_model.h"
#include "ui_playlist_notebook.h"

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

gint playlist_get_playlist_from_treeview(GtkTreeView *treeview)
{
    g_return_val_if_fail(treeview != NULL, -1);

    GtkTreeModel *tree_model = gtk_tree_view_get_model(treeview);
    UiPlaylistModel *model = UI_PLAYLIST_MODEL(tree_model);

    return model->playlist;
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
    aud_playlist_shift_selected(playlist_num, delta);
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

void playlist_scroll_to_row(GtkTreeView *treeview, gint position, gboolean only_if_focused)
{
    GtkTreePath *path = gtk_tree_path_new_from_indices(position, -1);

    g_return_if_fail(treeview != NULL);
    g_return_if_fail(path != NULL);
    g_return_if_fail(only_if_focused && !gtk_widget_is_focus(GTK_WIDGET(treeview)));

    gtk_tree_view_scroll_to_cell(treeview, path, NULL, TRUE, 0.5, 0.0);
    gtk_tree_path_free(path);
}

GList *playlist_get_selected_list(gint playlist)
{
    GtkTreeView *treeview = playlist_get_treeview(playlist);
    GtkTreeModel *treemodel = gtk_tree_view_get_model(treeview);
    GtkTreeSelection *sel;

    g_return_val_if_fail(treeview != NULL, NULL);

    sel = gtk_tree_view_get_selection(treeview);
    g_return_val_if_fail(sel != NULL, NULL);

    return gtk_tree_selection_get_selected_rows(sel, &treemodel);
}

gint playlist_get_selected_length(gint playlist)
{
    GList *list = playlist_get_selected_list(playlist);
    gint selected_length;

    selected_length = g_list_length(list);

    g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(list);

    return selected_length;
}

gint playlist_get_first_selected_index(gint playlist)
{
    GList *list;

    gint selected;

    g_return_val_if_fail(playlist >= 0, 0);

    list = playlist_get_selected_list(playlist);
    g_return_val_if_fail(list != NULL, 0);

    selected = playlist_get_index_from_path(list->data);

    g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(list);

    return selected;
}

gint playlist_get_first_selected_index_from_treeview(GtkTreeView *treeview)
{
    gint playlist = playlist_get_playlist_from_treeview(treeview);
    return playlist_get_first_selected_index(playlist);
}

GtkTreeSelection *playlist_get_selection_from_treeview(GtkTreeView *treeview)
{
    g_return_val_if_fail(treeview != NULL, NULL);
    return gtk_tree_view_get_selection(treeview);
}

GtkTreeSelection *playlist_get_selection(gint playlist)
{
    GtkTreeView *treeview = playlist_get_treeview(playlist);
    return playlist_get_selection_from_treeview(treeview);
}

gint playlist_get_index_from_path(GtkTreePath * path)
{
    gint *pos;

    g_return_val_if_fail(path != NULL, -1);

    if (!(pos = gtk_tree_path_get_indices(path)))
        return -1;

    return pos[0];
}

gint calculate_column_width(GtkWidget *widget, gint num)
{
    PangoFontDescription *font_desc;
    PangoContext *context;
    PangoFontMetrics *font_metrics;
    gint len = 0;
    gchar *buf = g_new0(gchar, 10);
    gchar *p = buf;

    font_desc = widget->style->font_desc;
    context = gtk_widget_get_pango_context(widget);
    font_metrics = pango_context_get_metrics(context, font_desc,
                                             pango_context_get_language(context));

    sprintf(buf, "%d", num);

    while(*(buf++))
        len++;

    g_free(p);

    return (PANGO_PIXELS(pango_font_metrics_get_approximate_digit_width(font_metrics)) * len) + 20;
}
