/*
 * search-tool.c
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

#include <errno.h>
#include <string.h>

#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/list.h>

#include "config.h"

#define MAX_RESULTS 12
#define SEARCH_DELAY 300

enum {ARTIST, ALBUM, TITLE, FIELDS};

typedef struct item {
    int field;
    char * name, * folded;
    struct item * parent;
    GHashTable * children;
    GArray * matches;
} Item;

typedef struct {
    Index * items[FIELDS];
    int mask;
} SearchState;

static int playlist_id;
static char * * search_terms;

static GHashTable * added_table;
static GHashTable * database;
static Index * items;
static GArray * selection;

static bool_t adding;
static int search_source;

static GtkWidget * entry, * help_label, * wait_label, * scrolled, * results_list;

static void item_free (Item * item);

/* str_unref() may be a macro */
static void str_unref_cb (void * str)
{
    str_unref (str);
}

static Item * item_new (int field, char * name, Item * parent)
{
    Item * item = g_slice_new (Item);
    item->field = field;
    item->name = name;
    item->folded = g_utf8_casefold (name, -1);
    item->parent = parent;
    item->matches = g_array_new (FALSE, FALSE, sizeof (int));

    /* speed things up by using g_direct_equal() instead of g_str_equal()
       because identical pooled strings have the same pointer */
    if (field == TITLE)
        item->children = NULL;
    else
        item->children = g_hash_table_new_full (g_str_hash, g_direct_equal,
         NULL, (GDestroyNotify) item_free);

    return item;
}

static void item_free (Item * item)
{
    if (item->children)
        g_hash_table_destroy (item->children);

    str_unref (item->name);
    g_free (item->folded);
    g_array_free (item->matches, TRUE);
    g_slice_free (Item, item);
}

static void find_playlist (void)
{
    playlist_id = -1;

    for (int p = 0; playlist_id < 0 && p < aud_playlist_count (); p ++)
    {
        char * title = aud_playlist_get_title (p);

        if (! strcmp (title, _("Library")))
            playlist_id = aud_playlist_get_unique_id (p);

        str_unref (title);
    }
}

static int create_playlist (void)
{
    int list = aud_playlist_get_blank ();
    aud_playlist_set_title (list, _("Library"));
    aud_playlist_set_active (list);
    playlist_id = aud_playlist_get_unique_id (list);
    return list;
}

static int get_playlist (bool_t require_added, bool_t require_scanned)
{
    if (playlist_id < 0)
        return -1;

    int list = aud_playlist_by_unique_id (playlist_id);

    if (list < 0)
    {
        playlist_id = -1;
        return -1;
    }

    if (require_added && aud_playlist_add_in_progress (list))
        return -1;
    if (require_scanned && aud_playlist_scan_in_progress (list))
        return -1;

    return list;
}

static void set_search_phrase (const char * phrase)
{
    g_strfreev (search_terms);

    char * folded = g_utf8_casefold (phrase, -1);
    search_terms = g_strsplit (folded, " ", -1);
    g_free (folded);
}

static char * get_path (void)
{
    char * path = aud_get_string ("search-tool", "path");
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

    g_free (path);
    path = g_build_filename (g_get_home_dir (), "Music", NULL);
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

    g_free (path);
    return g_strdup (g_get_home_dir ());
}

static void destroy_added_table (void)
{
    if (added_table)
    {
        g_hash_table_destroy (added_table);
        added_table = NULL;
    }
}

static void destroy_database (void)
{
    if (items)
        index_delete (items, 0, index_count (items));

    if (database)
    {
        g_hash_table_destroy (database);
        database = NULL;
    }
}

