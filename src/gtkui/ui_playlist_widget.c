/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2008 Tomasz Moń <desowin@gmail.com>
 *  Copyright (C) 2009 William Pitcock <nenolod@atheme.org>
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <audacious/plugin.h>

#include "ui_manager.h"
#include "ui_playlist_model.h"
#include "playlist_util.h"

typedef struct
{
    GList *targets;
    GtkWidget *source_widget;
    GtkTreePath *drop_path;
} UiPlaylistDragTracker;

static UiPlaylistDragTracker *t;

static gint ui_playlist_widget_get_playlist(GtkTreeView *treeview)
{
    GtkTreeModel *tree_model = gtk_tree_view_get_model(treeview);
    UiPlaylistModel *model = UI_PLAYLIST_MODEL(tree_model);

    return model->playlist;
}

static gint ui_playlist_widget_get_index_from_path(GtkTreePath * path)
{
    gint *pos;

    g_return_val_if_fail(path != NULL, -1);

    if (!(pos = gtk_tree_path_get_indices(path)))
        return -1;

    return pos[0];
}

static void _ui_playlist_widget_drag_begin(GtkWidget *widget, GdkDragContext * context, gpointer data)
{
    t = g_slice_new0(UiPlaylistDragTracker);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    gulong handler_id;

    g_signal_stop_emission_by_name(widget, "drag-begin");
    handler_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "selection_changed_handler_id"));
    g_signal_handler_block(G_OBJECT(sel), handler_id);

    t->targets = gtk_tree_selection_get_selected_rows(sel, NULL);
    t->source_widget = widget;
    t->drop_path = NULL;
}

static void _ui_playlist_widget_drag_motion(GtkTreeView * widget, GdkDragContext * context, gint x, gint y, guint time, gpointer user_data)
{
    GtkTreeViewDropPosition dp;
    GtkTreePath *path = NULL;
    gint cx, cy, ins_pos = -1;
    gint playlist = ui_playlist_widget_get_playlist(widget);

    /* FIXME: this happens whilst dragging items from other applications */
    g_return_if_fail(t != NULL);

    gdk_window_get_geometry(gtk_tree_view_get_bin_window(widget), &cx, &cy, NULL, NULL, NULL);

    if (y <= cy)
    {
        ins_pos = 0;
    }
    else if (gtk_tree_view_get_path_at_pos(widget, x -= cx, y -= cy, &path, NULL, &cx, &cy))
    {
        GdkRectangle rect;

        /* in lower 1/3 of row? use next row as target */
        gtk_tree_view_get_background_area(widget, path, gtk_tree_view_get_column(widget, 0), &rect);

        if (cy >= rect.height * 2 / 3.0)
        {
            gtk_tree_path_free(path);

            if (gtk_tree_view_get_path_at_pos(widget, x, y + rect.height, &path, NULL, &cx, &cy))
                ins_pos = ui_playlist_widget_get_index_from_path(path);

            /* last entry */
            if (ins_pos == -1)
                ins_pos = aud_playlist_entry_count(playlist) - 1;
        }
        else
            ins_pos = ui_playlist_widget_get_index_from_path(path);
    }
    else
    {
        /* selected items are moved below all other entries */
        ins_pos = aud_playlist_entry_count(playlist) - 1;
    }

    if (path != NULL)
    {
        t->drop_path = path;
        gtk_tree_view_set_drag_dest_row(widget, path, dp);
    }
}

static void _ui_playlist_widget_drag_data_received(GtkTreeView * widget, GdkDragContext * context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
    gint playlist = ui_playlist_widget_get_playlist(widget);

    /* Maybe dragged some files from another application. */
    if (gtk_drag_get_source_widget(context) == NULL)
    {
        GtkTreePath *drop_path = NULL;
        GtkTreeViewDropPosition drop_pos;
        gint ins_pos;

        gint ret = gtk_tree_view_get_dest_row_at_pos(widget, x, y, &drop_path, &drop_pos);

        /* If we can't get the path position, maybe means the position is out of the row area. */
        if (!ret)
            ins_pos = aud_playlist_entry_count(playlist);
        else
            ins_pos = ui_playlist_widget_get_index_from_path(drop_path);

        insert_drag_list_into_active(ins_pos, (gchar *)data->data);
    }

    gtk_drag_finish(context, FALSE, FALSE, time);
}

