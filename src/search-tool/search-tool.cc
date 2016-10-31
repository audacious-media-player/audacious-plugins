/*
 * search-tool.cc
 * Copyright 2011-2015 John Lindgren
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
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>
#include <libaudgui/menu.h>

#define MAX_RESULTS 20
#define SEARCH_DELAY 300

class SearchTool : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Search Tool"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginGLibOnly
    };

    constexpr SearchTool () : GeneralPlugin (info, false) {}

    void * get_gtk_widget ();
    int take_message (const char * code, const void * data, int size);
};

EXPORT SearchTool aud_plugin_instance;

enum class SearchField {
    Genre,
    Artist,
    Album,
    Title,
    count
};

struct Key
{
    SearchField field;
    String name;

    bool operator== (const Key & b) const
        { return field == b.field && name == b.name; }
    unsigned hash () const
        { return (unsigned) field + name.hash (); }
};

struct Item
{
    SearchField field;
    String name, folded;
    Item * parent;
    SimpleHash<Key, Item> children;
    Index<int> matches;

    Item (SearchField field, const String & name, Item * parent) :
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

/* Note: added_table is accessed by multiple threads.
 * When adding = true, it may only be accessed by the playlist add thread.
 * When adding = false, it may only be accessed by the UI thread.
 * adding may only be set by the UI thread while holding adding_lock. */
static TinyLock adding_lock;
static bool adding = false;
static SimpleHash<String, bool> added_table;

static SimpleHash<Key, Item> database;
static bool database_valid;
static Index<const Item *> items;
static int hidden_items;
static Index<bool> selection;

static QueuedFunc search_timer;
static bool search_pending;

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

static int get_playlist (bool require_added, bool require_scanned)
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