static void create_database (int list)
{
    destroy_database ();

    /* speed things up by using g_direct_equal() instead of g_str_equal()
       because identical pooled strings have the same pointer */
    database = g_hash_table_new_full (g_str_hash, g_direct_equal, NULL,
     (GDestroyNotify) item_free);

    int entries = aud_playlist_entry_count (list);

    for (int e = 0; e < entries; e ++)
    {
        char * title, * artist, * album;
        Item * artist_item, * album_item, * title_item;

        aud_playlist_entry_describe (list, e, & title, & artist, & album, TRUE);

        if (! title)
        {
            str_unref (artist);
            str_unref (album);
            continue;
        }

        if (! artist)
            artist = str_get (_("Unknown Artist"));
        if (! album)
            album = str_get (_("Unknown Album"));

        artist_item = g_hash_table_lookup (database, artist);

        if (! artist_item)
        {
            /* item_new() takes ownership of reference to artist */
            artist_item = item_new (ARTIST, artist, NULL);
            g_hash_table_insert (database, artist, artist_item);
        }
        else
            str_unref (artist);

        g_array_append_val (artist_item->matches, e);

        album_item = g_hash_table_lookup (artist_item->children, album);

        if (! album_item)
        {
            /* item_new() takes ownership of reference to album */
            album_item = item_new (ALBUM, album, artist_item);
            g_hash_table_insert (artist_item->children, album, album_item);
        }
        else
            str_unref (album);

        g_array_append_val (album_item->matches, e);

        title_item = g_hash_table_lookup (album_item->children, title);

        if (! title_item)
        {
            /* item_new() takes ownership of reference to title */
            title_item = item_new (TITLE, title, album_item);
            g_hash_table_insert (album_item->children, title, title_item);
        }
        else
            str_unref (title);

        g_array_append_val (title_item->matches, e);
    }
}

static void search_cb (void * key, void * _item, void * _state)
{
    Item * item = _item;
    SearchState * state = _state;

    if (index_count (state->items[item->field]) > MAX_RESULTS)
        return;

    int oldmask = state->mask;

    for (int t = 0, bit = 1; search_terms[t]; t ++, bit <<= 1)
    {
        if (! (state->mask & bit))
            continue; /* skip term if it is already found */

        if (strstr (item->folded, search_terms[t]))
            state->mask &= ~bit; /* we found it */
        else if (! item->children)
            break; /* quit early if there are no children to search */
    }

    if (! state->mask)
        index_append (state->items[item->field], item);

    if (item->children)
        g_hash_table_foreach (item->children, search_cb, state);

    state->mask = oldmask;
}

static int item_compare (const void * _a, const void * _b)
{
    const Item * a = _a, * b = _b;
    return string_compare (a->name, b->name);
}

static void do_search (void)
{
    index_delete (items, 0, index_count (items));

    SearchState state;

    for (int f = 0; f < FIELDS; f ++)
        state.items[f] = index_new ();

    /* effectively limits number of search terms to 32 */
    state.mask = 0;
    for (int t = 0, bit = 1; search_terms[t]; t ++, bit <<= 1)
        state.mask |= bit;

    g_hash_table_foreach (database, search_cb, & state);

    for (int f = 0; f < FIELDS; f ++)
    {
        if (index_count (state.items[f]) <= MAX_RESULTS)
        {
            index_sort (state.items[f], item_compare);
            index_merge_append (items, state.items[f]);
        }

        index_free (state.items[f]);
    }

    g_array_set_size (selection, index_count (items));
    memset (selection->data, 0, selection->len);
    if (selection->len > 0)
        selection->data[0] = 1;
}

static bool_t filter_cb (const char * filename, void * unused)
{
    return added_table && ! g_hash_table_lookup_extended (added_table, filename, NULL, NULL);
}

static void begin_add (const char * path)
{
    int list = get_playlist (FALSE, FALSE);

    if (list < 0)
        list = create_playlist ();

    aud_set_string ("search-tool", "path", path);

    char * uri = filename_to_uri (path);
    g_return_if_fail (uri);
    char * prefix = g_str_has_suffix (uri, "/") ? g_strdup (uri) : g_strconcat (uri, "/", NULL);

    destroy_added_table ();

    /* speed things up by using g_direct_equal() instead of g_str_equal()
       because identical pooled strings have the same pointer */
    added_table = g_hash_table_new_full (g_str_hash, g_direct_equal, str_unref_cb, NULL);

    int entries = aud_playlist_entry_count (list);

    for (int entry = 0; entry < entries; entry ++)
    {
        char * filename = aud_playlist_entry_get_filename (list, entry);

        if (g_str_has_prefix (filename, prefix) && ! g_hash_table_lookup_extended
         (added_table, filename, NULL, NULL))
        {
            aud_playlist_entry_set_selected (list, entry, FALSE);
            g_hash_table_insert (added_table, filename, NULL);
        }
        else
        {
            aud_playlist_entry_set_selected (list, entry, TRUE);
            str_unref (filename);
        }
    }

    aud_playlist_delete_selected (list);
    aud_playlist_remove_failed (list);

    Index * add = index_new ();
    index_append (add, str_get (uri));
    aud_playlist_entry_insert_filtered (list, -1, add, NULL, filter_cb, NULL, FALSE);

    g_free (uri);
    g_free (prefix);
    adding = TRUE;
}

