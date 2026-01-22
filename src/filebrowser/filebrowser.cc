/*
 * File Browser Plugin for Audacious
 * Copyright 2025 Thomas Lange
 *
 * Based on the implementation for Qmmp
 * Copyright 2013-2025 Ilya Kotov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <string.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

#define AUD_GLIB_INTEGRATION
#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/multihash.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>
#include <libaudcore/vfs.h>
#include <libaudgui/gtk-compat.h>

#define CFG_ID "filebrowser"
#define CFG_FILE_PATH "file_path"

class FileBrowser : public GeneralPlugin
{
public:
    static const char about[];

    static constexpr PluginInfo info = {N_("File Browser"), PACKAGE, about,
                                        nullptr, PluginGLibOnly};

    constexpr FileBrowser() : GeneralPlugin(info, false) {}

    bool init() override;
    void cleanup() override;

    void * get_gtk_widget() override;
    int take_message(const char * code, const void * data, int size) override;
};

EXPORT FileBrowser aud_plugin_instance;

const char FileBrowser::about[] =
 N_("A dockable plugin that can be used to browse folders for music files. "
    "Right-click on a folder or file for supported actions.\n\n"
    "Copyright 2025 Thomas Lange\n\n"
    "Based on the implementation for Qmmp\n"
    "Copyright 2021-2025 Ilya Kotov");

enum
{
    COL_ICON,
    COL_NAME,
    COL_NAME_LOWER,
    COL_FILE,
    COL_IS_DIR,
    COL_LOADED,
    N_COLUMNS
};

static GtkWidget * nav_button;
static GtkEntry * filter_entry;
static GtkTreeView * tree_view;
static GtkTreeStore * tree_store;
static String filter_text;
static String current_path;
static SimpleHash<String, String> extension_dict;

static void init_music_directory()
{
    current_path = aud_get_str(CFG_ID, CFG_FILE_PATH);
    if (VFSFile::test_file(current_path, VFS_IS_DIR))
        return;

    const char * music_dir = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC);
    current_path = String(music_dir ? music_dir : g_get_home_dir());
}

static void init_extension_dict()
{
    for (const char * ext : aud_plugin_get_supported_extensions())
    {
        if (!ext)
            break;

        String extension = String(ext);
        extension_dict.add(extension, std::move(extension));
    }
}

static bool is_file_supported(GFile * file)
{
    CharPtr uri(g_file_get_uri(file));
    StringBuf ext = uri_get_extension(uri);
    return ext && extension_dict.lookup(String(ext));
}

static void append_tree_row(GFile * source, GFileInfo * info, GtkTreeIter * parent)
{
    if (g_file_info_get_is_hidden(info))
        return;

    const char * name = g_file_info_get_name(info);
    GFile * file = g_file_get_child(source, name);
    GFileType type = g_file_info_get_file_type(info);
    gboolean is_dir = type == G_FILE_TYPE_DIRECTORY;

    if (!is_dir && !is_file_supported(file))
    {
        g_object_unref(file);
        return;
    }

    GIcon * icon = g_file_info_get_icon(info);
    StringBuf name_lower = str_tolower_utf8(name);

    GtkTreeIter iter;
    gtk_tree_store_append(tree_store, &iter, parent);
    gtk_tree_store_set(tree_store, &iter,
                       COL_ICON, icon,
                       COL_NAME, name,
                       COL_NAME_LOWER, (const char *)name_lower,
                       COL_FILE, file,
                       COL_IS_DIR, is_dir,
                       COL_LOADED, false,
                       -1);

    /* Append dummy child to show an expander */
    if (is_dir)
    {
        GtkTreeIter dummy;
        gtk_tree_store_append(tree_store, &dummy, &iter);
        gtk_tree_store_set(tree_store, &dummy,
                           COL_ICON, nullptr,
                           COL_NAME, "",
                           COL_NAME_LOWER, "",
                           COL_FILE, nullptr,
                           COL_IS_DIR, false,
                           COL_LOADED, true,
                           -1);
    }

    g_object_unref(file);
}

