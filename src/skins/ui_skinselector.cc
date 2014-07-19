/*
 * ui_skinselector.c
 * Copyright 1998-2003 XMMS Development Team
 * Copyright 2003-2004 BMP Development Team
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <stdlib.h>
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui-gtk.h>

#include "plugin.h"
#include "ui_skin.h"
#include "ui_skinselector.h"
#include "util.h"

#define EXTENSION_TARGETS 7

static const char *ext_targets[EXTENSION_TARGETS] = { "bmp", "xpm", "png", "svg",
        "gif", "jpg", "jpeg" };

enum SkinViewCols {
    SKIN_VIEW_COL_PREVIEW,
    SKIN_VIEW_COL_FORMATTEDNAME,
    SKIN_VIEW_COL_NAME,
    SKIN_VIEW_N_COLS
};

typedef struct {
    char *name;
    char *desc;
    char *path;
    GTime *time;
} SkinNode;

static void skin_view_on_cursor_changed (GtkTreeView * treeview, void * data);

static GList *skinlist = nullptr;

static char *
get_thumbnail_filename(const char * path)
{
    char *basename, *pngname, *thumbname;

    g_return_val_if_fail(path != nullptr, nullptr);

    basename = g_path_get_basename(path);
    pngname = g_strconcat(basename, ".png", nullptr);

    thumbname = g_build_filename(skins_paths[SKINS_PATH_SKIN_THUMB_DIR],
                                 pngname, nullptr);

    g_free(basename);
    g_free(pngname);

    return thumbname;
}


static GdkPixbuf *
skin_get_preview(const char * path)
{
    GdkPixbuf *preview = nullptr;
    char *dec_path, *preview_path;
    gboolean is_archive = FALSE;
    int i = 0;
    char buf[60];			/* gives us lots of room */

    if (file_is_archive(path))
    {
        if (!(dec_path = archive_decompress(path)))
            return nullptr;

        is_archive = TRUE;
    }
    else
    {
        dec_path = g_strdup(path);
    }

    for (i = 0; i < EXTENSION_TARGETS; i++)
    {
        sprintf(buf, "main.%s", ext_targets[i]);

        if ((preview_path = find_file_case_path (dec_path, buf)) != nullptr)
            break;
    }

    if (preview_path)
    {
        preview = gdk_pixbuf_new_from_file(preview_path, nullptr);
        g_free(preview_path);
    }

    if (is_archive)
        del_directory(dec_path);

    g_free(dec_path);

    return preview;
}

static GdkPixbuf * skin_get_thumbnail (const char * path)
{
    char * thumbname = get_thumbnail_filename (path);
    GdkPixbuf * thumb = nullptr;

    if (g_file_test (thumbname, G_FILE_TEST_EXISTS))
    {
        thumb = gdk_pixbuf_new_from_file (thumbname, nullptr);
        if (thumb)
            goto DONE;
    }

    thumb = skin_get_preview (path);
    if (! thumb)
        goto DONE;

    audgui_pixbuf_scale_within (& thumb, 128);

    if (thumb)
        gdk_pixbuf_save (thumb, thumbname, "png", nullptr, nullptr);

DONE:
    g_free (thumbname);
    return thumb;
}

static void
skinlist_add(const char * filename)
{
    SkinNode *node;
    char *basename;

    g_return_if_fail(filename != nullptr);

    node = g_slice_new0(SkinNode);
    node->path = g_strdup(filename);

    basename = g_path_get_basename(filename);

    if (file_is_archive(filename)) {
        node->name = archive_basename(basename);
    node->desc = _("Archived Winamp 2.x skin");
        g_free(basename);
    }
    else {
        node->name = basename;
    node->desc = _("Unarchived Winamp 2.x skin");
    }

    skinlist = g_list_prepend(skinlist, node);
}

static gboolean
scan_skindir_func(const char * path, const char * basename, void * data)
{
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        if (file_is_archive(path)) {
            skinlist_add(path);
        }
    }
    else if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        skinlist_add(path);
    }

    return FALSE;
}

static void
scan_skindir(const char * path)
{
    GError *error = nullptr;

    g_return_if_fail(path != nullptr);

    if (path[0] == '.')
        return;

    if (!dir_foreach(path, scan_skindir_func, nullptr, &error)) {
        g_warning("Failed to open directory (%s): %s", path, error->message);
        g_error_free(error);
        return;
    }
}

static int skinlist_compare_func (const void * _a, const void * _b)
{
    const SkinNode * a = (SkinNode *) _a, * b = (SkinNode *) _b;
    g_return_val_if_fail (a && a->name, 1);
    g_return_val_if_fail (b && b->name, 1);
    return g_ascii_strcasecmp (a->name, b->name);
}

