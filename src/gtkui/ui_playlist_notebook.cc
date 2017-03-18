/*
 * ui_playlist_notebook.c
 * Copyright 2010-2017 Michał Lipski and John Lindgren
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

#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define AUD_GLIB_INTEGRATION
#include <libaudcore/runtime.h>
#include <libaudcore/playlist.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/list.h>
#include <libaudgui/libaudgui.h>

#include "../ui-common/menu-ops.h"

#include "gtkui.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

GtkWidget * pl_notebook = nullptr;

static Playlist highlighted;

static int switch_handler = 0;
static int reorder_handler = 0;

static GtkWidget * treeview_of (GtkWidget * page)
    { return (GtkWidget *) g_object_get_data ((GObject *) page, "treeview"); }

static GtkWidget * treeview_at_idx (int idx)
    { return treeview_of (gtk_notebook_get_nth_page ((GtkNotebook *) pl_notebook, idx)); }

static Playlist list_of (GtkWidget * widget)
    { return aud::from_ptr<Playlist> (g_object_get_data ((GObject *) widget, "playlist")); }

void apply_column_widths (GtkWidget * treeview)
{
    /* skip righthand column since it expands with the window */
    for (int i = 0; i < pw_num_cols - 1; i ++)
    {
        GtkTreeViewColumn * col = gtk_tree_view_get_column ((GtkTreeView *) treeview, i);
        gtk_tree_view_column_set_fixed_width (col, pw_col_widths[pw_cols[i]]);
    }
}

static void size_allocate_cb (GtkWidget * treeview)
{
    int current = gtk_notebook_get_current_page ((GtkNotebook *) pl_notebook);

    if (current < 0 || treeview != treeview_at_idx (current))
        return;

    bool changed = false;

    /* skip righthand column since it expands with the window */
    for (int i = 0; i < pw_num_cols - 1; i ++)
    {
        GtkTreeViewColumn * col = gtk_tree_view_get_column ((GtkTreeView *) treeview, i);
        int width = gtk_tree_view_column_get_width (col);

        if (width != pw_col_widths[pw_cols[i]])
        {
            pw_col_widths[pw_cols[i]] = width;
            changed = true;
        }
    }

    if (changed)
    {
        int count = gtk_notebook_get_n_pages ((GtkNotebook *) pl_notebook);

        for (int i = 0; i < count; i ++)
        {
            if (i != current)
                apply_column_widths (treeview_at_idx (i));
        }
    }
}

static void make_add_button (GtkWidget * notebook)
{
    GtkWidget * button = gtk_button_new ();
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("list-add", GTK_ICON_SIZE_MENU));
    gtk_widget_set_can_focus (button, false);

    g_signal_connect (button, "clicked", pl_new, nullptr);
    gtk_widget_show_all (button);

    gtk_notebook_set_action_widget ((GtkNotebook *) notebook, button, GTK_PACK_END);
}

static void close_button_cb (GtkWidget * button, void * data)
{
    audgui_confirm_playlist_delete (aud::from_ptr<Playlist> (data));
}

static void close_button_style_set (GtkWidget * button)
{
    int w, h;
    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (button),
     GTK_ICON_SIZE_MENU, & w, & h);
    gtk_widget_set_size_request (button, w + 2, h + 2);
}

static GtkWidget * make_close_button (GtkWidget * ebox, Playlist list)
{
    GtkWidget * button = gtk_button_new ();
    GtkWidget * image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
    gtk_button_set_image ((GtkButton *) button, image);
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click ((GtkButton *) button, false);
    gtk_widget_set_name (button, "gtkui-tab-close-button");

    g_signal_connect (button, "clicked", (GCallback) close_button_cb, aud::to_ptr (list));

    gtk_rc_parse_string (
     "style \"gtkui-tab-close-button-style\" {"
     " GtkButton::default-border = {0, 0, 0, 0}"
     " GtkButton::default-outside-border = {0, 0, 0, 0}"
     " GtkButton::inner-border = {0, 0, 0, 0}"
     " GtkWidget::focus-padding = 0"
     " GtkWidget::focus-line-width = 0"
     " xthickness = 0"
     " ythickness = 0 }"
     "widget \"*.gtkui-tab-close-button\" style \"gtkui-tab-close-button-style\""
    );

    g_signal_connect (button, "style-set", (GCallback) close_button_style_set, nullptr);

    gtk_widget_show (button);

    return button;
}

