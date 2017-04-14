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

#define AUD_GLIB_INTEGRATION
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

static Playlist s_playlist;
static Index<String> s_search_terms;

/* Note: added_table is accessed by multiple threads.
 * When adding = true, it may only be accessed by the playlist add thread.
 * When adding = false, it may only be accessed by the UI thread.
 * adding may only be set by the UI thread while holding adding_lock. */
static TinyLock s_adding_lock;
static bool s_adding = false;
static SimpleHash<String, bool> s_added_table;

static SimpleHash<Key, Item> s_database;
static bool s_database_valid;
static Index<const Item *> s_items;
static int s_hidden_items;
static Index<bool> s_selection;

static QueuedFunc s_search_timer;
static bool s_search_pending;

static GtkWidget * entry, * help_label, * wait_label, * scrolled, * results_list, * stats_label;

static void find_playlist ()
{
    s_playlist = Playlist ();

    for (int p = 0; p < Playlist::n_playlists (); p ++)
    {
        auto playlist = Playlist::by_index (p);
        if (! strcmp (playlist.get_title (), _("Library")))
        {
            s_playlist = playlist;
            break;
        }
    }
}

static void create_playlist ()
{
    s_playlist = Playlist::blank_playlist ();
    s_playlist.set_title (_("Library"));
    s_playlist.active_playlist ();
}

static bool check_playlist (bool require_added, bool require_scanned)
{
    if (! s_playlist.exists ())
    {
        s_playlist = Playlist ();
        return false;
    }

    if (require_added && s_playlist.add_in_progress ())
        return false;
    if (require_scanned && s_playlist.scan_in_progress ())
        return false;

    return true;
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
    s_items.clear ();
    s_hidden_items = 0;
    s_database.clear ();
    s_database_valid = false;
}