static void show_hide_widgets (void)
{
    if (! help_label || ! wait_label || ! scrolled)
        return;

    if (playlist_id < 0)
    {
        gtk_widget_hide (wait_label);
        gtk_widget_hide (scrolled);
        gtk_widget_show (help_label);
    }
    else
    {
        gtk_widget_hide (help_label);

        if (database)
        {
            gtk_widget_hide (wait_label);
            gtk_widget_show (scrolled);
        }
        else
        {
            gtk_widget_hide (scrolled);
            gtk_widget_show (wait_label);
        }
    }
}

static int search_timeout (void * unused)
{
    do_search ();

    if (results_list)
    {
        audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
        audgui_list_insert_rows (results_list, 0, index_count (items));
    }

    if (search_source)
    {
        g_source_remove (search_source);
        search_source = 0;
    }

    return FALSE;
}

static void schedule_search (void)
{
    if (search_source)
        g_source_remove (search_source);

    search_source = g_timeout_add (SEARCH_DELAY, search_timeout, NULL);
}

static void update_database (void)
{
    int list = get_playlist (TRUE, TRUE);

    if (list >= 0)
    {
        create_database (list);
        schedule_search ();
    }
    else
        destroy_database ();

    if (results_list)
        audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));

    show_hide_widgets ();
}

static void add_complete_cb (void * unused, void * unused2)
{
    if (adding)
    {
        int list = get_playlist (TRUE, FALSE);

        if (list >= 0 && ! aud_playlist_add_in_progress (list))
        {
            adding = FALSE;
            destroy_added_table ();
            aud_playlist_sort_by_scheme (list, PLAYLIST_SORT_PATH);
        }
    }

    if (! database && ! aud_playlist_update_pending ())
        update_database ();
}

static void scan_complete_cb (void * unused, void * unused2)
{
    if (! database && ! aud_playlist_update_pending ())
        update_database ();
}

static void playlist_update_cb (void * data, void * unused)
{
    if (! database)
        update_database ();
    else
    {
        int list = get_playlist (TRUE, TRUE);
        int at, count;

        if (list < 0 || aud_playlist_updated_range (list, & at, & count) >=
         PLAYLIST_UPDATE_METADATA)
            update_database ();
    }
}

static bool_t search_init (void)
{
    find_playlist ();

    set_search_phrase ("");
    items = index_new ();
    selection = g_array_new (FALSE, FALSE, 1);

    update_database ();

    hook_associate ("playlist add complete", add_complete_cb, NULL);
    hook_associate ("playlist scan complete", scan_complete_cb, NULL);
    hook_associate ("playlist update", playlist_update_cb, NULL);

    return TRUE;
}

static void search_cleanup (void)
{
    hook_dissociate ("playlist add complete", add_complete_cb);
    hook_dissociate ("playlist scan complete", scan_complete_cb);
    hook_dissociate ("playlist update", playlist_update_cb);

    if (search_source)
    {
        g_source_remove (search_source);
        search_source = 0;
    }

    g_strfreev (search_terms);
    search_terms = NULL;

    index_free (items);
    items = NULL;
    g_array_free (selection, TRUE);
    selection = NULL;

    destroy_added_table ();
    destroy_database ();
}