static void skin_free_func (void * _data)
{
    SkinNode * data = (SkinNode *) _data;
    g_return_if_fail(data != nullptr);
    g_free (data->name);
    g_free (data->path);
    g_slice_free(SkinNode, data);
}


static void
skinlist_clear(void)
{
    if (!skinlist)
        return;

    g_list_foreach(skinlist, (GFunc) skin_free_func, nullptr);
    g_list_free(skinlist);
    skinlist = nullptr;
}

static void
skinlist_update(void)
{
    char *skinsdir;

    skinlist_clear();

    if (g_file_test (skins_paths[SKINS_PATH_USER_SKIN_DIR], G_FILE_TEST_EXISTS))
        scan_skindir (skins_paths[SKINS_PATH_USER_SKIN_DIR]);

    char * path = g_strdup_printf ("%s/Skins", aud_get_path (AudPath::DataDir));
    scan_skindir (path);
    g_free (path);

    skinsdir = getenv("SKINSDIR");
    if (skinsdir) {
        char **dir_list = g_strsplit(skinsdir, ":", 0);
        char **dir;

        for (dir = dir_list; *dir; dir++)
            scan_skindir(*dir);
        g_strfreev(dir_list);
    }

    skinlist = g_list_sort(skinlist, skinlist_compare_func);

    g_assert(skinlist != nullptr);
}

void skin_view_update (GtkTreeView * treeview)
{
    GtkTreeSelection *selection = nullptr;
    GtkListStore *store;
    GtkTreeIter iter, iter_current_skin;
    gboolean have_current_skin = FALSE;
    GtkTreePath *path;

    GdkPixbuf *thumbnail;
    char *formattedname;
    char *name;
    GList *entry;

    g_signal_handlers_block_by_func (treeview, (void *) skin_view_on_cursor_changed, nullptr);

    store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));

    gtk_list_store_clear(store);

    skinlist_update();

    for (entry = skinlist; entry; entry = entry->next)
    {
        SkinNode * node = (SkinNode *) entry->data;

        thumbnail = skin_get_thumbnail (node->path);
        formattedname = g_strdup_printf ("<big><b>%s</b></big>\n<i>%s</i>",
         node->name, node->desc);
        name = node->name;

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           SKIN_VIEW_COL_PREVIEW, thumbnail,
                           SKIN_VIEW_COL_FORMATTEDNAME, formattedname,
                           SKIN_VIEW_COL_NAME, name, -1);
        if (thumbnail)
            g_object_unref(thumbnail);
        g_free(formattedname);

        if (g_strstr_len(active_skin->path,
                         strlen(active_skin->path), name) ) {
            iter_current_skin = iter;
            have_current_skin = TRUE;
        }
    }

    if (have_current_skin) {
        selection = gtk_tree_view_get_selection(treeview);
        gtk_tree_selection_select_iter(selection, &iter_current_skin);

        path = gtk_tree_model_get_path(GTK_TREE_MODEL(store),
                                       &iter_current_skin);
        gtk_tree_view_scroll_to_cell(treeview, path, nullptr, TRUE, 0.5, 0.5);
        gtk_tree_path_free(path);
    }

    g_signal_handlers_unblock_by_func (treeview, (void *) skin_view_on_cursor_changed, nullptr);
}


static void
skin_view_on_cursor_changed(GtkTreeView * treeview,
                            void * data)
{
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    GList *node;
    char *name;
    char *comp = nullptr;

    selection = gtk_tree_view_get_selection(treeview);

    /* workaround for Gnome bug #679291 */
    if (! selection)
        return;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, SKIN_VIEW_COL_NAME, &name, -1);

    for (node = skinlist; node; node = g_list_next(node)) {
        comp = ((SkinNode *) node->data)->path;
        if (g_strrstr(comp, name))
            break;
    }

    g_free(name);

    active_skin_load (comp);
}


void
skin_view_realize(GtkTreeView * treeview)
{
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;

    gtk_widget_show_all(GTK_WIDGET(treeview));

    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

    store = gtk_list_store_new(SKIN_VIEW_N_COLS, GDK_TYPE_PIXBUF,
                               G_TYPE_STRING , G_TYPE_STRING);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));
    g_object_unref (store);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_spacing(column, 16);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview),
                                GTK_TREE_VIEW_COLUMN(column));

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, renderer, "pixbuf",
                                        SKIN_VIEW_COL_PREVIEW, nullptr);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_attributes(column, renderer, "markup",
                                        SKIN_VIEW_COL_FORMATTEDNAME, nullptr);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

    g_signal_connect(treeview, "cursor-changed",
                     G_CALLBACK(skin_view_on_cursor_changed), nullptr);
}
