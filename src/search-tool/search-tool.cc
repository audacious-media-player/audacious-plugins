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

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>
#include <libaudgui/list.h>
#include <libaudgui/menu.h>

#define MAX_RESULTS 20
#define SEARCH_DELAY 300

enum {GENRE = 0, ARTIST, ALBUM, TITLE, FIELDS};

struct Key
{
    int field;
    String name;

    bool operator== (const Key & b) const
        { return field == b.field && name == b.name; }
    unsigned hash () const
        { return field + name.hash (); }
};

struct Item
{
    int field;
    String name, folded;
    Item * parent;
    SimpleHash<Key, Item> children;
    Index<int> matches;

    Item (int field, const String & name, Item * parent) :
        field (field),
        name (name),
        folded (str_tolower_utf8 (name)),
        parent (parent) {}

    Item (Item &&) = default;
    Item & operator= (Item &&) = default;
};

struct SearchState {
    Index<const Item *> items;
    int mask;
};

static int playlist_id;
static Index<String> search_terms;

static SimpleHash<String, bool> added_table;
static SimpleHash<Key, Item> database;
static bool database_valid;
static Index<const Item *> items;
static int hidden_items;
static Index<bool> selection;

static gboolean adding;
static int search_source;

static GtkWidget * entry, * help_label, * wait_label, * scrolled, * results_list, * stats_label;

static void find_playlist ()
{
    playlist_id = -1;

    for (int p = 0; playlist_id < 0 && p < aud_playlist_count (); p ++)
    {
        String title = aud_playlist_get_title (p);
        if (! strcmp (title, _("Library")))
            playlist_id = aud_playlist_get_unique_id (p);
    }
}

static int create_playlist ()
{
    int list = aud_playlist_get_blank ();
    aud_playlist_set_title (list, _("Library"));
    aud_playlist_set_active (list);
    playlist_id = aud_playlist_get_unique_id (list);
    return list;
}

static int get_playlist (gboolean require_added, gboolean require_scanned)
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
    search_terms = str_list_to_index (str_tolower_utf8 (phrase), " ");
}

static String get_path ()
{
    String path1 = aud_get_str ("search-tool", "path");
    if (g_file_test (path1, G_FILE_TEST_EXISTS))
        return path1;

    StringBuf path2 = filename_build ({g_get_home_dir (), "Music"});
    if (g_file_test (path2, G_FILE_TEST_EXISTS))
        return String (path2);

    return String (g_get_home_dir ());
}

static void destroy_database ()
{
    items.clear ();
    hidden_items = 0;
    database.clear ();
    database_valid = false;
}

static void create_database (int list)
{
    destroy_database ();

    int entries = aud_playlist_entry_count (list);

    for (int e = 0; e < entries; e ++)
    {
        String fields[FIELDS];

        aud_playlist_entry_describe (list, e, fields[TITLE], fields[ARTIST], fields[ALBUM], true);
        fields[GENRE] = aud_playlist_entry_get_tuple (list, e, true).get_str (FIELD_GENRE);

        if (! fields[TITLE])
            continue;

        Item * parent = nullptr;
        SimpleHash<Key, Item> * hash = & database;

        for (int f = 0; f < FIELDS; f ++)
        {
            if (fields[f])
            {
                Key key = {f, fields[f]};
                Item * item = hash->lookup (key);

                if (! item)
                    item = hash->add (key, Item (f, fields[f], parent));

                item->matches.append (e);

                /* genre is outside the normal hierarchy */
                if (f != GENRE)
                {
                    parent = item;
                    hash = & item->children;
                }
            }
        }
    }

    database_valid = true;
}

static void search_cb (const Key & key, Item & item, void * _state)
{
    SearchState * state = (SearchState *) _state;

    int oldmask = state->mask;
    int count = search_terms.len ();

    for (int t = 0, bit = 1; t < count; t ++, bit <<= 1)
    {
        if (! (state->mask & bit))
            continue; /* skip term if it is already found */

        if (strstr (item.folded, search_terms[t]))
            state->mask &= ~bit; /* we found it */
        else if (! item.children.n_items ())
            break; /* quit early if there are no children to search */
    }

    /* adding an item with exactly one child is redundant, so avoid it */
    if (! state->mask && item.children.n_items () != 1)
        state->items.append (& item);

    item.children.iterate (search_cb, state);

    state->mask = oldmask;
}