static void enumerate_children_cb(GObject * source, GAsyncResult * res, void * data)
{
    GFile * file = (GFile *)source;
    GtkTreeRowReference * row_ref = (GtkTreeRowReference *)data;
    GtkTreeIter parent;
    GtkTreeIter * parent_ptr = nullptr;
    GError * error = nullptr;

    GFileEnumerator * enumerator = g_file_enumerate_children_finish(file, res, &error);

    if (!enumerator)
    {
        AUDERR("%s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    if (row_ref)
    {
        GtkTreePath * path = gtk_tree_row_reference_get_path(row_ref);
        if (!path)
            goto cleanup_enum;

        if (!gtk_tree_model_get_iter((GtkTreeModel *)tree_store, &parent, path))
        {
            gtk_tree_path_free(path);
            goto cleanup_enum;
        }

        parent_ptr = &parent;
        gtk_tree_path_free(path);
    }

    GFileInfo * info;
    while ((info = g_file_enumerator_next_file(enumerator, nullptr, nullptr)))
    {
        append_tree_row(file, info, parent_ptr);
        g_object_unref(info);
    }

    if (row_ref)
    {
        GtkTreePath * store_path = gtk_tree_row_reference_get_path(row_ref);
        gtk_tree_store_set(tree_store, &parent, COL_LOADED, true, -1);

        if (store_path)
        {
            GtkTreeModel * model = gtk_tree_view_get_model(tree_view);
            GtkTreePath * view_path =
                gtk_tree_model_filter_convert_child_path_to_path(
                    (GtkTreeModelFilter *)model, store_path);

            if (view_path)
            {
                gtk_tree_view_expand_row(tree_view, view_path, false);
                gtk_tree_path_free(view_path);
            }

            gtk_tree_path_free(store_path);
        }
    }

cleanup_enum:
    g_object_unref(enumerator);
cleanup:
    gtk_tree_row_reference_free(row_ref);
    g_object_unref(file);
}

static void load_dir_async(GFile * file, GtkTreeRowReference * row_ref)
{
    g_file_enumerate_children_async(
        (GFile *)g_object_ref(file),
        G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," G_FILE_ATTRIBUTE_STANDARD_ICON,
        G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, nullptr,
        enumerate_children_cb, row_ref);
}

static void load_children_async(GtkTreeIter * iter)
{
    GtkTreeModel * model = (GtkTreeModel *)tree_store;
    GFile * file = nullptr;
    gboolean loaded = false;

    gtk_tree_model_get(model, iter, COL_FILE, &file, COL_LOADED, &loaded, -1);

    if (!file || loaded)
    {
        if (file)
            g_object_unref(file);
        return;
    }

    GtkTreeIter dummy;
    if (gtk_tree_model_iter_children(model, &dummy, iter))
        gtk_tree_store_remove(tree_store, &dummy);

    GtkTreePath * path = gtk_tree_model_get_path(model, iter);
    GtkTreeRowReference * row_ref = gtk_tree_row_reference_new(model, path);
    gtk_tree_path_free(path);

    load_dir_async(file, row_ref);
    g_object_unref(file);
}

static gboolean load_child_dirs_idle_cb(void * data)
{
    load_children_async((GtkTreeIter *)data);
    return G_SOURCE_REMOVE;
}

static gboolean load_toplevel_dir_idle_cb(void * data)
{
    load_dir_async((GFile *)data, nullptr);
    return G_SOURCE_REMOVE;
}

static void add_selected_items(bool play)
{
    Playlist list = Playlist::active_playlist();
    Index<PlaylistAddItem> items;

    GtkTreeModel * model = nullptr;
    GtkTreeSelection * selection = gtk_tree_view_get_selection(tree_view);
    GList * rows = gtk_tree_selection_get_selected_rows(selection, &model);

    for (GList * l = rows; l; l = l->next)
    {
        GtkTreePath * path = (GtkTreePath *)l->data;
        GtkTreeIter iter;

        if (!gtk_tree_model_get_iter(model, &iter, path))
            continue;

        GFile * file = nullptr;
        gtk_tree_model_get(model, &iter, COL_FILE, &file, -1);
        CharPtr uri(g_file_get_uri(file));
        items.append(String(uri));
        g_object_unref(file);
    }

    list.insert_items(-1, std::move(items), play);
    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
}

static void replace_playlist()
{
    Playlist::temporary_playlist().activate();
    add_selected_items(true);
}

static void create_playlist()
{
    Playlist::new_playlist();
    add_selected_items(false);
}

static void add_to_playlist()
{
    add_selected_items(false);
}

static void open_uri(const char * uri, VFSFileTest sanity_check)
{
    GError * error = nullptr;

    if (!VFSFile::test_file(uri, sanity_check))
    {
        AUDERR("%s is not a file or directory.\n", uri);
        return;
    }

#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_show_uri_on_window(nullptr, uri, GDK_CURRENT_TIME, &error);
#else
    gtk_show_uri(gdk_screen_get_default(), uri, GDK_CURRENT_TIME, &error);
#endif

    if (error)
    {
        aud_ui_show_error(error->message);
        g_error_free(error);
    }
}

static void open_folder(GtkMenuItem * item, void * data)
{
    const char * uri = (const char *)data;
    open_uri(uri, VFS_IS_DIR);
}

static void open_cover(GtkMenuItem * item, void * data)
{
    const char * uri = (const char *)data;
    open_uri(uri, VFS_IS_REGULAR);
}

static bool is_image_file(const char * filename)
{
    static constexpr const char * extensions[] = {
        ".jpg", ".jpeg", ".png", ".webp"
    };

    for (const char * ext : extensions)
    {
        if (str_has_suffix_nocase(filename, ext))
            return true;
    }

    return false;
}

static bool check_cover_file(GFileInfo * info, GFile * search_dir,
                             const Index<String> & cover_names,
                             String & out_cover_uri)
{
    if (g_file_info_get_file_type(info) != G_FILE_TYPE_REGULAR)
        return false;

    const char * filename = g_file_info_get_name(info);
    if (!is_image_file(filename))
        return false;

    StringBuf base_name = str_copy(filename);
    char * dot = strrchr(base_name, '.');
    if (dot)
        *dot = '\0';

    for (const String & cover_name : cover_names)
    {
        if (strcmp_nocase(base_name, cover_name))
            continue;

        GFile * child = g_file_get_child(search_dir, filename);
        CharPtr uri(g_file_get_uri(child));
        out_cover_uri = String(uri);
        g_object_unref(child);
        return true;
    }

    return false;
}

static bool search_cover(GFile * search_dir, String & out_cover_uri)
{
    out_cover_uri = String();
    bool cover_found = false;

    GFileEnumerator * enumerator = g_file_enumerate_children(
        search_dir,
        G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
        G_FILE_QUERY_INFO_NONE, nullptr, nullptr);

    if (!enumerator)
        return cover_found;

    String cover_names_csv = aud_get_str("cover_name_include");
    Index<String> cover_names = str_list_to_index(cover_names_csv, ",");

    GFileInfo * info;
    while ((info = g_file_enumerator_next_file(enumerator, nullptr, nullptr)))
    {
        cover_found = check_cover_file(info, search_dir, cover_names, out_cover_uri);
        g_object_unref(info);
        if (cover_found)
            break;
    }

    g_object_unref(enumerator);
    return cover_found;
}

/* We still use GtkImageMenuItem until there is a good alternative */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void menu_append_item(const char * text, const char * icon,
                             GCallback callback, char * data, GtkWidget * menu)
{
    GtkWidget * item = gtk_image_menu_item_new_with_mnemonic(text);

    if (icon)
    {
        GtkWidget * image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image((GtkImageMenuItem *)item, image);
    }

    if (callback && data)
        g_signal_connect_data(item, "activate", callback, data,
                              (GClosureNotify)g_free, (GConnectFlags)0);
    else if (callback)
        g_signal_connect(item, "activate", callback, nullptr);

    gtk_menu_shell_append((GtkMenuShell *)menu, item);
}

G_GNUC_END_IGNORE_DEPRECATIONS

static void menu_append_separator_item(GtkWidget * menu)
{
    gtk_menu_shell_append((GtkMenuShell *)menu, gtk_separator_menu_item_new());
}

static void menu_append_open_folder_item(GtkWidget * menu, const char * uri)
{
    menu_append_item(_("_Open Folder Externally"), "folder",
                     (GCallback)open_folder, g_strdup(uri), menu);
}

static void menu_append_open_cover_item(GtkWidget * menu, const char * uri)
{
    menu_append_item(_("Open Co_ver Art"), "image-x-generic",
                     (GCallback)open_cover, g_strdup(uri), menu);
}

static GFile * get_containing_directory(GFile * file, bool is_dir)
{
    if (is_dir || !file)
        return file;

    GFile * parent = g_file_get_parent(file);
    return parent ? parent : file;
}

static void menu_append_optional_items(GtkWidget * menu)
{
    GtkTreeModel * model = nullptr;
    GtkTreeSelection * selection = gtk_tree_view_get_selection(tree_view);
    GList * rows = gtk_tree_selection_get_selected_rows(selection, &model);

    if (!rows)
    {
        menu_append_separator_item(menu);
        menu_append_open_folder_item(menu, filename_to_uri(current_path));
        return;
    }

    if (g_list_length(rows) > 1)
    {
        g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
        return;
    }

    GtkTreeIter iter;
    GtkTreePath * path = (GtkTreePath *)rows->data;

    if (gtk_tree_model_get_iter(model, &iter, path))
    {
        GFile * file = nullptr;
        gboolean is_dir = false;

        gtk_tree_model_get(model, &iter, COL_FILE, &file, COL_IS_DIR, &is_dir, -1);

        GFile * dir = get_containing_directory(file, is_dir);
        CharPtr dir_uri(g_file_get_uri(dir));

        menu_append_separator_item(menu);
        menu_append_open_folder_item(menu, g_strdup(dir_uri));

        String cover_uri;
        if (search_cover(dir, cover_uri))
            menu_append_open_cover_item(menu, g_strdup(cover_uri));

        if (dir != file)
            g_object_unref(dir);
        g_object_unref(file);
    }

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
}

static void show_context_menu(GdkEventButton * event)
{
    GtkWidget * menu = gtk_menu_new();
    menu_append_item(_("_Play"), "media-playback-start",
                     (GCallback)replace_playlist, nullptr, menu);
    menu_append_item(_("_Create Playlist"), "document-new",
                     (GCallback)create_playlist, nullptr, menu);
    menu_append_item(_("_Add to Playlist"), "list-add",
                     (GCallback)add_to_playlist, nullptr, menu);
    menu_append_optional_items(menu);
    gtk_widget_show_all(menu);

#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_menu_popup_at_pointer((GtkMenu *)menu, (const GdkEvent *)event);
#else
    gtk_menu_popup((GtkMenu *)menu, nullptr, nullptr, nullptr, nullptr,
                   event->button, event->time);
#endif
}

static void set_entry_placeholder_text(const char * dir_name)
{
#ifdef USE_GTK3
    StringBuf text = (!dir_name || *dir_name == '\0')
        ? str_copy(_("Search"))
        : str_printf(_("Search in %s"), dir_name);
    gtk_entry_set_placeholder_text(filter_entry, text);
#endif
}

static void navigate_to_directory(const char * filepath)
{
    if (!VFSFile::test_file(filepath, VFSFileTest::VFS_IS_DIR))
    {
        AUDERR("%s is not a directory.\n", filepath);
        return;
    }

    bool is_root = !filename_get_parent(filepath);
    gtk_widget_set_sensitive(nav_button, !is_root);
    gtk_entry_set_text(filter_entry, "");
    gtk_tree_store_clear(tree_store);

    GFile * target_dir = g_file_new_for_path(filepath);
    CharPtr dir_name(g_file_get_basename(target_dir));
    set_entry_placeholder_text(dir_name);
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, load_toplevel_dir_idle_cb,
                    g_object_ref(target_dir), g_object_unref);
    g_object_unref(target_dir);

    aud_set_str(CFG_ID, CFG_FILE_PATH, filepath);
    current_path = String(filepath);
}

static gboolean treeview_button_press_cb(GtkWidget * widget, GdkEventButton * event, void * data)
{
    if (event->type != GDK_BUTTON_PRESS || event->button != 3)
        return false;

    GtkTreePath * path = nullptr;

    if (gtk_tree_view_get_path_at_pos(tree_view, event->x, event->y, &path,
                                      nullptr, nullptr, nullptr))
    {
        GtkTreeSelection * selection = gtk_tree_view_get_selection(tree_view);

        if (!gtk_tree_selection_path_is_selected(selection, path))
            gtk_tree_view_set_cursor(tree_view, path, nullptr, false);
    }

    gtk_tree_path_free(path);
    gtk_widget_grab_focus(widget);
    show_context_menu(event);
    return true;
}

static void row_activated_cb(GtkTreeView * view, GtkTreePath * path,
                             GtkTreeViewColumn * column, void * data)
{
    GtkTreeIter iter;
    GtkTreeModel * model = gtk_tree_view_get_model(view);

    if (!gtk_tree_model_get_iter(model, &iter, path))
        return;

    GFile * file = nullptr;
    gboolean is_dir = false;

    gtk_tree_model_get(model, &iter, COL_FILE, &file, COL_IS_DIR, &is_dir, -1);

    if (is_dir)
    {
        CharPtr filepath(g_file_get_path(file));
        navigate_to_directory(filepath);
    }
    else
        replace_playlist();

    g_object_unref(file);
}

static gboolean test_expand_row_cb(GtkTreeView * view, GtkTreeIter * iter,
                                   GtkTreePath * path, void * data)
{
    GtkTreeIter store_iter;
    GtkTreeModel * model = gtk_tree_view_get_model(view);

    gtk_tree_model_filter_convert_iter_to_child_iter(
        (GtkTreeModelFilter *)model, &store_iter, iter);

    gboolean is_dir = false;
    gboolean loaded = false;

    gtk_tree_model_get((GtkTreeModel *)tree_store, &store_iter,
                       COL_IS_DIR, &is_dir, COL_LOADED, &loaded, -1);

    /* Never expand non-directories */
    if (!is_dir)
        return true;

    /* Already loaded, allow normal expansion */
    if (loaded)
        return false;

    /* Do NOT touch the model here, defer async loading */
    GtkTreeIter * iter_copy = g_new(GtkTreeIter, 1);
    *iter_copy = store_iter;
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, load_child_dirs_idle_cb,
                    iter_copy, g_free);

    /* Block expansion for now, the row gets expanded later
     * programmatically when its children were loaded */
    return true;
}