void pl_notebook_grab_focus ()
{
    int idx = gtk_notebook_get_current_page ((GtkNotebook *) pl_notebook);
    gtk_widget_grab_focus (treeview_at_idx (idx));
}

static void tab_title_reset (GtkWidget * ebox)
{
    GtkWidget * label = (GtkWidget *) g_object_get_data ((GObject *) ebox, "label");
    GtkWidget * entry = (GtkWidget *) g_object_get_data ((GObject *) ebox, "entry");
    gtk_widget_hide (entry);
    gtk_widget_show (label);
}

static void tab_title_save (GtkEntry * entry, GtkWidget * ebox)
{
    GtkWidget * label = (GtkWidget *) g_object_get_data ((GObject *) ebox, "label");
    list_of (ebox).set_title (gtk_entry_get_text (entry));
    gtk_widget_hide ((GtkWidget *) entry);
    gtk_widget_show (label);
}

static gboolean tab_key_press_cb (GtkWidget * widget, GdkEventKey * event)
{
    if (event->keyval == GDK_KEY_Escape)
        tab_title_reset (widget);

    return false;
}

static gboolean tab_button_press_cb (GtkWidget * ebox, GdkEventButton * event)
{
    auto list = list_of (ebox);

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
        list.start_playback ();

    if (event->type == GDK_BUTTON_PRESS && event->button == 2)
        audgui_confirm_playlist_delete (list);

    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
        popup_menu_tab (event->button, event->time, list);

    return false;
}

static gboolean scroll_cb (GtkWidget * widget, GdkEventScroll * event)
{
    switch (event->direction)
    {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_LEFT:
        pl_prev ();
        return true;

    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_RIGHT:
        pl_next ();
        return true;

    default:
        return false;
    }
}

static gboolean scroll_ignore_cb ()
{
    return true;
}

static void tab_changed (GtkNotebook * notebook, GtkWidget * page, unsigned page_num)
{
    Playlist::by_index (page_num).activate ();
}

static void tab_reordered (GtkNotebook * notebook, GtkWidget * page, unsigned page_num)
{
    auto list = list_of (treeview_of (page));
    Playlist::reorder_playlists (list.index (), page_num, 1);
}

static GtkLabel * get_tab_label (int list_idx)
{
    GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) pl_notebook, list_idx);
    GtkWidget * ebox = gtk_notebook_get_tab_label ((GtkNotebook *) pl_notebook, page);
    return (GtkLabel *) g_object_get_data ((GObject *) ebox, "label");
}

static void update_tab_label (GtkLabel * label, Playlist list)
{
    String title0 = list.get_title ();
    StringBuf title = aud_get_bool ("gtkui", "entry_count_visible") ?
     str_printf ("%s (%d)", (const char *) title0, list.n_entries ()) :
     str_copy (title0);

    if (list == Playlist::playing_playlist ())
    {
        CharPtr markup (g_markup_printf_escaped ("<b>%s</b>", (const char *) title));
        gtk_label_set_markup (label, markup);
    }
    else
        gtk_label_set_text (label, title);
}

void start_rename_playlist (Playlist playlist)
{
    if (! gtk_notebook_get_show_tabs ((GtkNotebook *) pl_notebook))
    {
        audgui_show_playlist_rename (playlist);
        return;
    }

    GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) pl_notebook, playlist.index ());
    GtkWidget * ebox = gtk_notebook_get_tab_label ((GtkNotebook *) pl_notebook, page);

    GtkWidget * label = (GtkWidget *) g_object_get_data ((GObject *) ebox, "label");
    GtkWidget * entry = (GtkWidget *) g_object_get_data ((GObject *) ebox, "entry");
    gtk_widget_hide (label);

    gtk_entry_set_text ((GtkEntry *) entry, playlist.get_title ());

    gtk_widget_grab_focus (entry);
    gtk_editable_select_region ((GtkEditable *) entry, 0, -1);
    gtk_widget_show (entry);
}