static int item_compare (const Item * const & a, const Item * const & b, void *)
{
    if (a->field < b->field)
        return -1;
    if (a->field > b->field)
        return 1;

    int val = str_compare (a->name, b->name);
    if (val)
        return val;

    if (a->parent)
        return b->parent ? item_compare (a->parent, b->parent, nullptr) : 1;
    else
        return b->parent ? -1 : 0;
}

static int item_compare_pass1 (const Item * const & a, const Item * const & b, void *)
{
    if (a->matches.len () > b->matches.len ())
        return -1;
    if (a->matches.len () < b->matches.len ())
        return 1;

    return item_compare (a, b, nullptr);
}

static void do_search ()
{
    items.clear ();
    hidden_items = 0;

    if (! database_valid)
        return;

    SearchState state;

    /* effectively limits number of search terms to 32 */
    state.mask = (1 << search_terms.len ()) - 1;

    database.iterate (search_cb, & state);

    items = std::move (state.items);

    /* first sort by number of songs per item */
    items.sort (item_compare_pass1, nullptr);

    /* limit to items with most songs */
    if (items.len () > MAX_RESULTS)
    {
        hidden_items = items.len () - MAX_RESULTS;
        items.remove (MAX_RESULTS, -1);
    }

    /* sort by item type, then item name */
    items.sort (item_compare, nullptr);

    selection.remove (0, -1);
    selection.insert (0, items.len ());
    if (items.len ())
        selection[0] = true;
}

static bool filter_cb (const char * filename, void * unused)
{
    return ! added_table.lookup (String (filename));
}

static void begin_add (const char * path)
{
    int list = get_playlist (false, false);

    if (list < 0)
        list = create_playlist ();

    aud_set_str ("search-tool", "path", path);

    StringBuf uri = filename_to_uri (path);
    g_return_if_fail (uri);

    if (! g_str_has_suffix (uri, "/"))
        str_insert (uri, -1, "/");

    added_table.clear ();

    int entries = aud_playlist_entry_count (list);

    for (int entry = 0; entry < entries; entry ++)
    {
        String filename = aud_playlist_entry_get_filename (list, entry);

        if (g_str_has_prefix (filename, uri) && ! added_table.lookup (filename))
        {
            aud_playlist_entry_set_selected (list, entry, false);
            added_table.add (filename, true);
        }
        else
            aud_playlist_entry_set_selected (list, entry, true);
    }

    aud_playlist_delete_selected (list);
    aud_playlist_remove_failed (list);

    Index<PlaylistAddItem> add;
    add.append (String (uri));
    aud_playlist_entry_insert_filtered (list, -1, std::move (add), filter_cb, nullptr, false);

    adding = true;
}

static void show_hide_widgets ()
{
    if (playlist_id < 0)
    {
        gtk_widget_hide (wait_label);
        gtk_widget_hide (scrolled);
        gtk_widget_hide (stats_label);
        gtk_widget_show (help_label);
    }
    else
    {
        gtk_widget_hide (help_label);

        if (database_valid)
        {
            gtk_widget_hide (wait_label);
            gtk_widget_show (scrolled);
            gtk_widget_show (stats_label);
        }
        else
        {
            gtk_widget_hide (scrolled);
            gtk_widget_hide (stats_label);
            gtk_widget_show (wait_label);
        }
    }
}

static int search_timeout (void * unused = nullptr)
{
    do_search ();

    audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
    audgui_list_insert_rows (results_list, 0, items.len ());

    int total = items.len () + hidden_items;
    StringBuf stats = str_printf (dngettext (PACKAGE, "%d result",
     "%d results", total), total);

    if (hidden_items)
    {
        str_insert (stats, -1, " ");
        stats.combine (str_printf (dngettext (PACKAGE, "(%d hidden)",
         "(%d hidden)", hidden_items), hidden_items));
    }

    gtk_label_set_text ((GtkLabel *) stats_label, stats);

    if (search_source)
    {
        g_source_remove (search_source);
        search_source = 0;
    }

    return false;
}