static void _ui_playlist_widget_drag_end(GtkTreeView * widget, GdkDragContext * context, gpointer data)
{
    gulong handler_id;
    GtkTreeSelection *sel;
    gint pos;
    struct index *filenames;
    struct index *tuples;

    g_return_if_fail(t != NULL);
    g_return_if_fail(t->drop_path != NULL);
    g_return_if_fail(t->targets != NULL);
    g_return_if_fail(t->source_widget != NULL);

    sel = gtk_tree_view_get_selection(widget);
    handler_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "selection_changed_handler_id"));
    g_signal_handler_unblock(G_OBJECT(sel), handler_id);

    gint drop_pos = ui_playlist_widget_get_index_from_path(t->drop_path);
    gint source_playlist = ui_playlist_widget_get_playlist(GTK_TREE_VIEW(t->source_widget));
    gint dest_playlist = ui_playlist_widget_get_playlist(playlist_get_active_treeview());

    if (source_playlist == dest_playlist)
    {
        gint drag_pos = ui_playlist_widget_get_index_from_path(g_list_first(t->targets)->data);
        playlist_shift_selected(source_playlist, drag_pos, drop_pos, g_list_length(t->targets));
    }
    else
    {
        filenames = index_new();
        tuples = index_new();

        for (GList *target = g_list_first(t->targets); target; target = target->next)
        {
            if (target->data != NULL)
            {
                pos = ui_playlist_widget_get_index_from_path(target->data);

                gchar *filename = g_strdup(aud_playlist_entry_get_filename(source_playlist, pos));

                if (filename != NULL)
                {
                    Tuple *tuple = tuple_copy(aud_playlist_entry_get_tuple(source_playlist, pos));
                    index_append(filenames, filename);
                    index_append(tuples, tuple);
                }

                gtk_tree_path_free(target->data);
            }
        }

        aud_playlist_entry_insert_batch(dest_playlist, drop_pos, filenames, tuples);
        aud_playlist_delete_selected(source_playlist);
    }

    gtk_widget_grab_focus(GTK_WIDGET(playlist_get_treeview(dest_playlist)));

    sel = gtk_tree_view_get_selection(playlist_get_treeview(dest_playlist));

    gtk_tree_selection_unselect_all(sel);
    gtk_tree_selection_select_path(sel, t->drop_path);
    gtk_tree_path_free(t->drop_path);
    g_list_free(t->targets);
    g_slice_free(UiPlaylistDragTracker, t);
}

static void _ui_playlist_widget_selection_update(GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer playlist_p)
{
    gint entry;

    gtk_tree_model_get(model, iter, PLAYLIST_COLUMN_NUM, &entry, -1);

    /* paths are numbered from 1, playlist index start from 0 */
    entry -= 1;

    aud_playlist_entry_set_selected(GPOINTER_TO_INT(playlist_p), entry, TRUE);
}

static void _ui_playlist_widget_selection_changed(GtkTreeSelection * selection, gpointer playlist_p)
{
    gint playlist = GPOINTER_TO_INT(playlist_p);

    aud_playlist_select_all(playlist, FALSE);

    gtk_tree_selection_selected_foreach(selection, _ui_playlist_widget_selection_update, playlist_p);
}

static void ui_playlist_widget_change_song(GtkTreeView * treeview, guint pos)
{
    gint playlist = ui_playlist_widget_get_playlist(treeview);

    aud_playlist_set_playing(playlist);
    aud_playlist_set_position(playlist, pos);

    if (!audacious_drct_get_playing())
        audacious_drct_play();
}

