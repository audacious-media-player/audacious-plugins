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

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui-gtk.h>

#include "plugin.h"
#include "skin.h"
#include "skinselector.h"
#include "util.h"
#include "view.h"

enum SkinViewCols {
    SKIN_VIEW_COL_PREVIEW,
    SKIN_VIEW_COL_FORMATTEDNAME,
    SKIN_VIEW_COL_NAME,
    SKIN_VIEW_N_COLS
};

struct SkinNode {
    String name, desc, path;
};

static Index<SkinNode> skinlist;

static void skin_view_on_cursor_changed (GtkTreeView * treeview);

static AudguiPixbuf skin_get_preview (const char * path)
{
    AudguiPixbuf preview;

    StringBuf archive_path;
    if (file_is_archive (path))
    {
        archive_path.steal (archive_decompress (path));
        if (! archive_path)
            return preview;

        path = archive_path;
    }

    StringBuf preview_path = skin_pixmap_locate (path, "main");
    if (preview_path)
        preview.capture (gdk_pixbuf_new_from_file (preview_path, nullptr));

    if (archive_path)
        del_directory (archive_path);

    return preview;
}

static AudguiPixbuf skin_get_thumbnail (const char * path)
{
    StringBuf base = filename_get_base (path);
    base.insert (-1, ".png");

    StringBuf thumbname = filename_build ({skins_get_skin_thumb_dir (), base});
    AudguiPixbuf thumb;

    if (g_file_test (thumbname, G_FILE_TEST_EXISTS))
        thumb.capture (gdk_pixbuf_new_from_file (thumbname, nullptr));

    if (! thumb)
    {
        thumb = skin_get_preview (path);

        if (thumb)
        {
            make_directory (skins_get_skin_thumb_dir ());
            gdk_pixbuf_save (thumb.get (), thumbname, "png", nullptr, nullptr);
        }
    }

    if (thumb)
        audgui_pixbuf_scale_within (thumb, audgui_get_dpi () * 3 / 2);

    return thumb;
}

static void scan_skindir_func (const char * path, const char * basename)
{
    if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
        if (file_is_archive (path))
            skinlist.append (String (archive_basename (basename)),
             String (_("Archived Winamp 2.x skin")), String (path));
    }
    else if (g_file_test (path, G_FILE_TEST_IS_DIR))
        skinlist.append (String (basename),
         String (_("Unarchived Winamp 2.x skin")), String (path));
}

static void skinlist_update ()
{
    skinlist.clear ();

    const char * user_skin_dir = skins_get_user_skin_dir ();
    if (g_file_test (user_skin_dir, G_FILE_TEST_EXISTS))
        dir_foreach (user_skin_dir, scan_skindir_func);

    StringBuf path = filename_build ({aud_get_path (AudPath::DataDir), "Skins"});
    dir_foreach (path, scan_skindir_func);

    const char * skinsdir = getenv ("SKINSDIR");
    if (skinsdir)
    {
        for (const String & dir : str_list_to_index (skinsdir, ":"))
            dir_foreach (dir, scan_skindir_func);
    }

    skinlist.sort ([] (const SkinNode & a, const SkinNode & b)
        { return str_compare (a.name, b.name); });
}

void skin_view_update (GtkTreeView * treeview)
{
    g_signal_handlers_block_by_func (treeview, (void *) skin_view_on_cursor_changed, nullptr);

    auto store = (GtkListStore *) gtk_tree_view_get_model (treeview);
    gtk_list_store_clear (store);

    skinlist_update ();

    String current_path = aud_get_str ("skins", "skin");
    GtkTreePath * current_skin = nullptr;

    for (const SkinNode & node : skinlist)
    {
        AudguiPixbuf thumbnail = skin_get_thumbnail (node.path);
        StringBuf formattedname = str_concat ({"<big><b>", node.name,
         "</b></big>\n<i>", node.desc, "</i>"});

        GtkTreeIter iter;
        gtk_list_store_append (store, & iter);
        gtk_list_store_set (store, & iter,
         SKIN_VIEW_COL_PREVIEW, thumbnail.get (),
         SKIN_VIEW_COL_FORMATTEDNAME, (const char *) formattedname,
         SKIN_VIEW_COL_NAME, (const char *) node.name, -1);

        if (! current_skin && strstr (current_path, node.name))
            current_skin = gtk_tree_model_get_path ((GtkTreeModel *) store, & iter);
    }

    if (current_skin)
    {
        auto selection = gtk_tree_view_get_selection (treeview);
        gtk_tree_selection_select_path (selection, current_skin);
        gtk_tree_view_scroll_to_cell (treeview, current_skin, nullptr, true, 0.5, 0.5);
        gtk_tree_path_free (current_skin);
    }

    g_signal_handlers_unblock_by_func (treeview, (void *) skin_view_on_cursor_changed, nullptr);
}

static void skin_view_on_cursor_changed (GtkTreeView * treeview)
{
    GtkTreeModel * model;
    GtkTreeIter iter;

    auto selection = gtk_tree_view_get_selection (treeview);
    if (! gtk_tree_selection_get_selected (selection, & model, & iter))
        return;

    GtkTreePath * path = gtk_tree_model_get_path (model, & iter);
    int row = gtk_tree_path_get_indices (path)[0];
    g_return_if_fail (row >= 0 && row < skinlist.len ());
    gtk_tree_path_free (path);

    if (skin_load (skinlist[row].path))
        view_apply_skin ();
}

void skin_view_realize (GtkTreeView * treeview)
{
    gtk_widget_show_all ((GtkWidget *) treeview);

    gtk_tree_view_set_rules_hint (treeview, true);
    gtk_tree_view_set_headers_visible (treeview, false);

    GtkListStore * store = gtk_list_store_new (SKIN_VIEW_N_COLS, GDK_TYPE_PIXBUF,
     G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model (treeview, (GtkTreeModel *) store);
    g_object_unref (store);

    GtkTreeViewColumn * column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_spacing (column, 16);
    gtk_tree_view_append_column (treeview, column);

    GtkCellRenderer * renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, false);
    gtk_tree_view_column_set_attributes (column, renderer, "pixbuf",
     SKIN_VIEW_COL_PREVIEW, nullptr);

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, true);
    gtk_tree_view_column_set_attributes (column, renderer, "markup",
     SKIN_VIEW_COL_FORMATTEDNAME, nullptr);

    GtkTreeSelection * selection = gtk_tree_view_get_selection (treeview);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    g_signal_connect (treeview, "cursor-changed",
     (GCallback) skin_view_on_cursor_changed, nullptr);
}