static void drag_data_get_cb(GtkWidget * widget, GdkDragContext * context,
                             GtkSelectionData * selection_data, guint info,
                             guint time, void * data)
{
    GtkTreeModel * model = nullptr;
    GtkTreeSelection * selection = gtk_tree_view_get_selection(tree_view);
    GList * rows = gtk_tree_selection_get_selected_rows(selection, &model);
    GPtrArray * uris = g_ptr_array_new_with_free_func(g_free);

    for (GList * l = rows; l; l = l->next)
    {
        GtkTreeIter iter;
        GtkTreePath * path = (GtkTreePath *)l->data;

        if (!gtk_tree_model_get_iter(model, &iter, path))
            continue;

        GFile * file = nullptr;
        gtk_tree_model_get(model, &iter, COL_FILE, &file, -1);
        g_ptr_array_add(uris, g_file_get_uri(file));
        g_object_unref(file);
    }

    g_ptr_array_add(uris, nullptr); /* must be null-terminated */
    gtk_selection_data_set_uris(selection_data, (char **)uris->pdata);
    g_ptr_array_free(uris, true);

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
}

static void nav_button_clicked_cb(GtkButton * button)
{
    StringBuf parent = filename_get_parent(current_path);
    navigate_to_directory(parent);
}

static void filter_entry_changed_cb(GtkEntry * entry)
{
    const char * text = gtk_entry_get_text(entry);
    filter_text = String(str_tolower_utf8(text));

    const char * icon_name = text[0] ? "edit-clear" : nullptr;
    g_object_set(entry, "secondary-icon-name", icon_name, nullptr);

    GtkTreeModel * model = gtk_tree_view_get_model(tree_view);
    gtk_tree_model_filter_refilter((GtkTreeModelFilter *)model);
    gtk_tree_view_collapse_all(tree_view);
}