static void ui_playlist_widget_jump(GtkTreeView * treeview, gpointer data)
{
    gint pos;
    pos = get_first_selected_pos(treeview);

    if (pos >= 0)
        ui_playlist_widget_change_song(treeview, pos);
}

static gboolean ui_playlist_widget_keypress_cb(GtkWidget * widget, GdkEventKey * event, gpointer data)
{
    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
      case 0:
        switch (event->keyval)
        {
          case GDK_KP_Enter:
            ui_playlist_widget_jump(GTK_TREE_VIEW(widget), NULL);
            return TRUE;
          default:
            return FALSE;
        }
        break;
      case GDK_MOD1_MASK:
      {
        if ((event->keyval == GDK_Up) || (event->keyval == GDK_Down)) {
            gint playlist = ui_playlist_widget_get_playlist(GTK_TREE_VIEW(widget));
            /* Copy the event, so we can get the selection to move as well */
            GdkEvent * ev = gdk_event_copy((GdkEvent *) event);
            ((GdkEventKey *) ev)->state = 0;

            aud_playlist_shift_selected(playlist, (event->keyval == GDK_Up) ? -1 : 1);
            gtk_propagate_event(widget, ev);
            gdk_event_free(ev);
            return TRUE;
        }
      }
      default:
        return FALSE;
    }
    return FALSE;
}

static gboolean ui_playlist_widget_button_press_cb(GtkWidget * widget, GdkEventButton * event)
{
    GtkTreePath *path = NULL;
    GtkTreeSelection *sel = NULL;
    gint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);

    gtk_widget_grab_focus(widget);

    if (event->button == 1 && state)
        return FALSE;

    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, NULL, NULL);
    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
        ui_manager_popup_menu_show(GTK_MENU(playlistwin_popup_menu), event->x_root, event->y_root + 2, 3, event->time);

    if (path == NULL)
        return FALSE;

    if (event->button == 1 && !state && event->type == GDK_2BUTTON_PRESS)
    {
        gtk_tree_view_row_activated(GTK_TREE_VIEW(widget), path, NULL);
        gtk_tree_path_free(path);
        return FALSE;
    }

    if (gtk_tree_selection_path_is_selected(sel, path))
    {
        gtk_tree_path_free(path);
        return TRUE;
    }

    gtk_tree_path_free(path);
    return FALSE;
}

static gboolean ui_playlist_widget_button_release_cb(GtkWidget * widget, GdkEventButton * event)
{
    GtkTreePath *path = NULL;
    GtkTreeSelection *sel = NULL;
    gint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);

    if (event->button == 1 && !state)
    {
        gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, NULL, NULL);
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

        if (path == NULL)
            return FALSE;

        gtk_tree_selection_unselect_all(sel);
        gtk_tree_selection_select_path(sel, path);
        gtk_tree_path_free(path);
    }

    return FALSE;
}

