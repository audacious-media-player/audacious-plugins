/*
 * search-tool.cc
 * Copyright 2011-2017 John Lindgren
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
#include <gdk/gdk.h>

#define AUD_GLIB_INTEGRATION
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/runtime.h>
#include <libaudgui/gtk-compat.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>
#include <libaudgui/menu.h>

#include "library.h"
#include "search-model.h"

#define CFG_ID "search-tool"
#define SEARCH_DELAY 300

class SearchTool : public GeneralPlugin
{
public:
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Search Tool"),
        PACKAGE,
        nullptr, // about
        & prefs,
        PluginGLibOnly
    };

    constexpr SearchTool () : GeneralPlugin (info, false) {}

    bool init ();
    void * get_gtk_widget ();
    int take_message (const char * code, const void *, int);
};

EXPORT SearchTool aud_plugin_instance;

static void trigger_search ();

const char * const SearchTool::defaults[] = {
    "max_results", "20",
    "rescan_on_startup", "FALSE",
    nullptr
};

const PreferencesWidget SearchTool::widgets[] = {
    WidgetSpin (N_("Number of results to show:"),
        WidgetInt (CFG_ID, "max_results", trigger_search),
         {10, 10000, 10}),
    WidgetCheck (N_("Rescan library at startup"),
        WidgetBool (CFG_ID, "rescan_on_startup"))
};

const PluginPreferences SearchTool::prefs = {{widgets}};

static Library * s_library = nullptr;
static SearchModel s_model;
static Index<bool> s_selection;

static QueuedFunc s_search_timer;
static bool s_search_pending;

static GtkWidget * entry, * help_label, * wait_label, * scrolled, * results_list, * stats_label;

static String get_uri ()
{
    auto to_uri = [] (const char * path)
        { return String (filename_to_uri (path)); };

    String path1 = aud_get_str (CFG_ID, "path");
    if (path1[0])
        return strstr (path1, "://") ? path1 : to_uri (path1);

    StringBuf path2 = filename_build ({g_get_home_dir (), "Music"});
    if (g_file_test (path2, G_FILE_TEST_EXISTS))
        return to_uri (path2);

    return to_uri (g_get_home_dir ());
}

static void show_hide_widgets ()
{
    if (s_library->playlist () == Playlist ())
    {
        gtk_widget_hide (wait_label);
        gtk_widget_hide (scrolled);
        gtk_widget_hide (stats_label);
        gtk_widget_show (help_label);
    }
    else
    {
        gtk_widget_hide (help_label);

        if (s_library->is_ready ())
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

static void search_timeout ()
{
    const char * text = gtk_entry_get_text ((GtkEntry *) entry);
    auto terms = str_list_to_index (str_tolower_utf8 (text), " ");
    s_model.do_search (terms, aud_get_int (CFG_ID, "max_results"));

    int shown = s_model.num_items ();
    int hidden = s_model.num_hidden_items ();
    int total = shown + hidden;

    s_selection.clear ();
    s_selection.insert (0, shown);
    if (shown)
        s_selection[0] = true;

    audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
    audgui_list_insert_rows (results_list, 0, shown);

    if (hidden)
        gtk_label_set_text ((GtkLabel *) stats_label,
         str_printf (dngettext (PACKAGE, "%d of %d result shown",
         "%d of %d results shown", total), shown, total));
    else
        gtk_label_set_text ((GtkLabel *) stats_label,
         str_printf (dngettext (PACKAGE, "%d result", "%d results", total), total));

    s_search_timer.stop ();
    s_search_pending = false;
}

static void trigger_search ()
{
    s_search_timer.queue (SEARCH_DELAY, search_timeout);
    s_search_pending = true;
}

void Library::signal_update ()
{
    if (s_library->is_ready ())
    {
        s_model.create_database (s_library->playlist ());
        search_timeout ();
    }
    else
    {
        s_model.destroy_database ();
        s_selection.clear ();
        audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
        gtk_label_set_text ((GtkLabel *) stats_label, "");
    }

    show_hide_widgets ();
}

static void search_init ()
{
    s_library = new Library;

    if (aud_get_bool (CFG_ID, "rescan_on_startup"))
        s_library->begin_add (get_uri ());

    s_library->check_ready_and_update (true);
}

static void search_cleanup ()
{
    s_search_timer.stop ();
    s_search_pending = false;

    delete s_library;
    s_library = nullptr;

    s_model.destroy_database ();
    s_selection.clear ();
}

static void do_add (bool play, bool set_title)
{
    if (s_search_pending)
        search_timeout ();

    auto list = s_library->playlist ();
    int n_items = s_model.num_items ();
    int n_selected = 0;

    Index<PlaylistAddItem> add;
    String title;

    for (int i = 0; i < n_items; i ++)
    {
        if (! s_selection[i])
            continue;

        auto & item = s_model.item_at (i);

        for (int entry : item.matches)
        {
            add.append (
                list.entry_filename (entry),
                list.entry_tuple (entry, Playlist::NoWait),
                list.entry_decoder (entry, Playlist::NoWait)
            );
        }

        n_selected ++;
        if (n_selected == 1)
            title = item.name;
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
    if (s_library->playlist () != Playlist::active_playlist ())
        do_add (false, false);
}

static void list_get_value (void * user, int row, int column, GValue * value)
{
    auto escape = [] (const char * s)
        { return CharPtr (g_markup_escape_text (s, -1)); };

    g_return_if_fail (row >= 0 && row < s_model.num_items ());

    auto & item = s_model.item_at (row);

    CharPtr name = escape ((item.field == SearchField::Genre) ?
                           str_toupper_utf8 (item.name) : item.name);

    StringBuf desc (0);

    if (item.field != SearchField::Title)
    {
        desc.insert (-1, " ");
        str_append_printf (desc, dngettext (PACKAGE, "%d song", "%d songs",
         item.matches.len ()), item.matches.len ());
    }

    if (item.field == SearchField::Genre)
    {
        desc.insert (-1, " ");
        desc.insert (-1, _("of this genre"));
    }
    else if (item.parent)
    {
        auto parent = (item.parent->parent ? item.parent->parent : item.parent);

        desc.insert (-1, " ");
        desc.insert (-1, parent_prefix (parent->field));
        desc.insert (-1, " ");
        desc.insert (-1, start_tags[parent->field]);
        desc.insert (-1, escape (parent->name));
        desc.insert (-1, end_tags[parent->field]);
    }

    g_value_take_string (value, g_strdup_printf
     ("%s%s%s\n<small>%s</small>", start_tags[item.field], (const char *) name,
      end_tags[item.field], (const char *) desc));
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

#ifdef USE_GTK3
    gtk_menu_popup_at_pointer ((GtkMenu *) menu, (const GdkEvent *) event);
#else
    gtk_menu_popup ((GtkMenu *) menu, nullptr, nullptr, nullptr, nullptr, event->button, event->time);
#endif
}

static Index<char> list_get_data (void * user)
{
    if (s_search_pending)
        search_timeout ();

    auto list = s_library->playlist ();
    int n_items = s_model.num_items ();
    Index<char> buf;

    list.select_all (false);

    for (int i = 0; i < n_items; i ++)
    {
        if (! s_selection[i])
            continue;

        auto & item = s_model.item_at (i);

        for (int entry : item.matches)
        {
            if (buf.len ())
                buf.append ('\n');

            String filename = list.entry_filename (entry);
            buf.insert (filename, -1, strlen (filename));

            list.select_entry (entry, true);
        }
    }

    list.cache_selected ();

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
    s_search_timer.queue (SEARCH_DELAY, search_timeout);
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
        /* if possible, store local path for compatibility with older versions */
        StringBuf path = uri_to_filename (uri);
        aud_set_str (CFG_ID, "path", path ? path : uri);

        s_library->begin_add (uri);
        s_library->check_ready_and_update (true);
    }
}