static void filter_icon_press_cb(GtkEntry * entry)
{
    gtk_entry_set_text(entry, "");
}

static int tree_view_sort_func(GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b, void * data)
{
    int result;
    char * name_a = nullptr, * name_b = nullptr;
    gboolean is_dir_a = false, is_dir_b = false;

    gtk_tree_model_get(model, a, COL_NAME, &name_a, COL_IS_DIR, &is_dir_a, -1);
    gtk_tree_model_get(model, b, COL_NAME, &name_b, COL_IS_DIR, &is_dir_b, -1);

    /* Show directories before files */
    if (is_dir_a != is_dir_b)
        result = is_dir_a ? -1 : 1;
    else
        result = g_utf8_collate(name_a, name_b);

    g_free(name_a);
    g_free(name_b);

    return result;
}

static gboolean tree_view_filter_func(GtkTreeModel * model, GtkTreeIter * iter, void * data)
{
    if (!filter_text || *filter_text == '\0')
        return true;

    GtkTreeIter parent;
    if (gtk_tree_model_iter_parent(model, &parent, iter))
        return true;

    char * name_lower = nullptr;
    gtk_tree_model_get(model, iter, COL_NAME_LOWER, &name_lower, -1);
    gboolean visible = (name_lower && strstr(name_lower, filter_text));
    g_free(name_lower);

    return visible;
}