static void do_add (bool_t play, char * * title)
{
    int list = aud_playlist_by_unique_id (playlist_id);
    int n_items = index_count (items);
    int n_selected = 0;

    Index * filenames = index_new ();
    Index * tuples = index_new ();

    for (int i = 0; i < n_items; i ++)
    {
        if (! selection->data[i])
            continue;

        Item * item = index_get (items, i);

        for (int m = 0; m < item->matches->len; m ++)
        {
            int entry = g_array_index (item->matches, int, m);
            index_append (filenames, aud_playlist_entry_get_filename (list, entry));
            index_append (tuples, aud_playlist_entry_get_tuple (list, entry, TRUE));
        }

        n_selected ++;
        if (title && n_selected == 1)
            * title = item->name;
    }

    if (title && n_selected != 1)
        * title = NULL;

    aud_playlist_entry_insert_batch (aud_playlist_get_active (), -1, filenames,
     tuples, play);
}

static void action_play (void)
{
    int list = aud_playlist_get_temporary ();
    aud_playlist_set_active (list);

    if (aud_get_bool (NULL, "clear_playlist"))
        aud_playlist_entry_delete (list, 0, aud_playlist_entry_count (list));
    else
        aud_playlist_queue_delete (list, 0, aud_playlist_queue_count (list));

    do_add (TRUE, NULL);
}

static void action_create_playlist (void)
{
    char * title;

    aud_playlist_insert (-1);
    aud_playlist_set_active (aud_playlist_count () - 1);
    do_add (FALSE, & title);

    if (title)
        aud_playlist_set_title (aud_playlist_count () - 1, title);
}

static void action_add_to_playlist (void)
{
    if (aud_playlist_by_unique_id (playlist_id) == aud_playlist_get_active ())
        return;

    do_add (FALSE, NULL);
}

static void list_get_value (void * user, int row, int column, GValue * value)
{
    g_return_if_fail (items && row >= 0 && row < index_count (items));

    Item * item = index_get (items, row);
    char * string = NULL;

    switch (item->field)
    {
        int albums;
        char scratch[128];

    case TITLE:
        string = g_strdup_printf (_("%s\n on %s by %s"), item->name,
         item->parent->name, item->parent->parent->name);
        break;

    case ARTIST:
        albums = g_hash_table_size (item->children);
        snprintf (scratch, sizeof scratch, dngettext (PACKAGE, "%d album",
         "%d albums", albums), albums);
        string = g_strdup_printf (dngettext (PACKAGE, "%s\n %s, %d song",
         "%s\n %s, %d songs", item->matches->len), item->name, scratch,
         item->matches->len);
        break;

    case ALBUM:
        string = g_strdup_printf (dngettext (PACKAGE, "%s\n %d song by %s",
         "%s\n %d songs by %s", item->matches->len), item->name,
         item->matches->len, item->parent->name);
        break;
    }

    g_value_take_string (value, string);
}

static bool_t list_get_selected (void * user, int row)
{
    g_return_val_if_fail (selection && row >= 0 && row < selection->len, FALSE);
    return selection->data[row];
}

static void list_set_selected (void * user, int row, bool_t selected)
{
    g_return_if_fail (selection && row >= 0 && row < selection->len);
    selection->data[row] = selected;
}

static void list_select_all (void * user, bool_t selected)
{
    g_return_if_fail (selection);
    memset (selection->data, selected, selection->len);
}

static void list_activate_row (void * user, int row)
{
    action_play ();
}