static void schedule_search ()
{
    if (search_source)
        g_source_remove (search_source);

    search_source = g_timeout_add (SEARCH_DELAY, search_timeout, nullptr);
}

static void update_database ()
{
    int list = get_playlist (true, true);

    if (list >= 0)
    {
        create_database (list);
        search_timeout ();
    }
    else
    {
        destroy_database ();
        audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
        gtk_label_set_text ((GtkLabel *) stats_label, "");
    }

    show_hide_widgets ();
}

static void add_complete_cb (void * unused, void * unused2)
{
    if (adding)
    {
        int list = get_playlist (true, false);

        if (list >= 0 && ! aud_playlist_add_in_progress (list))
        {
            adding = false;
            added_table.clear ();
            aud_playlist_sort_by_scheme (list, PLAYLIST_SORT_PATH);
        }
    }

    if (! database_valid && ! aud_playlist_update_pending ())
        update_database ();
}

static void scan_complete_cb (void * unused, void * unused2)
{
    if (! database_valid && ! aud_playlist_update_pending ())
        update_database ();
}

static void playlist_update_cb (void * data, void * unused)
{
    if (! database_valid)
        update_database ();
    else
    {
        int list = get_playlist (true, true);
        int at, count;

        if (list < 0 || aud_playlist_updated_range (list, & at, & count) >=
         PLAYLIST_UPDATE_METADATA)
            update_database ();
    }
}

static void search_init ()
{
    find_playlist ();

    update_database ();

    hook_associate ("playlist add complete", add_complete_cb, nullptr);
    hook_associate ("playlist scan complete", scan_complete_cb, nullptr);
    hook_associate ("playlist update", playlist_update_cb, nullptr);
}

static void search_cleanup ()
{
    hook_dissociate ("playlist add complete", add_complete_cb);
    hook_dissociate ("playlist scan complete", scan_complete_cb);
    hook_dissociate ("playlist update", playlist_update_cb);

    if (search_source)
    {
        g_source_remove (search_source);
        search_source = 0;
    }

    search_terms.clear ();
    items.clear ();
    selection.clear ();

    added_table.clear ();
    destroy_database ();
}

static void do_add (gboolean play, String & title)
{
    if (search_source)
        search_timeout ();

    int list = aud_playlist_by_unique_id (playlist_id);
    int n_items = items.len ();
    int n_selected = 0;

    Index<PlaylistAddItem> add;

    for (int i = 0; i < n_items; i ++)
    {
        if (! selection[i])
            continue;

        const Item * item = items[i];

        for (int entry : item->matches)
        {
            add.append (
                aud_playlist_entry_get_filename (list, entry),
                aud_playlist_entry_get_tuple (list, entry, true)
            );
        }

        n_selected ++;
        if (n_selected == 1)
            title = item->name;
    }

    if (n_selected != 1)
        title = String ();

    aud_playlist_entry_insert_batch (aud_playlist_get_active (), -1, std::move (add), play);
}

static void action_play ()
{
    int list = aud_playlist_get_temporary ();
    aud_playlist_set_active (list);

    if (aud_get_bool (nullptr, "clear_playlist"))
        aud_playlist_entry_delete (list, 0, aud_playlist_entry_count (list));
    else
        aud_playlist_queue_delete (list, 0, aud_playlist_queue_count (list));

    String title;
    do_add (true, title);
}

static void action_create_playlist ()
{
    aud_playlist_insert (-1);
    aud_playlist_set_active (aud_playlist_count () - 1);

    String title;
    do_add (false, title);

    if (title)
        aud_playlist_set_title (aud_playlist_count () - 1, title);
}

static void action_add_to_playlist ()
{
    if (aud_playlist_by_unique_id (playlist_id) == aud_playlist_get_active ())
        return;

    String title;
    do_add (false, title);
}