static void create_database ()
{
    destroy_database ();

    int entries = s_playlist.n_entries ();

    for (int e = 0; e < entries; e ++)
    {
        Tuple tuple = s_playlist.entry_tuple (e, Playlist::NoWait);

        aud::array<SearchField, String> fields;
        fields[SearchField::Genre] = tuple.get_str (Tuple::Genre);
        fields[SearchField::Artist] = tuple.get_str (Tuple::Artist);
        fields[SearchField::Album] = tuple.get_str (Tuple::Album);
        fields[SearchField::Title] = tuple.get_str (Tuple::Title);

        Item * parent = nullptr;
        SimpleHash<Key, Item> * hash = & s_database;

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

    s_database_valid = true;
}

static void search_recurse (SimpleHash<Key, Item> & domain, int mask, Index<const Item *> & results)
{
    domain.iterate ([mask, & results] (const Key & key, Item & item)
    {
        int count = s_search_terms.len ();
        int new_mask = mask;

        for (int t = 0, bit = 1; t < count; t ++, bit <<= 1)
        {
            if (! (new_mask & bit))
                continue; /* skip term if it is already found */

            if (strstr (item.folded, s_search_terms[t]))
                new_mask &= ~bit; /* we found it */
            else if (! item.children.n_items ())
                break; /* quit early if there are no children to search */
        }

        /* adding an item with exactly one child is redundant, so avoid it */
        if (! new_mask && item.children.n_items () != 1)
            results.append (& item);

        search_recurse (item.children, new_mask, results);
    });
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
    s_items.clear ();
    s_hidden_items = 0;

    if (! s_database_valid)
        return;

    /* effectively limits number of search terms to 32 */
    search_recurse (s_database, (1 << s_search_terms.len ()) - 1, s_items);

    /* first sort by number of songs per item */
    s_items.sort (item_compare_pass1);

    /* limit to items with most songs */
    if (s_items.len () > MAX_RESULTS)
    {
        s_hidden_items = s_items.len () - MAX_RESULTS;
        s_items.remove (MAX_RESULTS, -1);
    }

    /* sort by item type, then item name */
    s_items.sort (item_compare);

    s_selection.remove (0, -1);
    s_selection.insert (0, s_items.len ());
    if (s_items.len ())
        s_selection[0] = true;
}

static bool filter_cb (const char * filename, void * unused)
{
    bool add = false;
    tiny_lock (& s_adding_lock);

    if (s_adding)
    {
        bool * added = s_added_table.lookup (String (filename));

        if ((add = ! added))
            s_added_table.add (String (filename), true);
        else
            (* added) = true;
    }

    tiny_unlock (& s_adding_lock);
    return add;
}

static void begin_add (const char * uri)
{
    if (s_adding)
        return;

    if (! check_playlist (false, false))
        create_playlist ();

    /* if possible, store local path for compatibility with older versions */
    StringBuf path = uri_to_filename (uri);
    aud_set_str ("search-tool", "path", path ? path : uri);

    s_added_table.clear ();

    int entries = s_playlist.n_entries ();

    for (int entry = 0; entry < entries; entry ++)
    {
        String filename = s_playlist.entry_filename (entry);

        if (! s_added_table.lookup (filename))
        {
            s_playlist.select_entry (entry, false);
            s_added_table.add (filename, false);
        }
        else
            s_playlist.select_entry (entry, true);
    }

    s_playlist.remove_selected ();

    tiny_lock (& s_adding_lock);
    s_adding = true;
    tiny_unlock (& s_adding_lock);

    Index<PlaylistAddItem> add;
    add.append (String (uri));
    s_playlist.insert_filtered (-1, std::move (add), filter_cb, nullptr, false);
}

static void show_hide_widgets ()
{
    if (s_playlist == Playlist ())
    {
        gtk_widget_hide (wait_label);
        gtk_widget_hide (scrolled);
        gtk_widget_hide (stats_label);
        gtk_widget_show (help_label);
    }
    else
    {
        gtk_widget_hide (help_label);

        if (s_database_valid)
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
    audgui_list_insert_rows (results_list, 0, s_items.len ());

    int total = s_items.len () + s_hidden_items;

    if (s_hidden_items)
        gtk_label_set_text ((GtkLabel *) stats_label,
         str_printf (dngettext (PACKAGE, "%d of %d result shown",
         "%d of %d results shown", total), s_items.len (), total));
    else
        gtk_label_set_text ((GtkLabel *) stats_label,
         str_printf (dngettext (PACKAGE, "%d result", "%d results", total), total));

    s_search_timer.stop ();
    s_search_pending = false;
}

static void update_database ()
{
    if (check_playlist (true, true))
    {
        create_database ();
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

static void add_complete_cb (void *, void *)
{
    if (! check_playlist (true, false))
        return;

    if (s_adding)
    {
        tiny_lock (& s_adding_lock);
        s_adding = false;
        tiny_unlock (& s_adding_lock);

        int entries = s_playlist.n_entries ();

        for (int entry = 0; entry < entries; entry ++)
        {
            String filename = s_playlist.entry_filename (entry);
            bool * added = s_added_table.lookup (filename);

            s_playlist.select_entry (entry, ! added || ! (* added));
        }

        s_added_table.clear ();

        /* don't clear the playlist if nothing was added */
        if (s_playlist.n_selected () < entries)
            s_playlist.remove_selected ();
        else
            s_playlist.select_all (false);

        s_playlist.sort_entries (Playlist::Path);
    }

    if (! s_database_valid && ! s_playlist.update_pending ())
        update_database ();
}

static void scan_complete_cb (void *, void *)
{
    if (! check_playlist (true, true))
        return;

    if (! s_database_valid && ! s_playlist.update_pending ())
        update_database ();
}

static void playlist_update_cb (void *, void *)
{
    if (! s_database_valid || ! check_playlist (true, true) ||
        s_playlist.update_detail ().level >= Playlist::Metadata)
    {
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

    s_search_timer.stop ();
    s_search_pending = false;

    s_search_terms.clear ();
    s_items.clear ();
    s_selection.clear ();

    tiny_lock (& s_adding_lock);
    s_adding = false;
    tiny_unlock (& s_adding_lock);

    s_added_table.clear ();
    destroy_database ();
}

static void do_add (bool play, bool set_title)
{
    if (s_search_pending)
        search_timeout ();

    int n_items = s_items.len ();
    int n_selected = 0;

    Index<PlaylistAddItem> add;
    String title;

    for (int i = 0; i < n_items; i ++)
    {
        if (! s_selection[i])
            continue;

        const Item * item = s_items[i];

        for (int entry : item->matches)
        {
            add.append (
                s_playlist.entry_filename (entry),
                s_playlist.entry_tuple (entry, Playlist::NoWait),
                s_playlist.entry_decoder (entry, Playlist::NoWait)
            );
        }

        n_selected ++;
        if (n_selected == 1)
            title = item->name;
    }

    auto list2 = Playlist::active_playlist ();
    list2.insert_items (-1, std::move (add), play);

    if (set_title && n_selected == 1)
        list2.set_title (title);
}

static void action_play ()
{
    Playlist::temporary_playlist ().activate ();
    do_add (true, false);
}

static void action_create_playlist ()
{
    Playlist::new_playlist ();
    do_add (false, true);
}

static void action_add_to_playlist ()
{
    if (s_playlist != Playlist::active_playlist ())
        do_add (false, false);
}

static void list_get_value (void * user, int row, int column, GValue * value)
{
    static constexpr aud::array<SearchField, const char *> start_tags =
        {"", "<b>", "<i>", ""};
    static constexpr aud::array<SearchField, const char *> end_tags =
        {"", "</b>", "</i>", ""};

    auto escape = [] (const char * s)
        { return CharPtr (g_markup_escape_text (s, -1)); };

    g_return_if_fail (row >= 0 && row < s_items.len ());

    const Item * item = s_items[row];

    CharPtr name = escape ((item->field == SearchField::Genre) ?
                           str_toupper_utf8 (item->name) : item->name);

    StringBuf desc (0);

    if (item->field != SearchField::Title)
    {
        desc.insert (-1, " ");
        desc.combine (str_printf (dngettext (PACKAGE, "%d song", "%d songs",
         item->matches.len ()), item->matches.len ()));
    }

    if (item->field == SearchField::Genre)
    {
        desc.insert (-1, " ");
        desc.insert (-1, _("of this genre"));
    }

    if (item->parent)
    {
        auto parent = (item->parent->parent ? item->parent->parent : item->parent);

        desc.insert (-1, " ");
        desc.insert (-1, (parent->field == SearchField::Album) ? _("on") : _("by"));
        desc.insert (-1, " ");
        desc.insert (-1, start_tags[parent->field]);
        desc.insert (-1, escape (parent->name));
        desc.insert (-1, end_tags[parent->field]);
    }

    g_value_take_string (value, g_strdup_printf
     ("%s%s%s\n<small>%s</small>", start_tags[item->field], (const char *) name,
      end_tags[item->field], (const char *) desc));
}

static bool list_get_selected (void * user, int row)
{
    g_return_val_if_fail (row >= 0 && row < s_selection.len (), false);
    return s_selection[row];
}

static void list_set_selected (void * user, int row, bool selected)
{
    g_return_if_fail (row >= 0 && row < s_selection.len ());
    s_selection[row] = selected;
}

static void list_select_all (void * user, bool selected)
{
    for (bool & s : s_selection)
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
    if (s_search_pending)
        search_timeout ();

    int n_items = s_items.len ();
    Index<char> buf;

    s_playlist.select_all (false);

    for (int i = 0; i < n_items; i ++)
    {
        if (! s_selection[i])
            continue;

        const Item * item = s_items[i];

        for (int entry : item->matches)
        {
            if (buf.len ())
                buf.append ('\n');

            String filename = s_playlist.entry_filename (entry);
            buf.insert (filename, -1, strlen (filename));

            s_playlist.select_entry (entry, true);
        }
    }

    s_playlist.cache_selected ();

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
    s_search_terms = str_list_to_index (str_tolower_utf8 (text), " ");
    s_search_timer.queue (SEARCH_DELAY, search_timeout, nullptr);
    s_search_pending = true;
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
    GtkWidget * vbox = gtk_vbox_new (false, 6);

    entry = gtk_entry_new ();
    gtk_entry_set_icon_from_icon_name ((GtkEntry *) entry, GTK_ENTRY_ICON_PRIMARY, "edit-find");
    (void) _("Search library");  // translated string is used in GTK3 branch
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

    results_list = audgui_list_new (& list_callbacks, nullptr, s_items.len ());
    g_signal_connect (results_list, "destroy", (GCallback) gtk_widget_destroyed, & results_list);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) results_list, false);
    audgui_list_add_column (results_list, nullptr, 0, G_TYPE_STRING, -1, true);
    gtk_container_add ((GtkContainer *) scrolled, results_list);

    stats_label = gtk_label_new ("");
    g_signal_connect (stats_label, "destroy", (GCallback) gtk_widget_destroyed, & stats_label);
    gtk_widget_set_no_show_all (stats_label, true);
    gtk_box_pack_start ((GtkBox *) vbox, stats_label, false, false, 0);

    GtkWidget * hbox = gtk_hbox_new (false, 6);
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