static void list_right_click (void * user, GdkEventButton * event)
{
    GtkWidget * menu = gtk_menu_new ();

    GtkWidget * item = gtk_image_menu_item_new_from_stock (GTK_STOCK_MEDIA_PLAY, NULL);
    g_signal_connect (item, "activate", (GCallback) action_play, NULL);
    gtk_widget_show (item);
    gtk_menu_shell_append ((GtkMenuShell *) menu, item);

    item = gtk_image_menu_item_new_with_mnemonic (_("_Create Playlist"));
    gtk_image_menu_item_set_image ((GtkImageMenuItem *) item,
     gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
    g_signal_connect (item, "activate", (GCallback) action_create_playlist, NULL);
    gtk_widget_show (item);
    gtk_menu_shell_append ((GtkMenuShell *) menu, item);

    item = gtk_image_menu_item_new_with_mnemonic (_("_Add to Playlist"));
    gtk_image_menu_item_set_image ((GtkImageMenuItem *) item,
     gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
    g_signal_connect (item, "activate", (GCallback) action_add_to_playlist, NULL);
    gtk_widget_show (item);
    gtk_menu_shell_append ((GtkMenuShell *) menu, item);

    gtk_menu_popup ((GtkMenu *) menu, NULL, NULL, NULL, NULL, event->button, event->time);
}

static const AudguiListCallbacks list_callbacks = {
 .get_value = list_get_value,
 .get_selected = list_get_selected,
 .set_selected = list_set_selected,
 .select_all = list_select_all,
 .activate_row = list_activate_row,
 .right_click = list_right_click};

static void entry_cb (GtkEntry * entry, void * unused)
{
    set_search_phrase (gtk_entry_get_text ((GtkEntry *) entry));
    schedule_search ();
}

static void refresh_cb (GtkButton * button, GtkWidget * chooser)
{
    char * path = gtk_file_chooser_get_filename ((GtkFileChooser *) chooser);
    begin_add (path);
    g_free (path);

    update_database ();
}

static void * search_get_widget (void)
{
    GtkWidget * vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width ((GtkContainer *) vbox, 2);

    entry = gtk_entry_new ();
    gtk_entry_set_icon_from_stock ((GtkEntry *) entry, GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
#if GTK_CHECK_VERSION (3, 2, 0)
    gtk_entry_set_placeholder_text ((GtkEntry *) entry, _("Search library"));
#endif
    g_signal_connect (entry, "destroy", (GCallback) gtk_widget_destroyed, & entry);
    gtk_box_pack_start ((GtkBox *) vbox, entry, FALSE, FALSE, 0);

    help_label = gtk_label_new (_("To import your music library into "
     "Audacious, choose a folder and then click the \"refresh\" icon."));
    gtk_widget_set_size_request (help_label, 194, -1);
    gtk_label_set_line_wrap ((GtkLabel *) help_label, TRUE);
    g_signal_connect (help_label, "destroy", (GCallback) gtk_widget_destroyed, & help_label);
    gtk_widget_set_no_show_all (help_label, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, help_label, TRUE, FALSE, 0);

    wait_label = gtk_label_new (_("Please wait ..."));
    g_signal_connect (wait_label, "destroy", (GCallback) gtk_widget_destroyed, & wait_label);
    gtk_widget_set_no_show_all (wait_label, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, wait_label, TRUE, FALSE, 0);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    g_signal_connect (scrolled, "destroy", (GCallback) gtk_widget_destroyed, & scrolled);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrolled,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_no_show_all (scrolled, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, scrolled, TRUE, TRUE, 0);

    results_list = audgui_list_new (& list_callbacks, NULL, items ? index_count (items) : 0);
    g_signal_connect (results_list, "destroy", (GCallback) gtk_widget_destroyed, & results_list);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) results_list, FALSE);
    audgui_list_add_column (results_list, NULL, 0, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scrolled, results_list);

    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_end ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

    GtkWidget * chooser = gtk_file_chooser_button_new (_("Choose Folder"),
     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_box_pack_start ((GtkBox *) hbox, chooser, TRUE, TRUE, 0);

    char * path = get_path ();
    gtk_file_chooser_set_filename ((GtkFileChooser *) chooser, path);
    g_free (path);

    GtkWidget * button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_stock
     (GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON));
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_box_pack_start ((GtkBox *) hbox, button, FALSE, FALSE, 0);

    g_signal_connect (entry, "changed", (GCallback) entry_cb, NULL);
    g_signal_connect (entry, "activate", (GCallback) action_play, NULL);
    g_signal_connect (button, "clicked", (GCallback) refresh_cb, chooser);

    gtk_widget_show_all (vbox);
    gtk_widget_show (results_list);
    show_hide_widgets ();

    return vbox;
}

int search_take_message (const char * code, const void * data, int size)
{
    if (! strcmp (code, "grab focus"))
    {
        if (entry)
            gtk_widget_grab_focus (entry);
        return 0;
    }

    return EINVAL;
}

AUD_GENERAL_PLUGIN
(
    .name = "Search Tool",
    .init = search_init,
    .cleanup = search_cleanup,
    .get_widget = search_get_widget,
    .take_message = search_take_message
)
