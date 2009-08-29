/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2008 Tomasz Moń <desowin@gmail.com>
 *  Copyright (C) 2009 William Pitcock <nenolod@atheme.org>
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

typedef struct
{
    gint old_index;
    gint new_index;
    GtkTreeRowReference *ref;
} UiPlaylistDragTracker;

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

static void ui_playlist_drag_tracker_free(UiPlaylistDragTracker * t)
{
    g_slice_free(UiPlaylistDragTracker, t);
}

static void _ui_playlist_widget_drag_begin(GtkTreeView * widget, GdkDragContext * context, gpointer data)
{
    UiPlaylistDragTracker *t = g_slice_new0(UiPlaylistDragTracker);
    GtkTreeModel *model;
    GtkTreeSelection *sel;
    GtkTreePath *path;
    GtkTreeIter iter;
    gulong handler_id;

    sel = gtk_tree_view_get_selection(widget);

    handler_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "selection_changed_handler_id"));
    g_signal_handler_block(G_OBJECT(sel), handler_id);

    if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
        return;

    model = gtk_tree_view_get_model(widget);
    path = gtk_tree_model_get_path(model, &iter);
    t->old_index = ui_playlist_widget_get_index_from_path(path);

    g_object_set_data_full(G_OBJECT(widget), "ui_playlist_drag_context", t, (GDestroyNotify) ui_playlist_drag_tracker_free);
}

static int _ui_playlist_widget_get_drop_index(GtkTreeView * widget, GdkDragContext * context, gint x, gint y)
{
    GtkTreePath *path;
    gint cx, cy, ins_pos = -1;
    gint playlist = ui_playlist_widget_get_playlist(widget);

    gdk_window_get_geometry(gtk_tree_view_get_bin_window(widget), &cx, &cy, NULL, NULL, NULL);

    if (gtk_tree_view_get_path_at_pos(widget, x -= cx, y -= cy, &path, NULL, &cx, &cy))
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
            if (ins_pos == -1) {
                ins_pos = aud_playlist_entry_count(playlist) - 1;
            }
        }
        else
            ins_pos = ui_playlist_widget_get_index_from_path(path);

        gtk_tree_path_free(path);
    } else {
        /* selected items are moved below all other entries */
        ins_pos = aud_playlist_entry_count(playlist) - 1;
    }

    return ins_pos;
}

static void _ui_playlist_widget_drag_motion(GtkTreeView * widget, GdkDragContext * context, gint x, gint y, guint time, gpointer user_data)
{
    UiPlaylistDragTracker *t;
    t = g_object_get_data(G_OBJECT(widget), "ui_playlist_drag_context");

    /* FIXME: this happens whilst dragging items from other applications */
    g_return_if_fail(t != NULL);

    t->new_index = _ui_playlist_widget_get_drop_index(widget, context, x, y);
}

static void _ui_playlist_widget_drag_data_received(GtkTreeView * widget, GdkDragContext * context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
    gint playlist = ui_playlist_widget_get_playlist(widget);
    gint delta;

    UiPlaylistDragTracker *t;
    t = g_object_get_data(G_OBJECT(widget), "ui_playlist_drag_context");

    g_return_if_fail(t != NULL);
    delta = t->new_index - t->old_index;

    aud_playlist_shift_selected(playlist, delta);
    /* replacing data with NULL will call connected destroy notify function */
    g_object_set_data(G_OBJECT(widget), "ui_playlist_drag_context", NULL);

    gtk_drag_finish(context, FALSE, FALSE, time);
}

static void _ui_playlist_widget_drag_end(GtkTreeView * widget, GdkDragContext * context, gpointer data)
{
    gulong handler_id;
    GtkTreeSelection *sel;

    sel = gtk_tree_view_get_selection(widget);

    handler_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "selection_changed_handler_id"));
    g_signal_handler_unblock(G_OBJECT(sel), handler_id);
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
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreePath *path;
    guint pos;

    model = gtk_tree_view_get_model(treeview);
    selection = gtk_tree_view_get_selection(treeview);

    if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
        return;

    path = gtk_tree_model_get_path(model, &iter);
    pos = ui_playlist_widget_get_index_from_path(path);

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
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        ui_manager_popup_menu_show(GTK_MENU(playlistwin_popup_menu), event->x_root, event->y_root + 2, 3, event->time);
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

    model = ui_playlist_model_new(playlist);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
    g_object_unref(model);

    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), TRUE);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
    gtk_drag_dest_set_track_motion(treeview, TRUE);

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

    g_signal_connect(treeview, "row-activated", G_CALLBACK(ui_playlist_widget_jump), NULL);

    g_signal_connect(treeview, "key-press-event", G_CALLBACK(ui_playlist_widget_keypress_cb), NULL);
    g_signal_connect(treeview, "button-press-event", G_CALLBACK(ui_playlist_widget_button_press_cb), NULL);

    g_signal_connect(treeview, "drag-begin", G_CALLBACK(_ui_playlist_widget_drag_begin), NULL);
    g_signal_connect(treeview, "drag-motion", G_CALLBACK(_ui_playlist_widget_drag_motion), NULL);
    g_signal_connect(treeview, "drag-data-received", G_CALLBACK(_ui_playlist_widget_drag_data_received), NULL);
    g_signal_connect(treeview, "drag-end", G_CALLBACK(_ui_playlist_widget_drag_end), NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    selection_changed_handler_id = g_signal_connect(selection, "changed", G_CALLBACK(_ui_playlist_widget_selection_changed), GINT_TO_POINTER(playlist));
    g_object_set_data(G_OBJECT(treeview), "selection_changed_handler_id", GINT_TO_POINTER(selection_changed_handler_id));

    return treeview;
}
