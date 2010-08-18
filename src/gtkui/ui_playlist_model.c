/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2009 Tomasz Moń <desowin@gmail.com>
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

#include "config.h"

#include <audacious/audconfig.h>
#include <audacious/debug.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>

#include "ui_playlist_model.h"
#include "ui_playlist_widget.h"
#include "playlist_util.h"

struct _UiPlaylistModel {
    GObject parent;

    guint num_rows;
    gint playlist; /* associated playlist number */
    gint position;
    GList * queue;
    gboolean song_changed, focus_changed;
    gint focus;

    gint n_columns;
    GType * column_types;

    gint stamp; /* Random integer to check whether an iter belongs to our model */
};

struct _UiPlaylistModelClass {
    GObjectClass parent_class;
};

GType ui_playlist_model_get_type (void);

static GObjectClass *parent_class = NULL;

static void ui_playlist_model_class_init(UiPlaylistModelClass *klass);
static void ui_playlist_tree_model_init(GtkTreeModelIface *iface);
static void ui_playlist_model_init(UiPlaylistModel *model);
static void ui_playlist_model_finalize(GObject *object);
static GtkTreeModelFlags ui_playlist_model_get_flags(GtkTreeModel *model);
static gint ui_playlist_model_get_n_columns(GtkTreeModel *model);
static GType ui_playlist_model_get_column_type(GtkTreeModel *model, gint index);
static gboolean ui_playlist_model_get_iter(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path);
static GtkTreePath * ui_playlist_model_get_path(GtkTreeModel *tree_model, GtkTreeIter *iter);
static void ui_playlist_model_get_value(GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value);
static gboolean ui_playlist_model_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean ui_playlist_model_iter_children(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent);
static gboolean ui_playlist_model_iter_has_child(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gint ui_playlist_model_iter_n_children(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean ui_playlist_model_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n);
static gboolean ui_playlist_model_iter_parent(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child);

GType
ui_playlist_model_get_type(void)
{
    static GType ui_playlist_model_type = 0;

    if (ui_playlist_model_type == 0)
    {
        static const GTypeInfo ui_playlist_model_info =
        {
            sizeof(UiPlaylistModelClass),
            NULL,                                    /* base_init */
            NULL,                                    /* base_finalize */
            (GClassInitFunc) ui_playlist_model_class_init,
            NULL,                                    /* class_finalize */
            NULL,                                    /* class_data */
            sizeof(UiPlaylistModel),
            0,                                       /* n_preallocs */
            (GInstanceInitFunc) ui_playlist_model_init
        };

        static const GInterfaceInfo tree_model_info =
        {
            (GInterfaceInitFunc) ui_playlist_tree_model_init,
            NULL,
            NULL
        };

        ui_playlist_model_type =
            g_type_register_static(G_TYPE_OBJECT, "UiPlaylistModel",
                                   &ui_playlist_model_info, (GTypeFlags)0);

        g_type_add_interface_static(ui_playlist_model_type, GTK_TYPE_TREE_MODEL,
                                    &tree_model_info);
    }

    return ui_playlist_model_type;
}

static void
ui_playlist_model_class_init(UiPlaylistModelClass *klass)
{
    GObjectClass *object_class;

    parent_class = (GObjectClass *) g_type_class_peek_parent(klass);
    object_class = (GObjectClass *) klass;

    object_class->finalize = ui_playlist_model_finalize;
}

static void
ui_playlist_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags       = ui_playlist_model_get_flags;
    iface->get_n_columns   = ui_playlist_model_get_n_columns;
    iface->get_column_type = ui_playlist_model_get_column_type;
    iface->get_iter        = ui_playlist_model_get_iter;
    iface->get_path        = ui_playlist_model_get_path;
    iface->get_value       = ui_playlist_model_get_value;
    iface->iter_next       = ui_playlist_model_iter_next;
    iface->iter_children   = ui_playlist_model_iter_children;
    iface->iter_has_child  = ui_playlist_model_iter_has_child;
    iface->iter_n_children = ui_playlist_model_iter_n_children;
    iface->iter_nth_child  = ui_playlist_model_iter_nth_child;
    iface->iter_parent     = ui_playlist_model_iter_parent;
}