static GtkTreeView * build_tree_view()
{
    tree_store = gtk_tree_store_new(N_COLUMNS, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_OBJECT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_sort_column_id((GtkTreeSortable *)tree_store, COL_NAME, GTK_SORT_ASCENDING);
    gtk_tree_sortable_set_sort_func((GtkTreeSortable *)tree_store, COL_NAME,
                                    tree_view_sort_func, nullptr, nullptr);

    GtkTreeModel * tree_model_filter = gtk_tree_model_filter_new((GtkTreeModel *)tree_store, nullptr);
    gtk_tree_model_filter_set_visible_func((GtkTreeModelFilter *)tree_model_filter,
                                           tree_view_filter_func, nullptr, nullptr);

    GtkTreeView * treeview = (GtkTreeView *)gtk_tree_view_new_with_model(tree_model_filter);
    gtk_tree_view_set_fixed_height_mode(treeview, true);
    gtk_tree_view_set_headers_visible(treeview, false);
    g_object_unref(tree_store); /* GtkTreeView holds a reference now */

    GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    GtkTreeViewColumn * column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_spacing(column, 5);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);

    GtkCellRenderer * icon_renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, icon_renderer, false);
    gtk_tree_view_column_add_attribute(column, icon_renderer, "gicon", COL_ICON);

    GtkCellRenderer * text_renderer = gtk_cell_renderer_text_new();
    g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
    gtk_tree_view_column_pack_start(column, text_renderer, true);
    gtk_tree_view_column_add_attribute(column, text_renderer, "text", COL_NAME);

    gtk_tree_view_append_column(treeview, column);

    return treeview;
}