static String get_uri ()
{
    auto to_uri = [] (const char * path)
        { return String (filename_to_uri (path)); };

    String path1 = aud_get_str ("search-tool", "path");
    if (path1[0])
        return strstr (path1, "://") ? path1 : to_uri (path1);

    StringBuf path2 = filename_build ({g_get_home_dir (), "Music"});
    if (g_file_test (path2, G_FILE_TEST_EXISTS))
        return to_uri (path2);

    return to_uri (g_get_home_dir ());
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
        Tuple tuple = aud_playlist_entry_get_tuple (list, e, Playlist::NoWait);

        aud::array<SearchField, String> fields;
        fields[SearchField::Genre] = tuple.get_str (Tuple::Genre);
        fields[SearchField::Artist] = tuple.get_str (Tuple::Artist);
        fields[SearchField::Album] = tuple.get_str (Tuple::Album);
        fields[SearchField::Title] = tuple.get_str (Tuple::Title);

        Item * parent = nullptr;
        SimpleHash<Key, Item> * hash = & database;

        for (auto f : aud::range<SearchField> ())
        {
            if (fields[f])
            {
                Key key = {f, fields[f]};
                Item * item = hash->lookup (key);

                if (! item)
                    item = hash->add (key, Item (f, fields[f], parent));

                item->matches.append (e);

                /* genre is outside the normal hierarchy */
                if (f != SearchField::Genre)
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

static int item_compare (const Item * const & a, const Item * const & b)
{
    if (a->field < b->field)
        return -1;
    if (a->field > b->field)
        return 1;

    int val = str_compare (a->name, b->name);
    if (val)
        return val;

    if (a->parent)
        return b->parent ? item_compare (a->parent, b->parent) : 1;
    else
        return b->parent ? -1 : 0;
}

static int item_compare_pass1 (const Item * const & a, const Item * const & b)
{
    if (a->matches.len () > b->matches.len ())
        return -1;
    if (a->matches.len () < b->matches.len ())
        return 1;

    return item_compare (a, b);
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
    items.sort (item_compare_pass1);

    /* limit to items with most songs */
    if (items.len () > MAX_RESULTS)
    {
        hidden_items = items.len () - MAX_RESULTS;
        items.remove (MAX_RESULTS, -1);
    }

    /* sort by item type, then item name */
    items.sort (item_compare);

    selection.remove (0, -1);
    selection.insert (0, items.len ());
    if (items.len ())
        selection[0] = true;
}

static bool filter_cb (const char * filename, void * unused)
{
    bool add = false;
    tiny_lock (& adding_lock);

    if (adding)
    {
        bool * added = added_table.lookup (String (filename));

        if ((add = ! added))
            added_table.add (String (filename), true);
        else
            (* added) = true;
    }

    tiny_unlock (& adding_lock);
    return add;
}

static void begin_add (const char * uri)
{
    if (adding)
        return;

    int list = get_playlist (false, false);

    if (list < 0)
        list = create_playlist ();

    /* if possible, store local path for compatibility with older versions */
    StringBuf path = uri_to_filename (uri);
    aud_set_str ("search-tool", "path", path ? path : uri);

    added_table.clear ();

    int entries = aud_playlist_entry_count (list);

    for (int entry = 0; entry < entries; entry ++)
    {
        String filename = aud_playlist_entry_get_filename (list, entry);

        if (! added_table.lookup (filename))
        {
            aud_playlist_entry_set_selected (list, entry, false);
            added_table.add (filename, false);
        }
        else
            aud_playlist_entry_set_selected (list, entry, true);
    }

    aud_playlist_delete_selected (list);

    tiny_lock (& adding_lock);
    adding = true;
    tiny_unlock (& adding_lock);

    Index<PlaylistAddItem> add;
    add.append (String (uri));
    aud_playlist_entry_insert_filtered (list, -1, std::move (add), filter_cb, nullptr, false);
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

static void search_timeout (void * = nullptr)
{
    do_search ();

    audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
    audgui_list_insert_rows (results_list, 0, items.len ());

    int total = items.len () + hidden_items;

    if (hidden_items)
        gtk_label_set_text ((GtkLabel *) stats_label,
         str_printf (dngettext (PACKAGE, "%d of %d result shown",
         "%d of %d results shown", total), items.len (), total));
    else
        gtk_label_set_text ((GtkLabel *) stats_label,
         str_printf (dngettext (PACKAGE, "%d result", "%d results", total), total));

    search_timer.stop ();
    search_pending = false;
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
    int list = get_playlist (true, false);
    if (list < 0)
        return;

    if (adding)
    {
        tiny_lock (& adding_lock);
        adding = false;
        tiny_unlock (& adding_lock);

        int entries = aud_playlist_entry_count (list);

        for (int entry = 0; entry < entries; entry ++)
        {
            String filename = aud_playlist_entry_get_filename (list, entry);
            bool * added = added_table.lookup (filename);

            aud_playlist_entry_set_selected (list, entry, ! added || ! (* added));
        }

        added_table.clear ();

        /* don't clear the playlist if nothing was added */
        if (aud_playlist_selected_count (list) < aud_playlist_entry_count (list))
            aud_playlist_delete_selected (list);
        else
            aud_playlist_select_all (list, false);

        aud_playlist_sort_by_scheme (list, Playlist::Path);
    }

    if (! database_valid && ! aud_playlist_update_pending (list))
        update_database ();
}

static void scan_complete_cb (void * unused, void * unused2)
{
    int list = get_playlist (true, true);
    if (list < 0)
        return;

    if (! database_valid && ! aud_playlist_update_pending (list))
        update_database ();
}

static void playlist_update_cb (void * data, void * unused)
{
    if (! database_valid)
        update_database ();
    else
    {
        int list = get_playlist (true, true);
        if (list < 0 || aud_playlist_update_detail (list).level >= Playlist::Metadata)
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

    search_timer.stop ();
    search_pending = false;

    search_terms.clear ();
    items.clear ();
    selection.clear ();

    tiny_lock (& adding_lock);
    adding = false;
    tiny_unlock (& adding_lock);

    added_table.clear ();
    destroy_database ();
}

static void do_add (bool play, bool set_title)
{
    if (search_pending)
        search_timeout ();

    int list = aud_playlist_by_unique_id (playlist_id);
    int n_items = items.len ();
    int n_selected = 0;

    Index<PlaylistAddItem> add;
    String title;

    for (int i = 0; i < n_items; i ++)
    {
        if (! selection[i])
            continue;

        const Item * item = items[i];

        for (int entry : item->matches)
        {
            add.append (
                aud_playlist_entry_get_filename (list, entry),
                aud_playlist_entry_get_tuple (list, entry, Playlist::NoWait),
                aud_playlist_entry_get_decoder (list, entry, Playlist::NoWait)
            );
        }

        n_selected ++;
        if (n_selected == 1)
            title = item->name;
    }

    int list2 = aud_playlist_get_active ();
    aud_playlist_entry_insert_batch (list2, -1, std::move (add), play);

    if (set_title && n_selected == 1)
        aud_playlist_set_title (list2, title);
}

static void action_play ()
{
    aud_playlist_set_active (aud_playlist_get_temporary ());
    do_add (true, false);
}

static void action_create_playlist ()
{
    aud_playlist_new ();
    do_add (false, true);
}

static void action_add_to_playlist ()
{
    if (aud_playlist_by_unique_id (playlist_id) != aud_playlist_get_active ())
        do_add (false, false);
}

static void list_get_value (void * user, int row, int column, GValue * value)
{
    g_return_if_fail (row >= 0 && row < items.len ());

    const Item * item = items[row];
    StringBuf string = str_concat ({item->name, "\n"});

    if (item->field != SearchField::Title)
    {
        string.insert (-1, " ");
        string.combine (str_printf (dngettext (PACKAGE, "%d song", "%d songs",
         item->matches.len ()), item->matches.len ()));
    }

    if (item->field == SearchField::Genre)
    {
        string.insert (-1, " ");
        string.insert (-1, _("of this genre"));
    }

    while ((item = item->parent))
    {
        string.insert (-1, " ");
        string.insert (-1, (item->field == SearchField::Album) ? _("on") : _("by"));
        string.insert (-1, " ");
        string.insert (-1, item->name);
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

static Index<char> list_get_data (void * user)
{
    if (search_pending)
        search_timeout ();

    int list = aud_playlist_by_unique_id (playlist_id);
    int n_items = items.len ();

    Index<char> buf;

    aud_playlist_select_all (playlist_id, false);

    for (int i = 0; i < n_items; i ++)
    {
        if (! selection[i])
            continue;

        const Item * item = items[i];

        for (int entry : item->matches)
        {
            if (buf.len ())
                buf.append ('\n');

            String filename = aud_playlist_entry_get_filename (list, entry);
            buf.insert (filename, -1, strlen (filename));

            aud_playlist_entry_set_selected (list, entry, true);
        }
    }

    aud_playlist_cache_selected (list);

    return buf;
}

static const AudguiListCallbacks list_callbacks = {
    list_get_value,
    list_get_selected,
    list_set_selected,
    list_select_all,
    list_activate_row,
    list_right_click,
    nullptr,  // shift_rows
    "text/uri-list",
    list_get_data,
    nullptr  // receive_data
};

static void entry_cb (GtkEntry * entry, void * unused)
{
    const char * text = gtk_entry_get_text ((GtkEntry *) entry);
    search_terms = str_list_to_index (str_tolower_utf8 (text), " ");
    search_timer.queue (SEARCH_DELAY, search_timeout, nullptr);
    search_pending = true;
}

static void file_entry_cb (GtkEntry * entry, GtkWidget * button)
{
    const char * text = gtk_entry_get_text ((GtkEntry *) entry);
    gtk_widget_set_sensitive (button, text[0] != 0);
}

static void refresh_cb (GtkButton * button, GtkWidget * file_entry)
{
    String uri = audgui_file_entry_get_uri (file_entry);
    if (uri)
    {
        audgui_file_entry_set_uri (file_entry, uri);  // normalize path
        begin_add (uri);
        update_database ();
    }
}

void * SearchTool::get_gtk_widget ()
{
    GtkWidget * vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

    entry = gtk_entry_new ();
    gtk_entry_set_icon_from_icon_name ((GtkEntry *) entry, GTK_ENTRY_ICON_PRIMARY, "edit-find");
    gtk_entry_set_placeholder_text ((GtkEntry *) entry, _("Search library"));
    g_signal_connect (entry, "destroy", (GCallback) gtk_widget_destroyed, & entry);
    gtk_box_pack_start ((GtkBox *) vbox, entry, false, false, 0);

    help_label = gtk_label_new (_("To import your music library into "
     "Audacious, choose a folder and then click the \"refresh\" icon."));
    int label_width = aud::rescale (audgui_get_dpi (), 4, 7);
    gtk_widget_set_size_request (help_label, label_width, -1);
    gtk_label_set_line_wrap ((GtkLabel *) help_label, true);
    g_signal_connect (help_label, "destroy", (GCallback) gtk_widget_destroyed, & help_label);
    gtk_widget_set_no_show_all (help_label, true);
    gtk_box_pack_start ((GtkBox *) vbox, help_label, true, false, 0);

    wait_label = gtk_label_new (_("Please wait ..."));
    g_signal_connect (wait_label, "destroy", (GCallback) gtk_widget_destroyed, & wait_label);
    gtk_widget_set_no_show_all (wait_label, true);
    gtk_box_pack_start ((GtkBox *) vbox, wait_label, true, false, 0);

    scrolled = gtk_scrolled_window_new (nullptr, nullptr);
    g_signal_connect (scrolled, "destroy", (GCallback) gtk_widget_destroyed, & scrolled);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrolled,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_no_show_all (scrolled, true);
    gtk_box_pack_start ((GtkBox *) vbox, scrolled, true, true, 0);

    results_list = audgui_list_new (& list_callbacks, nullptr, items.len ());
    g_signal_connect (results_list, "destroy", (GCallback) gtk_widget_destroyed, & results_list);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) results_list, false);
    audgui_list_add_column (results_list, nullptr, 0, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scrolled, results_list);

    stats_label = gtk_label_new ("");
    g_signal_connect (stats_label, "destroy", (GCallback) gtk_widget_destroyed, & stats_label);
    gtk_widget_set_no_show_all (stats_label, true);
    gtk_box_pack_start ((GtkBox *) vbox, stats_label, false, false, 0);

    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_end ((GtkBox *) vbox, hbox, false, false, 0);

    GtkWidget * file_entry = audgui_file_entry_new
     (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("Choose Folder"));
    gtk_box_pack_start ((GtkBox *) hbox, file_entry, true, true, 0);

    audgui_file_entry_set_uri (file_entry, get_uri ());

    GtkWidget * button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("view-refresh", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_box_pack_start ((GtkBox *) hbox, button, false, false, 0);

    search_init ();

    g_signal_connect (vbox, "destroy", (GCallback) search_cleanup, nullptr);
    g_signal_connect (entry, "changed", (GCallback) entry_cb, nullptr);
    g_signal_connect (entry, "activate", (GCallback) action_play, nullptr);
    g_signal_connect (file_entry, "changed", (GCallback) file_entry_cb, button);
    g_signal_connect (file_entry, "activate", (GCallback) refresh_cb, file_entry);
    g_signal_connect (button, "clicked", (GCallback) refresh_cb, file_entry);

    gtk_widget_show_all (vbox);
    gtk_widget_show (results_list);
    show_hide_widgets ();

    return vbox;
}

int SearchTool::take_message (const char * code, const void * data, int size)
{
    if (! strcmp (code, "grab focus"))
    {
        if (entry)
            gtk_widget_grab_focus (entry);
        return 0;
    }

    return -1;
}