static void
ui_playlist_model_init(UiPlaylistModel *model)
{
    if (multi_column_view)
    {
        model->n_columns = PLAYLIST_N_MULTI_COLUMNS;

        model->column_types = g_new0(GType, PLAYLIST_N_MULTI_COLUMNS);
        model->column_types[PLAYLIST_MULTI_COLUMN_NUM] = G_TYPE_UINT;
        model->column_types[PLAYLIST_MULTI_COLUMN_ARTIST] = G_TYPE_STRING;
        model->column_types[PLAYLIST_MULTI_COLUMN_ALBUM] = G_TYPE_STRING;
        model->column_types[PLAYLIST_MULTI_COLUMN_TITLE] = G_TYPE_STRING;
        model->column_types[PLAYLIST_MULTI_COLUMN_TRACK_NUM] = G_TYPE_UINT;
        model->column_types[PLAYLIST_MULTI_COLUMN_QUEUED] = G_TYPE_STRING;
        model->column_types[PLAYLIST_MULTI_COLUMN_TIME] = G_TYPE_STRING;
        model->column_types[PLAYLIST_MULTI_COLUMN_WEIGHT] = PANGO_TYPE_WEIGHT;
    }
    else
    {
        model->n_columns = PLAYLIST_N_COLUMNS;

        model->column_types = g_new0(GType, PLAYLIST_N_COLUMNS);
        model->column_types[PLAYLIST_COLUMN_NUM] = G_TYPE_UINT;
        model->column_types[PLAYLIST_COLUMN_TEXT] = G_TYPE_STRING;
        model->column_types[PLAYLIST_COLUMN_QUEUED] = G_TYPE_STRING;
        model->column_types[PLAYLIST_COLUMN_TIME] = G_TYPE_STRING;
        model->column_types[PLAYLIST_COLUMN_WEIGHT] = PANGO_TYPE_WEIGHT;
    }

    model->num_rows = 0;

    model->stamp = g_random_int();  /* Random int to check whether an iter belongs to our model */
}

static void
ui_playlist_model_finalize(GObject *object)
{
    UiPlaylistModel * model = (UiPlaylistModel *) object;

    g_list_free (model->queue);
    g_free(model->column_types);

    (* parent_class->finalize) (object);
}

static GtkTreeModelFlags
ui_playlist_model_get_flags(GtkTreeModel *model)
{
    return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
ui_playlist_model_get_n_columns(GtkTreeModel *model)
{
    return ((UiPlaylistModel *) model)->n_columns;
}

static GType
ui_playlist_model_get_column_type(GtkTreeModel *model, gint index)
{
    g_return_val_if_fail((index < ((UiPlaylistModel *) model)->n_columns) &&
                         (index >= 0), G_TYPE_INVALID);

    return ((UiPlaylistModel *) model)->column_types[index];
}

static gboolean
ui_playlist_model_get_iter(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
    UiPlaylistModel *model;
    gint *indices, n, depth;

    g_assert(path != NULL);

    model = (UiPlaylistModel *) tree_model;

    indices = gtk_tree_path_get_indices(path);
    depth = gtk_tree_path_get_depth(path);

    /* top level, items have no children */
    if (depth != 1)
        return FALSE;

    n = indices[0];

    if (n >= model->num_rows || n < 0)
        return FALSE;

    iter->stamp = model->stamp;
    iter->user_data = GINT_TO_POINTER(n);
    iter->user_data2 = NULL;
    iter->user_data3 = NULL;

    return TRUE;
}


static GtkTreePath *
ui_playlist_model_get_path(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    GtkTreePath *path;

    g_return_val_if_fail(iter != NULL, NULL);

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, GPOINTER_TO_INT(iter->user_data));

    return path;
}

static void ui_playlist_model_get_value_time(UiPlaylistModel *model, GValue *value, gint position)
{
    gint length = aud_playlist_entry_get_length (model->playlist, position,
     TRUE) / 1000;
    gchar * len = g_strdup_printf("%02i:%02i", length / 60, length % 60);
    g_value_set_string(value, len);
    g_free(len);
}

static const gchar *ui_playlist_model_tuple_get_string(const Tuple *tuple, gint field)
{
    if (tuple == NULL)
        return NULL;

    return tuple_get_string(tuple, field, NULL);
}

static gint ui_playlist_model_tuple_get_int(const Tuple *tuple, gint field)
{
    if (tuple == NULL)
        return 0;

    return tuple_get_int(tuple, field, NULL);
}

