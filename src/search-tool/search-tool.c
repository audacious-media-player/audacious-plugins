/*
 * search-tool.c
 * Copyright 2011-2012 John Lindgren
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
#include <libaudgui/menu.h>

#define MAX_RESULTS 100
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
static Index * search_terms;

static GHashTable * added_table;
static GHashTable * database;
static Index * items;
static GArray * selection;

static bool_t adding;
static int search_source;

static GtkWidget * entry, * help_label, * wait_label, * scrolled, * results_list;

static void item_free (Item * item);

static Item * item_new (int field, char * name, Item * parent)
{
    Item * item = g_slice_new (Item);
    item->field = field;
    item->name = name;
    item->folded = str_tolower_utf8 (name);
    item->parent = parent;
    item->matches = g_array_new (FALSE, FALSE, sizeof (int));

    if (field == TITLE)
        item->children = NULL;
    else
        item->children = g_hash_table_new_full ((GHashFunc) str_hash,
         (GEqualFunc) str_equal, NULL, (GDestroyNotify) item_free);

    return item;
}

static void item_free (Item * item)
{
    if (item->children)
        g_hash_table_destroy (item->children);

    str_unref (item->name);
    str_unref (item->folded);
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
    char * folded = str_tolower_utf8 (phrase);
    index_free_full (search_terms, (IndexFreeFunc) str_unref);
    search_terms = str_list_to_index (folded, " ");
    str_unref (folded);
}

static char * get_path (void)
{
    char * path = aud_get_str ("search-tool", "path");
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

    str_unref (path);

    path = filename_build (g_get_home_dir (), "Music");
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

    str_unref (path);

    return str_get (g_get_home_dir ());
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

    database = g_hash_table_new_full ((GHashFunc) str_hash, (GEqualFunc)
     str_equal, NULL, (GDestroyNotify) item_free);

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
    int count = index_count (search_terms);

    for (int t = 0, bit = 1; t < count; t ++, bit <<= 1)
    {
        if (! (state->mask & bit))
            continue; /* skip term if it is already found */

        if (strstr (item->folded, index_get (search_terms, t)))
            state->mask &= ~bit; /* we found it */
        else if (! item->children)
            break; /* quit early if there are no children to search */
    }

    if (! state->mask)
        index_insert (state->items[item->field], -1, item);

    if (item->children)
        g_hash_table_foreach (item->children, search_cb, state);

    state->mask = oldmask;
}

static int item_compare (const void * _a, const void * _b)
{
    const Item * a = _a, * b = _b;

    int val = str_compare (a->name, b->name);
    if (val)
        return val;

    if (a->parent)
        return b->parent ? item_compare (a->parent, b->parent) : 1;
    else
        return b->parent ? -1 : 0;
}

static void do_search (void)
{
    index_delete (items, 0, index_count (items));

    if (! database)
        return;

    SearchState state;

    for (int f = 0; f < FIELDS; f ++)
        state.items[f] = index_new ();

    /* effectively limits number of search terms to 32 */
    state.mask = (1 << index_count (search_terms)) - 1;

    g_hash_table_foreach (database, search_cb, & state);

    int total = 0;

    for (int f = 0; f < FIELDS; f ++)
    {
        int count = index_count (state.items[f]);
        if (count > MAX_RESULTS - total)
            count = MAX_RESULTS - total;

        if (count)
        {
            index_sort (state.items[f], item_compare);
            index_copy_insert (state.items[f], 0, items, -1, count);
            total += count;
        }

        index_free (state.items[f]);
    }

    g_array_set_size (selection, total);
    memset (selection->data, 0, selection->len);
    if (selection->len > 0)
        selection->data[0] = 1;
}

static bool_t filter_cb (const char * filename, void * unused)
{
    return added_table && ! g_hash_table_contains (added_table, filename);
}

static void begin_add (const char * path)
{
    int list = get_playlist (FALSE, FALSE);

    if (list < 0)
        list = create_playlist ();

    aud_set_str ("search-tool", "path", path);

    char * uri = filename_to_uri (path);
    g_return_if_fail (uri);

    if (! g_str_has_suffix (uri, "/"))
    {
        SCONCAT2 (temp, uri, "/");
        str_unref (uri);
        uri = str_get (temp);
    }

    destroy_added_table ();

    added_table = g_hash_table_new_full ((GHashFunc) str_hash, (GEqualFunc)
     str_equal, (GDestroyNotify) str_unref, NULL);

    int entries = aud_playlist_entry_count (list);

    for (int entry = 0; entry < entries; entry ++)
    {
        char * filename = aud_playlist_entry_get_filename (list, entry);

        if (g_str_has_prefix (filename, uri) && ! g_hash_table_contains (added_table, filename))
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
    index_insert (add, -1, uri);
    aud_playlist_entry_insert_filtered (list, -1, add, NULL, filter_cb, NULL, FALSE);

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

    search_terms = index_new ();
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

    index_free_full (search_terms, (IndexFreeFunc) str_unref);
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
    if (search_source)
        search_timeout (NULL);

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
            index_insert (filenames, -1, aud_playlist_entry_get_filename (list, entry));
            index_insert (tuples, -1, aud_playlist_entry_get_tuple (list, entry, TRUE));
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
    static const AudguiMenuItem items[] = {
        {N_("_Play"), "media-playback-start", .func = action_play},
        {N_("_Create Playlist"), "document-new", .func = action_create_playlist},
        {N_("_Add to Playlist"), "list-add", .func = action_add_to_playlist}
    };

    GtkWidget * menu = gtk_menu_new ();
    audgui_menu_init (menu, items, ARRAY_LEN (items), NULL);
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

    entry = gtk_entry_new ();
    gtk_entry_set_icon_from_icon_name ((GtkEntry *) entry, GTK_ENTRY_ICON_PRIMARY, "edit-find");
    gtk_entry_set_placeholder_text ((GtkEntry *) entry, _("Search library"));
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
    str_unref (path);

    GtkWidget * button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("view-refresh", GTK_ICON_SIZE_BUTTON));
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
    .name = N_("Search Tool"),
    .domain = PACKAGE,
    .init = search_init,
    .cleanup = search_cleanup,
    .get_widget = search_get_widget,
    .take_message = search_take_message
)