static void create_tab (int list_idx, Playlist list)
{
    GtkWidget * scrollwin = gtk_scrolled_window_new (nullptr, nullptr);
    GtkAdjustment * vscroll = gtk_scrolled_window_get_vadjustment ((GtkScrolledWindow *) scrollwin);

    /* do not allow scroll events to propagate up to the notebook */
    g_signal_connect_after (scrollwin, "scroll-event", (GCallback) scroll_ignore_cb, nullptr);

    GtkWidget * treeview = ui_playlist_widget_new (list);

    apply_column_widths (treeview);
    g_signal_connect (treeview, "size-allocate", (GCallback) size_allocate_cb, nullptr);

    g_object_set_data ((GObject *) scrollwin, "treeview", treeview);

    gtk_container_add ((GtkContainer *) scrollwin, treeview);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrollwin,
     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show_all (scrollwin);

    GtkWidget * ebox = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) ebox, false);

    GtkWidget * hbox = gtk_hbox_new (false, 2);

    GtkWidget * label = gtk_label_new ("");
    update_tab_label ((GtkLabel *) label, list);
    gtk_box_pack_start ((GtkBox *) hbox, label, false, false, 0);

    GtkWidget * entry = gtk_entry_new ();
    gtk_box_pack_start ((GtkBox *) hbox, entry, false, false, 0);
    gtk_container_add ((GtkContainer *) ebox, hbox);
    gtk_widget_show_all (ebox);
    gtk_widget_hide (entry);

    GtkWidget * button = nullptr;

    if (aud_get_bool ("gtkui", "close_button_visible"))
    {
        button = make_close_button (ebox, list);
        gtk_box_pack_end ((GtkBox *) hbox, button, false, false, 0);
    }

    g_object_set_data ((GObject *) ebox, "label", label);
    g_object_set_data ((GObject *) ebox, "entry", entry);
    g_object_set_data ((GObject *) ebox, "page", scrollwin);

    gtk_notebook_insert_page ((GtkNotebook *) pl_notebook, scrollwin, ebox, list_idx);
    gtk_notebook_set_tab_reorderable ((GtkNotebook *) pl_notebook, scrollwin, true);

    g_object_set_data ((GObject *) ebox, "playlist", aud::to_ptr (list));
    g_object_set_data ((GObject *) treeview, "playlist", aud::to_ptr (list));

    int position = list.get_position ();
    if (position >= 0)
        audgui_list_set_highlight (treeview, position);

    int focus = list.get_focus ();
    if (focus >= 0)
        audgui_list_set_focus (treeview, position);

    g_signal_connect (ebox, "button-press-event", (GCallback) tab_button_press_cb, nullptr);
    g_signal_connect (ebox, "key-press-event", (GCallback) tab_key_press_cb, nullptr);
    g_signal_connect (entry, "activate", (GCallback) tab_title_save, ebox);
    g_signal_connect_swapped (vscroll, "value-changed",
     (GCallback) ui_playlist_widget_scroll, treeview);

    /* we have to connect to "scroll-event" on the notebook, the tabs, AND the
     * close buttons (sigh) */
    gtk_widget_add_events (ebox, GDK_SCROLL_MASK);
    g_signal_connect (ebox, "scroll-event", (GCallback) scroll_cb, nullptr);

    if (button)
    {
        gtk_widget_add_events (button, GDK_SCROLL_MASK);
        g_signal_connect (button, "scroll-event", (GCallback) scroll_cb, nullptr);
    }
}

static void switch_to_active ()
{
    int active_idx = Playlist::active_playlist ().index ();
    gtk_notebook_set_current_page ((GtkNotebook *) pl_notebook, active_idx);
}

void pl_notebook_populate ()
{
    int n_playlists = Playlist::n_playlists ();
    for (int idx = 0; idx < n_playlists; idx ++)
        create_tab (idx, Playlist::by_index (idx));

    switch_to_active ();
    highlighted = Playlist::playing_playlist ();

    if (! switch_handler)
        switch_handler = g_signal_connect (pl_notebook, "switch-page",
         (GCallback) tab_changed, nullptr);
    if (! reorder_handler)
        reorder_handler = g_signal_connect (pl_notebook, "page-reordered",
         (GCallback) tab_reordered, nullptr);

    pl_notebook_grab_focus ();
}

void pl_notebook_purge ()
{
    if (switch_handler)
        g_signal_handler_disconnect (pl_notebook, switch_handler);
    switch_handler = 0;
    if (reorder_handler)
        g_signal_handler_disconnect (pl_notebook, reorder_handler);
    reorder_handler = 0;

    int n_pages = gtk_notebook_get_n_pages ((GtkNotebook *) pl_notebook);
    while (n_pages)
        gtk_notebook_remove_page ((GtkNotebook *) pl_notebook, -- n_pages);
}