static void
ui_playlist_model_get_value(GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value)
{
    UiPlaylistModel * model = (UiPlaylistModel *) tree_model;
    gint n, i;

    g_return_if_fail(iter != NULL);
    g_return_if_fail(column < model->n_columns);

    g_value_init(value, model->column_types[column]);

    n = GPOINTER_TO_INT(iter->user_data);

    if (n >= model->num_rows)
        g_return_if_reached();

    if (multi_column_view)
    {
        const Tuple * tu = aud_playlist_entry_get_tuple (model->playlist, n,
         TRUE);

        switch (column)
        {
            case PLAYLIST_MULTI_COLUMN_NUM:
                g_value_set_uint(value, n+1);
                break;
            case PLAYLIST_MULTI_COLUMN_ARTIST:
                g_value_set_string(value, ui_playlist_model_tuple_get_string(tu, FIELD_ARTIST));
                break;

            case PLAYLIST_MULTI_COLUMN_ALBUM:
                g_value_set_string(value, ui_playlist_model_tuple_get_string(tu, FIELD_ALBUM));
                break;

            case PLAYLIST_MULTI_COLUMN_TRACK_NUM:
                g_value_set_uint(value, ui_playlist_model_tuple_get_int(tu, FIELD_TRACK_NUMBER));
                break;

            case PLAYLIST_MULTI_COLUMN_TITLE:
            {
                const gchar *title = ui_playlist_model_tuple_get_string(tu, FIELD_TITLE);

                if (title == NULL)
                    g_value_set_string (value, aud_playlist_entry_get_title
                     (model->playlist, n, TRUE));
                else
                    g_value_set_string(value, title);
                break;
            }

            case PLAYLIST_MULTI_COLUMN_QUEUED:
                if ((i = aud_playlist_queue_find_entry (model->playlist, n)) < 0)
                    g_value_set_string (value, "");
                else
                    g_value_take_string (value, g_strdup_printf ("#%d", 1 + i));
                break;

            case PLAYLIST_MULTI_COLUMN_TIME:
                ui_playlist_model_get_value_time(model, value, n);
                break;

            case PLAYLIST_MULTI_COLUMN_WEIGHT:
                if (n == model->position)
                    g_value_set_enum(value, PANGO_WEIGHT_BOLD);
                else
                    g_value_set_enum(value, PANGO_WEIGHT_NORMAL);
                break;
            default:
                break;
        }
    }
    else
    {
        switch (column)
        {
            case PLAYLIST_COLUMN_NUM:
                g_value_set_uint(value, n+1);
                break;

            case PLAYLIST_COLUMN_TEXT:
                g_value_set_string (value, aud_playlist_entry_get_title
                 (model->playlist, n, TRUE));
                break;

            case PLAYLIST_COLUMN_QUEUED:
                if ((i = aud_playlist_queue_find_entry (model->playlist, n)) < 0)
                    g_value_set_string (value, "");
                else
                    g_value_take_string (value, g_strdup_printf ("#%d", 1 + i));
                break;

            case PLAYLIST_COLUMN_TIME:
                ui_playlist_model_get_value_time(model, value, n);
                break;
            case PLAYLIST_COLUMN_WEIGHT:
                if (n == model->position)
                    g_value_set_enum(value, PANGO_WEIGHT_BOLD);
                else
                    g_value_set_enum(value, PANGO_WEIGHT_NORMAL);
                break;
            default:
                break;
        }
    }
}

static gboolean
ui_playlist_model_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    UiPlaylistModel * model = (UiPlaylistModel *) tree_model;
    guint n;

    if (iter == NULL)
        return FALSE;

    n = GPOINTER_TO_INT(iter->user_data);

    if (n+1 >= model->num_rows)
        return FALSE;

    iter->user_data = GINT_TO_POINTER(n+1);
    iter->stamp = model->stamp;

    return TRUE;
}

static gboolean
ui_playlist_model_iter_children(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
    UiPlaylistModel * model = (UiPlaylistModel *) tree_model;

    /* this is a list, nodes have no children */
    if (parent)
        return FALSE;

    if (model->num_rows == 0)
        return FALSE;

    iter->stamp = model->stamp;
    iter->user_data = GINT_TO_POINTER(0);

    return TRUE;
}

static gboolean
ui_playlist_model_iter_has_child(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    return FALSE;
}

static gint
ui_playlist_model_iter_n_children(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    UiPlaylistModel * model = (UiPlaylistModel *) tree_model;

    /* return number of top-level rows */
    if (iter == NULL)
        return model->num_rows;

    return 0; /* nodes have no children */
}

static gboolean
ui_playlist_model_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
    UiPlaylistModel * model = (UiPlaylistModel *) tree_model;

    /* we have only top-level rows */
    if (parent)
        return FALSE;

    if (n >= model->num_rows)
        return FALSE;

    iter->stamp = model->stamp;
    iter->user_data = GINT_TO_POINTER(n);

    return TRUE;
}

static gboolean
ui_playlist_model_iter_parent(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
    return FALSE;
}

UiPlaylistModel *
ui_playlist_model_new(gint playlist)
{
    UiPlaylistModel *model;

    model = (UiPlaylistModel *) g_object_new (ui_playlist_model_get_type (),
     NULL);

    g_assert(model != NULL);

    model->playlist = playlist;
    model->num_rows = aud_playlist_entry_count(playlist);
    model->position = aud_playlist_get_position (playlist);
    model->queue = NULL;
    model->song_changed = FALSE;
    model->focus_changed = FALSE;

    return model;
}