static void list_get_value (void * user, int row, int column, GValue * value)
{
    g_return_if_fail (row >= 0 && row < items.len ());

    const Item * item = items[row];
    StringBuf string = str_concat ({item->name, "\n"});

    if (item->field != TITLE)
    {
        str_insert (string, -1, " ");
        string.combine (str_printf (dngettext (PACKAGE, "%d song", "%d songs",
         item->matches.len ()), item->matches.len ()));
    }

    if (item->field == GENRE)
    {
        str_insert (string, -1, " ");
        str_insert (string, -1, _("of this genre"));
    }

    while ((item = item->parent))
    {
        str_insert (string, -1, " ");
        str_insert (string, -1, (item->field == ALBUM) ? _("on") : _("by"));
        str_insert (string, -1, " ");
        str_insert (string, -1, item->name);
    }

    g_value_set_string (value, string);
}

static bool list_get_selected (void * user, int row)
{
    g_return_val_if_fail (row >= 0 && row < selection.len (), false);
    return selection[row];
}

static void list_set_selected (void * user, int row, bool selected)
{
    g_return_if_fail (row >= 0 && row < selection.len ());
    selection[row] = selected;
}

static void list_select_all (void * user, bool selected)
{
    for (bool & s : selection)
        s = selected;
}

static void list_activate_row (void * user, int row)
{
    action_play ();
}

static void list_right_click (void * user, GdkEventButton * event)
{
    static const AudguiMenuItem items[] = {
        MenuCommand (N_("_Play"), "media-playback-start",
         0, (GdkModifierType) 0, action_play),
        MenuCommand (N_("_Create Playlist"), "document-new",
         0, (GdkModifierType) 0, action_create_playlist),
        MenuCommand (N_("_Add to Playlist"), "list-add",
         0, (GdkModifierType) 0, action_add_to_playlist)
    };

    GtkWidget * menu = gtk_menu_new ();
    audgui_menu_init (menu, {items}, nullptr);
    gtk_menu_popup ((GtkMenu *) menu, nullptr, nullptr, nullptr, nullptr, event->button, event->time);
}

static const AudguiListCallbacks list_callbacks = {
    list_get_value,
    list_get_selected,
    list_set_selected,
    list_select_all,
    list_activate_row,
    list_right_click
};

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

static void * search_get_widget ()
{
    GtkWidget * vbox = gtk_vbox_new (FALSE, 6);

    entry = gtk_entry_new ();
    gtk_entry_set_icon_from_icon_name ((GtkEntry *) entry, GTK_ENTRY_ICON_PRIMARY, "edit-find");
    (void) _("Search library");  // translated string is used in GTK3 branch
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

    scrolled = gtk_scrolled_window_new (nullptr, nullptr);
    g_signal_connect (scrolled, "destroy", (GCallback) gtk_widget_destroyed, & scrolled);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrolled,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_no_show_all (scrolled, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, scrolled, TRUE, TRUE, 0);

    results_list = audgui_list_new (& list_callbacks, nullptr, items.len ());
    g_signal_connect (results_list, "destroy", (GCallback) gtk_widget_destroyed, & results_list);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) results_list, FALSE);
    audgui_list_add_column (results_list, nullptr, 0, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scrolled, results_list);

    stats_label = gtk_label_new ("");
    g_signal_connect (stats_label, "destroy", (GCallback) gtk_widget_destroyed, & stats_label);
    gtk_widget_set_no_show_all (stats_label, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, stats_label, FALSE, FALSE, 0);

    GtkWidget * hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_end ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

    GtkWidget * chooser = gtk_file_chooser_button_new (_("Choose Folder"),
     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_box_pack_start ((GtkBox *) hbox, chooser, TRUE, TRUE, 0);

    gtk_file_chooser_set_filename ((GtkFileChooser *) chooser, get_path ());

    GtkWidget * button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("view-refresh", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_box_pack_start ((GtkBox *) hbox, button, FALSE, FALSE, 0);

    search_init ();

    g_signal_connect (vbox, "destroy", (GCallback) search_cleanup, nullptr);
    g_signal_connect (entry, "changed", (GCallback) entry_cb, nullptr);
    g_signal_connect (entry, "activate", (GCallback) action_play, nullptr);
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

#define AUD_PLUGIN_NAME        N_("Search Tool")
#define AUD_GENERAL_GET_WIDGET   search_get_widget
#define AUD_PLUGIN_TAKE_MESSAGE  search_take_message

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>