static void add_remove_pages ()
{
    g_signal_handlers_block_by_func (pl_notebook, (void *) tab_changed, nullptr);
    g_signal_handlers_block_by_func (pl_notebook, (void *) tab_reordered, nullptr);

    int lists = Playlist::n_playlists ();
    int pages = gtk_notebook_get_n_pages ((GtkNotebook *) pl_notebook);

    /* scan through existing treeviews */
    for (int i = 0; i < pages; )
    {
        auto list0 = list_of (treeview_at_idx (i));

        /* do we have an orphaned treeview? */
        if (! list0.exists ())
        {
            gtk_notebook_remove_page ((GtkNotebook *) pl_notebook, i);
            pages --;
            continue;
        }

        /* do we have the right treeview? */
        auto list = Playlist::by_index (i);

        if (list0 == list)
        {
            i ++;
            continue;
        }

        /* look for the right treeview */
        int found = false;

        for (int j = i + 1; j < pages; j ++)
        {
            GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) pl_notebook, j);
            auto list2 = list_of (treeview_of (page));

            /* found it? move it to the right place */
            if (list2 == list)
            {
                gtk_notebook_reorder_child ((GtkNotebook *) pl_notebook, page, i);
                found = true;
                break;
            }
        }

        /* didn't find it? create it */
        if (! found)
        {
            create_tab (i, list);
            pages ++;
            continue;
        }
    }

    /* create new treeviews */
    while (pages < lists)
    {
        create_tab (pages, Playlist::by_index (pages));
        pages ++;
    }

    switch_to_active ();
    show_hide_playlist_tabs ();

    g_signal_handlers_unblock_by_func (pl_notebook, (void *) tab_changed, nullptr);
    g_signal_handlers_unblock_by_func (pl_notebook, (void *) tab_reordered, nullptr);
}

void pl_notebook_update (void * data, void * user)
{
    auto global_level = aud::from_ptr<Playlist::UpdateLevel> (data);
    if (global_level == Playlist::Structure)
        add_remove_pages ();

    int n_pages = gtk_notebook_get_n_pages ((GtkNotebook *) pl_notebook);

    for (int i = 0; i < n_pages; i ++)
    {
        GtkWidget * treeview = treeview_at_idx (i);

        if (global_level >= Playlist::Metadata)
            update_tab_label (get_tab_label (i), list_of (treeview));

        ui_playlist_widget_update (treeview);
    }

    switch_to_active ();
}

void pl_notebook_set_position (void * data, void * user)
{
    auto list = aud::from_ptr<Playlist> (data);
    int row = list.get_position ();

    if (aud_get_bool ("gtkui", "autoscroll"))
    {
        list.select_all (false);
        list.select_entry (row, true);
        list.set_focus (row);
    }

    audgui_list_set_highlight (treeview_at_idx (list.index ()), row);
}

void pl_notebook_activate (void * data, void * user)
{
    switch_to_active ();
}

void pl_notebook_set_playing (void * data, void * user)
{
    auto playing = Playlist::playing_playlist ();

    // if the previous playing playlist was deleted, ignore it
    if (! highlighted.exists ())
        highlighted = Playlist ();

    if (highlighted == playing)
        return;

    int pages = gtk_notebook_get_n_pages ((GtkNotebook *) pl_notebook);

    for (int i = 0; i < pages; i ++)
    {
        auto list = list_of (treeview_at_idx (i));
        if (list == highlighted || list == playing)
            update_tab_label (get_tab_label (i), list);
    }

    highlighted = playing;
}

static void destroy_cb ()
{
    pl_notebook = nullptr;
    switch_handler = 0;
    reorder_handler = 0;
}

GtkWidget * pl_notebook_new ()
{
    pl_notebook = gtk_notebook_new ();
    gtk_notebook_set_scrollable ((GtkNotebook *) pl_notebook, true);
    make_add_button (pl_notebook);

    show_hide_playlist_tabs ();

    gtk_widget_add_events (pl_notebook, GDK_SCROLL_MASK);
    g_signal_connect (pl_notebook, "scroll-event", (GCallback) scroll_cb, nullptr);
    g_signal_connect (pl_notebook, "destroy", (GCallback) destroy_cb, nullptr);

    return pl_notebook;
}

void show_hide_playlist_tabs ()
{
    gtk_notebook_set_show_tabs ((GtkNotebook *) pl_notebook, aud_get_bool ("gtkui",
     "playlist_tabs_visible") || Playlist::n_playlists () > 1);
}