gint ui_playlist_model_get_playlist (UiPlaylistModel * model)
{
    return model->playlist;
}

void ui_playlist_model_set_playlist (UiPlaylistModel * model, gint playlist)
{
    model->playlist = playlist;
}

static void
ui_playlist_model_row_changed(UiPlaylistModel *model, gint n)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, n);
    ui_playlist_model_get_iter(GTK_TREE_MODEL(model), &iter, path);

    gtk_tree_model_row_changed(GTK_TREE_MODEL(model), path, &iter);

    gtk_tree_path_free(path);
}

static void
ui_playlist_model_row_inserted(UiPlaylistModel *model, gint n)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, n);
    ui_playlist_model_get_iter(GTK_TREE_MODEL(model), &iter, path);

    gtk_tree_model_row_inserted(GTK_TREE_MODEL(model), path, &iter);

    gtk_tree_path_free(path);
}

static void
ui_playlist_model_row_deleted(UiPlaylistModel *model, gint n)
{
    GtkTreePath *path;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, n);

    gtk_tree_model_row_deleted(GTK_TREE_MODEL(model), path);

    gtk_tree_path_free(path);
}

static void update_position (GtkTreeView * tree)
{
    UiPlaylistModel * model = (UiPlaylistModel *) gtk_tree_view_get_model (tree);

    if (model->position >= 0)
        ui_playlist_model_row_changed (model, model->position); /* remove bold */

    model->position = aud_playlist_get_position (model->playlist);

    if (model->position >= 0)
        ui_playlist_model_row_changed (model, model->position); /* set bold */

    treeview_set_focus (tree, model->position);
}

void treeview_update_position (GtkTreeView * tree)
{
    UiPlaylistModel * model = (UiPlaylistModel *) gtk_tree_view_get_model (tree);

    aud_playlist_select_all (model->playlist, FALSE);

    gint song = aud_playlist_get_position (model->playlist);
    if (song >= 0)
        aud_playlist_entry_set_selected (model->playlist, song, TRUE);

    if (aud_playlist_update_pending ())
        model->song_changed = TRUE;
    else
        update_position (tree);
}

static void update_queue_row_changed (void * row, void * model)
{
    ui_playlist_model_row_changed (model, GPOINTER_TO_INT (row));
}

static void update_queue (UiPlaylistModel * model)
{
    /* update previously queued rows */
    g_list_foreach (model->queue, update_queue_row_changed, model);

    g_list_free (model->queue);
    model->queue = NULL;

    for (gint i = aud_playlist_queue_count (model->playlist); i --; )
        model->queue = g_list_prepend (model->queue, GINT_TO_POINTER
         (aud_playlist_queue_get_entry (model->playlist, i)));

    /* update currently queued rows */
    g_list_foreach (model->queue, update_queue_row_changed, model);
}

void treeview_update (GtkTreeView * tree, gint type, gint at, gint count)
{
    UiPlaylistModel * model = (UiPlaylistModel *) gtk_tree_view_get_model (tree);

    ui_playlist_widget_block_updates ((GtkWidget *) tree, TRUE);

    if (type >= PLAYLIST_UPDATE_STRUCTURE)
    {
        gint entries = aud_playlist_entry_count (model->playlist);

        for (; model->num_rows < entries; model->num_rows ++)
            ui_playlist_model_row_inserted (model, at);

        for (; model->num_rows > entries; model->num_rows --)
            ui_playlist_model_row_deleted (model, at);

        model->position = aud_playlist_get_position (model->playlist);
    }

    if (type >= PLAYLIST_UPDATE_METADATA)
    {
        for (gint i = 0; i < count; i ++)
            ui_playlist_model_row_changed (model, at + i);
    }

    treeview_refresh_selected (tree, at, count);
    update_queue (model);

    if (model->song_changed)
    {
        update_position (tree);
        model->song_changed = FALSE;
    }

    if (model->focus_changed)
    {
        treeview_set_focus_now (tree, model->focus);
        model->focus_changed = FALSE;
    }

    ui_playlist_widget_block_updates ((GtkWidget *) tree, FALSE);

    gtk_tree_view_column_set_visible (g_object_get_data ((GObject *) tree,
     "number column"), aud_cfg->show_numbers_in_pl);
}

void treeview_set_focus_on_update (GtkTreeView * tree, gint focus)
{
    UiPlaylistModel * model = (UiPlaylistModel *) gtk_tree_view_get_model (tree);
    model->focus_changed = TRUE;
    model->focus = focus;
}
