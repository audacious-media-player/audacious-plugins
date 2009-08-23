/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2009 Tomasz Moń <desowin@gmail.com>
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

#include <audacious/plugin.h>
#include "ui_playlist_model.h"

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

static void ui_playlist_model_associate_hooks(UiPlaylistModel *model);
static void ui_playlist_model_dissociate_hooks(UiPlaylistModel *model);

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
    model->n_columns = PLAYLIST_N_COLUMNS;

    model->column_types[PLAYLIST_COLUMN_NUM] = G_TYPE_UINT;
    model->column_types[PLAYLIST_COLUMN_TEXT] = G_TYPE_STRING;
    model->column_types[PLAYLIST_COLUMN_TIME] = G_TYPE_STRING;
    model->column_types[PLAYLIST_COLUMN_WEIGHT] = PANGO_TYPE_WEIGHT;

    model->num_rows = 0;

    model->stamp = g_random_int();  /* Random int to check whether an iter belongs to our model */
}

static void
ui_playlist_model_finalize(GObject *object)
{
    UiPlaylistModel *model = UI_PLAYLIST_MODEL(object);

    ui_playlist_model_dissociate_hooks(model);

    (* parent_class->finalize) (object);
}

static GtkTreeModelFlags
ui_playlist_model_get_flags(GtkTreeModel *model)
{
    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(model), (GtkTreeModelFlags)0);

    return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
ui_playlist_model_get_n_columns(GtkTreeModel *model)
{
    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(model), 0);

    return UI_PLAYLIST_MODEL(model)->n_columns;
}

static GType
ui_playlist_model_get_column_type(GtkTreeModel *model, gint index)
{
    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(model), G_TYPE_INVALID);
    g_return_val_if_fail((index < UI_PLAYLIST_MODEL(model)->n_columns) &&
                         (index >= 0), G_TYPE_INVALID);

    return UI_PLAYLIST_MODEL(model)->column_types[index];
}

static gboolean
ui_playlist_model_get_iter(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
    UiPlaylistModel *model;
    gint *indices, n, depth;

    g_assert(UI_IS_PLAYLIST_MODEL(tree_model));
    g_assert(path != NULL);

    model = UI_PLAYLIST_MODEL(tree_model);

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

    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(tree_model), NULL);
    g_return_val_if_fail(iter != NULL, NULL);

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, GPOINTER_TO_INT(iter->user_data));

    return path;
}

static void
ui_playlist_model_get_value(GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value)
{
    UiPlaylistModel *model;
    gint n;

    g_return_if_fail(UI_IS_PLAYLIST_MODEL(tree_model));
    g_return_if_fail(iter != NULL);
    g_return_if_fail(column < UI_PLAYLIST_MODEL(tree_model)->n_columns);

    model = UI_PLAYLIST_MODEL(tree_model);

    g_value_init(value, model->column_types[column]);

    n = GPOINTER_TO_INT(iter->user_data);

    if (n >= model->num_rows)
        g_return_if_reached();

    switch (column)
    {
      case PLAYLIST_COLUMN_NUM:
        g_value_set_uint(value, n+1);
        break;
      case PLAYLIST_COLUMN_TEXT:
        g_value_set_string(value, aud_playlist_entry_get_title(model->playlist, n));
        break;
      case PLAYLIST_COLUMN_TIME:
      {
        gint length = aud_playlist_entry_get_length(model->playlist, n) / 1000;
        gchar * len = g_strdup_printf("%02i:%02i", length / 60, length % 60);
        g_value_set_string(value, len);
        g_free(len);
        break;
      }
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

static gboolean
ui_playlist_model_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    UiPlaylistModel *model;
    guint n;

    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(tree_model), FALSE);

    if (iter == NULL)
        return FALSE;

    model = UI_PLAYLIST_MODEL(tree_model);

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
    UiPlaylistModel *model;

    /* this is a list, nodes have no children */
    if (parent)
        return FALSE;

    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(tree_model), FALSE);

    model = UI_PLAYLIST_MODEL(tree_model);

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
    UiPlaylistModel *model;

    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(tree_model), -1);

    model = UI_PLAYLIST_MODEL(tree_model);

    /* return number of top-level rows */
    if (iter == NULL)
        return model->num_rows;

    return 0; /* nodes have no children */
}

static gboolean
ui_playlist_model_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
    UiPlaylistModel *model;

    g_return_val_if_fail(UI_IS_PLAYLIST_MODEL(tree_model), FALSE);

    model = UI_PLAYLIST_MODEL(tree_model);

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

    model = (UiPlaylistModel *) g_object_new(UI_PLAYLIST_MODEL_TYPE, NULL);

    g_assert(model != NULL);

    model->playlist = playlist;
    model->num_rows = aud_playlist_entry_count(playlist);

    if (playlist == aud_playlist_get_active())
    {
        model->position = aud_playlist_get_position(playlist);
    }
    else
    {
        model->position = -1;
    }

    ui_playlist_model_associate_hooks(model);

    return model;
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

static void
ui_playlist_model_update_position(UiPlaylistModel *model, gint position)
{
    if (model->position != -1)
    {
        ui_playlist_model_row_changed(model, model->position); /* remove bold */
    }

    model->position = position;

    if (model->position != -1)
    {
        ui_playlist_model_row_changed(model, model->position); /* set bold */
    }
}

static void
ui_playlist_model_playlist_update(gpointer hook_data, gpointer user_data)
{
    UiPlaylistModel *model = UI_PLAYLIST_MODEL(user_data);
    gint type = GPOINTER_TO_INT(hook_data);
    gint position;

    if (model->playlist != aud_playlist_get_active())
        return;

    position = aud_playlist_get_position(model->playlist);
    if (position != model->position)
    {
        ui_playlist_model_update_position(model, position);
    }

    if (type == PLAYLIST_UPDATE_STRUCTURE)
    {
        gint changed_rows;
        changed_rows = aud_playlist_entry_count(model->playlist) - model->num_rows;

        if (changed_rows > 0)
        {
            /* entries added */
            while (changed_rows != 0)
            {
                ui_playlist_model_row_inserted(model, model->num_rows++);
                changed_rows--;
            }
        }
        else
        {
            /* entries removed */
            while (changed_rows != 0)
            {
                ui_playlist_model_row_deleted(model, --model->num_rows);
                changed_rows++;
            }
        }

        model->num_rows = aud_playlist_entry_count(model->playlist);
    }
}

static void
ui_playlist_model_playlist_delete(gpointer hook_data, gpointer user_data)
{
    UiPlaylistModel *model = UI_PLAYLIST_MODEL(user_data);
    gint playlist = GPOINTER_TO_INT(hook_data);

    if (model->playlist > playlist)
    {
        model->playlist--;
        return;
    }

    /* should happen only if GtkTreeView wasn't yet destroyed */
    if (model->playlist == playlist)
    {
        model->num_rows = 0;
        model->playlist = -1;
    }
}

static void
ui_playlist_model_associate_hooks(UiPlaylistModel *model)
{
    aud_hook_associate("playlist update", ui_playlist_model_playlist_update, model);
    aud_hook_associate("playlist delete", ui_playlist_model_playlist_delete, model);
}

static void
ui_playlist_model_dissociate_hooks(UiPlaylistModel *model)
{
    aud_hook_dissociate_full("playlist update", ui_playlist_model_playlist_update, model);
    aud_hook_dissociate_full("playlist delete", ui_playlist_model_playlist_delete, model);
}