bool FileBrowser::init()
{
    init_music_directory();
    init_extension_dict();
    return true;
}

void FileBrowser::cleanup()
{
    g_clear_object(&tree_store);
    filter_text = String();
    current_path = String();
    extension_dict.clear();
}

void * FileBrowser::get_gtk_widget()
{
    GtkWidget * hbox = audgui_hbox_new(0);
    GtkWidget * vbox = audgui_vbox_new(6);

    nav_button = gtk_button_new();
    GtkWidget * image = gtk_image_new_from_icon_name("go-up", GTK_ICON_SIZE_MENU);
    gtk_button_set_image((GtkButton *)nav_button, image);
    gtk_button_set_relief((GtkButton *)nav_button, GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(nav_button, _("Show the parent folder"));
    g_signal_connect(nav_button, "clicked", (GCallback)nav_button_clicked_cb, nullptr);
    g_signal_connect(nav_button, "destroy", (GCallback)gtk_widget_destroyed, &nav_button);
    gtk_box_pack_start((GtkBox *)hbox, nav_button, false, false, 0);

    filter_entry = (GtkEntry *)gtk_entry_new();
    g_signal_connect(filter_entry, "changed", (GCallback)filter_entry_changed_cb, nullptr);
    g_signal_connect(filter_entry, "icon-press", (GCallback)filter_icon_press_cb, nullptr);
    g_signal_connect(filter_entry, "destroy", (GCallback)gtk_widget_destroyed, &filter_entry);
    gtk_box_pack_end((GtkBox *)hbox, (GtkWidget *)filter_entry, true, true, 0);

    tree_view = build_tree_view();
    g_signal_connect(tree_view, "button-press-event", (GCallback)treeview_button_press_cb, nullptr);
    g_signal_connect(tree_view, "row-activated", (GCallback)row_activated_cb, nullptr);
    g_signal_connect(tree_view, "test-expand-row", (GCallback)test_expand_row_cb, nullptr);
    g_signal_connect(tree_view, "destroy", (GCallback)gtk_widget_destroyed, &tree_view);

    GtkTargetEntry dnd_target = {(char *)"text/uri-list", 0, 0};
    gtk_drag_source_set((GtkWidget *)tree_view, GDK_BUTTON1_MASK, &dnd_target, 1, GDK_ACTION_COPY);
    g_signal_connect(tree_view, "drag-data-get", (GCallback)drag_data_get_cb, nullptr);

    GtkWidget * scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled,
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add((GtkContainer *)scrolled, (GtkWidget *)tree_view);
    gtk_box_pack_start((GtkBox *)vbox, hbox, false, false, 0);
    gtk_box_pack_end((GtkBox *)vbox, scrolled, true, true, 0);

    GtkWidget * frame = gtk_frame_new(nullptr);
    gtk_frame_set_shadow_type((GtkFrame *)frame, GTK_SHADOW_IN);
    gtk_container_add((GtkContainer *)frame, vbox);

    navigate_to_directory(current_path);

    return frame;
}

int FileBrowser::take_message(const char * code, const void * data, int size)
{
    if (!strcmp(code, "grab focus") && tree_view)
    {
        gtk_widget_grab_focus((GtkWidget *)tree_view);
        return 0;
    }

    return -1;
}