GtkWidget *ui_playlist_widget_new(gint playlist)
{
    GtkWidget *treeview;
    UiPlaylistModel *model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    gulong selection_changed_handler_id;
    const GtkTargetEntry target_entry[] = {
        {"text/uri-list", 0, 0}
    };

    model = ui_playlist_model_new(playlist);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
    g_object_unref(model);

    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), TRUE);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
    gtk_drag_dest_set_track_motion(treeview, TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), 1);

    if (multi_column_view)
    {
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(NULL, renderer, "text", PLAYLIST_MULTI_COLUMN_NUM, "weight", PLAYLIST_MULTI_COLUMN_WEIGHT, NULL);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_spacing(column, 4);
        gtk_tree_view_column_set_min_width(column, 40);
        g_object_set(G_OBJECT(renderer), "ypad", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("Artist", renderer, "text", PLAYLIST_MULTI_COLUMN_ARTIST, "weight", PLAYLIST_MULTI_COLUMN_WEIGHT, NULL);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_spacing(column, 4);
        gtk_tree_view_column_set_min_width(column, 150);
        g_object_set(G_OBJECT(renderer), "ypad", 0, "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("Album", renderer, "text", PLAYLIST_MULTI_COLUMN_ALBUM, "weight", PLAYLIST_MULTI_COLUMN_WEIGHT, NULL);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_spacing(column, 4);
        gtk_tree_view_column_set_min_width(column, 200);
        g_object_set(G_OBJECT(renderer), "ypad", 0, "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("No", renderer, "text", PLAYLIST_MULTI_COLUMN_TRACK_NUM, "weight", PLAYLIST_MULTI_COLUMN_WEIGHT, NULL);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_spacing(column, 4);
        gtk_tree_view_column_set_min_width(column, 40);
        g_object_set(G_OBJECT(renderer), "ypad", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("Title", renderer, "text", PLAYLIST_MULTI_COLUMN_TITLE, "weight", PLAYLIST_MULTI_COLUMN_WEIGHT, NULL);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_spacing(column, 4);
        gtk_tree_view_column_set_min_width(column, 300);
        g_object_set(G_OBJECT(renderer), "ypad", 0, "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("Time", renderer, "text", PLAYLIST_MULTI_COLUMN_TIME, "weight", PLAYLIST_MULTI_COLUMN_WEIGHT, NULL);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_spacing(column, 4);
        gtk_tree_view_column_set_fixed_width(column, 50);
        g_object_set(G_OBJECT(renderer), "ypad", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    }
    else
    {
        column = gtk_tree_view_column_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_spacing(column, 4);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, FALSE);
        gtk_tree_view_column_set_attributes(column, renderer, "text", PLAYLIST_COLUMN_NUM, "weight", PLAYLIST_COLUMN_WEIGHT, NULL);
        g_object_set(G_OBJECT(renderer), "ypad", 0, NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, TRUE);
        gtk_tree_view_column_set_attributes(column, renderer, "text", PLAYLIST_COLUMN_TEXT, "weight", PLAYLIST_COLUMN_WEIGHT, NULL);
        g_object_set(G_OBJECT(renderer), "ypad", 0, "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, FALSE);
        gtk_tree_view_column_set_attributes(column, renderer, "text", PLAYLIST_COLUMN_TIME, "weight", PLAYLIST_COLUMN_WEIGHT, NULL);
        g_object_set(G_OBJECT(renderer), "ypad", 0, "xalign", 1.0, NULL);

        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    }

    gtk_drag_dest_set(treeview, GTK_DEST_DEFAULT_ALL, target_entry, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    gtk_drag_source_set(treeview, GDK_BUTTON1_MASK, target_entry, 1, GDK_ACTION_MOVE);

    g_signal_connect(treeview, "row-activated", G_CALLBACK(ui_playlist_widget_jump), NULL);

    g_signal_connect(treeview, "key-press-event", G_CALLBACK(ui_playlist_widget_keypress_cb), NULL);
    g_signal_connect(treeview, "button-press-event", G_CALLBACK(ui_playlist_widget_button_press_cb), NULL);
    g_signal_connect(treeview, "button-release-event", G_CALLBACK(ui_playlist_widget_button_release_cb), NULL);

    g_signal_connect(treeview, "drag-begin", G_CALLBACK(_ui_playlist_widget_drag_begin), NULL);
    g_signal_connect(treeview, "drag-motion", G_CALLBACK(_ui_playlist_widget_drag_motion), NULL);
    g_signal_connect(treeview, "drag-data-received", G_CALLBACK(_ui_playlist_widget_drag_data_received), NULL);
    g_signal_connect(treeview, "drag-end", G_CALLBACK(_ui_playlist_widget_drag_end), NULL);
    g_signal_connect_swapped(model, "row-changed", G_CALLBACK(gtk_widget_queue_draw), treeview);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
    gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(treeview), TRUE);
    selection_changed_handler_id = g_signal_connect(selection, "changed", G_CALLBACK(_ui_playlist_widget_selection_changed), GINT_TO_POINTER(playlist));
    g_object_set_data(G_OBJECT(treeview), "selection_changed_handler_id", GINT_TO_POINTER(selection_changed_handler_id));

    return treeview;
}