bool SearchTool::init ()
{
    aud_config_set_defaults (CFG_ID, defaults);
    return true;
}

void * SearchTool::get_gtk_widget ()
{
    GtkWidget * vbox = audgui_vbox_new (6);

    entry = gtk_entry_new ();
    gtk_entry_set_icon_from_icon_name ((GtkEntry *) entry, GTK_ENTRY_ICON_PRIMARY, "edit-find");
#ifdef USE_GTK3
    gtk_entry_set_placeholder_text ((GtkEntry *) entry, _("Search library"));
#endif
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

    results_list = audgui_list_new (& list_callbacks, nullptr, 0);
    g_signal_connect (results_list, "destroy", (GCallback) gtk_widget_destroyed, & results_list);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) results_list, false);
    audgui_list_add_column (results_list, nullptr, 0, G_TYPE_STRING, -1, true);
    gtk_container_add ((GtkContainer *) scrolled, results_list);

    stats_label = gtk_label_new ("");
    g_signal_connect (stats_label, "destroy", (GCallback) gtk_widget_destroyed, & stats_label);
    gtk_widget_set_no_show_all (stats_label, true);
    gtk_box_pack_start ((GtkBox *) vbox, stats_label, false, false, 0);

    GtkWidget * hbox = audgui_hbox_new (6);
    gtk_box_pack_end ((GtkBox *) vbox, hbox, false, false, 0);

    GtkWidget * file_entry = audgui_file_entry_new
     (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("Choose Folder"));
    gtk_box_pack_start ((GtkBox *) hbox, file_entry, true, true, 0);

    audgui_file_entry_set_uri (file_entry, get_uri ());

    GtkWidget * button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("view-refresh", GTK_ICON_SIZE_MENU));
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

int SearchTool::take_message (const char * code, const void *, int)
{
    if (! strcmp (code, "grab focus"))
    {
        if (entry)
            gtk_widget_grab_focus (entry);
        return 0;
    }

    return -1;
}
