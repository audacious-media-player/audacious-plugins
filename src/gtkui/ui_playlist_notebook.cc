/*
 * ui_playlist_notebook.c
 * Copyright 2010-2012 Michał Lipski and John Lindgren
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

#include <libaudcore/runtime.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/list.h>
#include <libaudgui/libaudgui.h>

#include "gtkui.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "playlist_util.h"

static GtkWidget * notebook = nullptr;
static int highlighted = -1;

static int switch_handler = 0;
static int reorder_handler = 0;

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
    int current = gtk_notebook_get_current_page ((GtkNotebook *) notebook);

    if (current < 0 || treeview != playlist_get_treeview (current))
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
        int count = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);

        for (int i = 0; i < count; i ++)
        {
            if (i != current)
                apply_column_widths (playlist_get_treeview (i));
        }
    }
}

static void add_button_cb (GtkButton * button)
{
    aud_playlist_insert (-1);
    aud_playlist_set_active (aud_playlist_count () - 1);
}

static void make_add_button (GtkWidget * notebook)
{
    GtkWidget * button = gtk_button_new ();
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("list-add", GTK_ICON_SIZE_MENU));
    gtk_widget_set_can_focus (button, false);

    g_signal_connect (button, "clicked", (GCallback) add_button_cb, nullptr);
    gtk_widget_show_all (button);

    gtk_notebook_set_action_widget ((GtkNotebook *) notebook, button, GTK_PACK_END);
}

static void close_button_cb (GtkWidget * button, void * id)
{
    audgui_confirm_playlist_delete (aud_playlist_by_unique_id (GPOINTER_TO_INT (id)));
}

static void close_button_style_set (GtkWidget * button)
{
    int w, h;
    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (button),
     GTK_ICON_SIZE_MENU, & w, & h);
    gtk_widget_set_size_request (button, w + 2, h + 2);
}

static GtkWidget * make_close_button (GtkWidget * ebox, int list)
{
    GtkWidget * button = gtk_button_new ();
    GtkWidget * image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
    gtk_button_set_image ((GtkButton *) button, image);
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click ((GtkButton *) button, false);
    gtk_widget_set_name (button, "gtkui-tab-close-button");

    g_signal_connect (button, "clicked", (GCallback) close_button_cb,
     GINT_TO_POINTER (aud_playlist_get_unique_id (list)));

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

GtkNotebook * ui_playlist_get_notebook ()
{
    return (GtkNotebook *) notebook;
}

static void tab_title_reset (GtkWidget * ebox)
{
    GtkWidget * label = (GtkWidget *) g_object_get_data ((GObject *) ebox, "label");
    GtkWidget * entry = (GtkWidget *) g_object_get_data ((GObject *) ebox, "entry");
    gtk_widget_hide (entry);
    gtk_widget_show (label);
}

static void tab_title_save (GtkEntry * entry, void * ebox)
{
    int id = GPOINTER_TO_INT (g_object_get_data ((GObject *) ebox, "playlist-id"));
    GtkWidget * label = (GtkWidget *) g_object_get_data ((GObject *) ebox, "label");

    aud_playlist_set_title (aud_playlist_by_unique_id (id), gtk_entry_get_text (entry));
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
    int id = GPOINTER_TO_INT (g_object_get_data ((GObject *) ebox, "playlist-id"));
    int playlist = aud_playlist_by_unique_id (id);

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
        aud_playlist_play (playlist);

    if (event->type == GDK_BUTTON_PRESS && event->button == 2)
        audgui_confirm_playlist_delete (playlist);

    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
        popup_menu_tab (event->button, event->time, playlist);

    return false;
}

static gboolean scroll_cb (GtkWidget * widget, GdkEventScroll * event)
{
    switch (event->direction)
    {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_LEFT:
        aud_playlist_set_active (aud_playlist_get_active () - 1);
        return true;

    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_RIGHT:
        aud_playlist_set_active (aud_playlist_get_active () + 1);
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
    aud_playlist_set_active (page_num);
}

static void tab_reordered (GtkNotebook * notebook, GtkWidget * child, unsigned page_num)
{
    GtkWidget * widget = (GtkWidget *) g_object_get_data ((GObject *) child, "treeview");
    g_return_if_fail (widget);
    aud_playlist_reorder (ui_playlist_widget_get_playlist (widget), page_num, 1);
}

static GtkLabel * get_tab_label (int playlist)
{
    GtkWidget * page = gtk_notebook_get_nth_page (UI_PLAYLIST_NOTEBOOK, playlist);
    GtkWidget * ebox = gtk_notebook_get_tab_label (UI_PLAYLIST_NOTEBOOK, page);
    return (GtkLabel *) g_object_get_data ((GObject *) ebox, "label");
}

static void set_tab_label (int list, GtkLabel * label)
{
    String title0 = aud_playlist_get_title (list);
    StringBuf title = aud_get_bool ("gtkui", "entry_count_visible") ?
     str_printf ("%s (%d)", (const char *) title0, aud_playlist_entry_count (list)) :
     str_copy (title0);

    if (list == aud_playlist_get_playing ())
    {
        char * markup = g_markup_printf_escaped ("<b>%s</b>", (const char *) title);
        gtk_label_set_markup (label, markup);
        g_free (markup);
    }
    else
        gtk_label_set_text (label, title);
}

void start_rename_playlist (int playlist)
{
    if (! gtk_notebook_get_show_tabs ((GtkNotebook *) notebook))
    {
        audgui_show_playlist_rename (playlist);
        return;
    }

    GtkWidget * page = gtk_notebook_get_nth_page (UI_PLAYLIST_NOTEBOOK, playlist);
    GtkWidget * ebox = gtk_notebook_get_tab_label (UI_PLAYLIST_NOTEBOOK, page);

    GtkWidget * label = (GtkWidget *) g_object_get_data ((GObject *) ebox, "label");
    GtkWidget * entry = (GtkWidget *) g_object_get_data ((GObject *) ebox, "entry");
    gtk_widget_hide (label);

    gtk_entry_set_text ((GtkEntry *) entry, aud_playlist_get_title (playlist));

    gtk_widget_grab_focus (entry);
    gtk_editable_select_region ((GtkEditable *) entry, 0, -1);
    gtk_widget_show (entry);
}

void ui_playlist_notebook_create_tab (int playlist)
{
    int position = aud_playlist_get_position (playlist);

    GtkWidget * scrollwin = gtk_scrolled_window_new (nullptr, nullptr);
    GtkAdjustment * vscroll = gtk_scrolled_window_get_vadjustment ((GtkScrolledWindow *) scrollwin);

    /* do not allow scroll events to propagate up to the notebook */
    g_signal_connect_after (scrollwin, "scroll-event", (GCallback) scroll_ignore_cb, nullptr);

    GtkWidget * treeview = ui_playlist_widget_new (playlist);

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
    set_tab_label (playlist, (GtkLabel *) label);
    gtk_box_pack_start ((GtkBox *) hbox, label, false, false, 0);

    GtkWidget * entry = gtk_entry_new ();
    gtk_box_pack_start ((GtkBox *) hbox, entry, false, false, 0);
    gtk_container_add ((GtkContainer *) ebox, hbox);
    gtk_widget_show_all (ebox);
    gtk_widget_hide (entry);

    GtkWidget * button = nullptr;

    if (aud_get_bool ("gtkui", "close_button_visible"))
    {
        button = make_close_button (ebox, playlist);
        gtk_box_pack_end ((GtkBox *) hbox, button, false, false, 0);
    }

    g_object_set_data ((GObject *) ebox, "label", label);
    g_object_set_data ((GObject *) ebox, "entry", entry);
    g_object_set_data ((GObject *) ebox, "page", scrollwin);

    gtk_notebook_insert_page (UI_PLAYLIST_NOTEBOOK, scrollwin, ebox, playlist);
    gtk_notebook_set_tab_reorderable (UI_PLAYLIST_NOTEBOOK, scrollwin, true);

    int id = aud_playlist_get_unique_id (playlist);
    g_object_set_data ((GObject *) ebox, "playlist-id", GINT_TO_POINTER (id));
    g_object_set_data ((GObject *) treeview, "playlist-id", GINT_TO_POINTER (id));

    if (position >= 0)
    {
        aud_playlist_select_all (playlist, false);
        aud_playlist_entry_set_selected (playlist, position, true);
        aud_playlist_set_focus (playlist, position);
        audgui_list_set_highlight (treeview, position);
        audgui_list_set_focus (treeview, position);
    }

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

void ui_playlist_notebook_populate ()
{
    int playlist = aud_playlist_get_active ();

    for (int count = 0; count < aud_playlist_count (); count ++)
        ui_playlist_notebook_create_tab (count);

    gtk_notebook_set_current_page (UI_PLAYLIST_NOTEBOOK, playlist);
    highlighted = aud_playlist_get_unique_id (aud_playlist_get_playing ());

    if (! switch_handler)
        switch_handler = g_signal_connect (notebook, "switch-page", (GCallback)
         tab_changed, nullptr);
    if (! reorder_handler)
        reorder_handler = g_signal_connect (notebook, "page-reordered",
         (GCallback) tab_reordered, nullptr);

    gtk_widget_grab_focus (playlist_get_treeview (playlist));
}

void ui_playlist_notebook_empty ()
{
    if (switch_handler)
        g_signal_handler_disconnect (notebook, switch_handler);
    switch_handler = 0;
    if (reorder_handler)
        g_signal_handler_disconnect (notebook, reorder_handler);
    reorder_handler = 0;

    int n_pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);
    while (n_pages)
        gtk_notebook_remove_page ((GtkNotebook *) notebook, -- n_pages);
}

static void add_remove_pages ()
{
    g_signal_handlers_block_by_func (notebook, (void *) tab_changed, nullptr);
    g_signal_handlers_block_by_func (notebook, (void *) tab_reordered, nullptr);

    int lists = aud_playlist_count ();
    int pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);

    /* scan through existing treeviews */
    for (int i = 0; i < pages; )
    {
        GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) notebook, i);
        GtkWidget * tree = (GtkWidget *) g_object_get_data ((GObject *) page, "treeview");
        int tree_id = GPOINTER_TO_INT (g_object_get_data ((GObject *) tree, "playlist-id"));

        /* do we have an orphaned treeview? */
        if (aud_playlist_by_unique_id (tree_id) < 0)
        {
            gtk_notebook_remove_page ((GtkNotebook *) notebook, i);
            pages --;
            continue;
        }

        /* do we have the right treeview? */
        int list_id = aud_playlist_get_unique_id (i);

        if (tree_id == list_id)
        {
            ui_playlist_widget_set_playlist (tree, i);
            i ++;
            continue;
        }

        /* look for the right treeview */
        int found = false;

        for (int j = i + 1; j < pages; j ++)
        {
            page = gtk_notebook_get_nth_page ((GtkNotebook *) notebook, j);
            tree = (GtkWidget *) g_object_get_data ((GObject *) page, "treeview");
            tree_id = GPOINTER_TO_INT (g_object_get_data ((GObject *) tree, "playlist-id"));

            /* found it? move it to the right place */
            if (tree_id == list_id)
            {
                gtk_notebook_reorder_child ((GtkNotebook *) notebook, page, i);
                found = true;
                break;
            }
        }

        /* didn't find it? create it */
        if (! found)
        {
            ui_playlist_notebook_create_tab (i);
            pages ++;
            continue;
        }
    }

    /* create new treeviews */
    while (pages < lists)
    {
        ui_playlist_notebook_create_tab (pages);
        pages ++;
    }

    gtk_notebook_set_current_page ((GtkNotebook *) notebook, aud_playlist_get_active ());

    show_hide_playlist_tabs ();

    g_signal_handlers_unblock_by_func (notebook, (void *) tab_changed, nullptr);
    g_signal_handlers_unblock_by_func (notebook, (void *) tab_reordered, nullptr);
}

void ui_playlist_notebook_update (void * data, void * user)
{
    auto global_level = aud::from_ptr<Playlist::UpdateLevel> (data);
    if (global_level == Playlist::Structure)
        add_remove_pages ();

    int lists = aud_playlist_count ();

    for (int list = 0; list < lists; list ++)
    {
        if (global_level >= Playlist::Metadata)
            set_tab_label (list, get_tab_label (list));

        GtkWidget * treeview = playlist_get_treeview (list);

        auto update = aud_playlist_update_detail (list);
        if (update.level)
            ui_playlist_widget_update (treeview, update);

        audgui_list_set_highlight (treeview, aud_playlist_get_position (list));
    }

    gtk_notebook_set_current_page ((GtkNotebook *) notebook, aud_playlist_get_active ());
}

void ui_playlist_notebook_position (void * data, void * user)
{
    int list = aud::from_ptr<int> (data);
    int row = aud_playlist_get_position (list);

    if (aud_get_bool ("gtkui", "autoscroll"))
    {
        aud_playlist_select_all (list, false);
        aud_playlist_entry_set_selected (list, row, true);
        aud_playlist_set_focus (list, row);
    }

    if (! aud_playlist_update_pending ())
        audgui_list_set_highlight (playlist_get_treeview (list), row);
}

void ui_playlist_notebook_activate (void * data, void * user)
{
    if (! aud_playlist_update_pending ())
        gtk_notebook_set_current_page ((GtkNotebook *) notebook, aud_playlist_get_active ());
}

void ui_playlist_notebook_set_playing (void * data, void * user)
{
    int id = aud_playlist_get_unique_id (aud_playlist_get_playing ());

    // if the previous playing playlist was deleted, ignore it
    if (aud_playlist_by_unique_id (highlighted) < 0)
        highlighted = -1;

    if (highlighted == id)
        return;

    int pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);

    for (int i = 0; i < pages; i ++)
    {
        GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) notebook, i);
        GtkWidget * tree = (GtkWidget *) g_object_get_data ((GObject *) page, "treeview");
        int tree_id = GPOINTER_TO_INT (g_object_get_data ((GObject *) tree, "playlist-id"));

        if (tree_id == highlighted || tree_id == id)
            set_tab_label (i, get_tab_label (i));
    }

    highlighted = id;
}

static void destroy_cb ()
{
    notebook = nullptr;
    switch_handler = 0;
    reorder_handler = 0;
}

GtkWidget * ui_playlist_notebook_new ()
{
    notebook = gtk_notebook_new ();
    gtk_notebook_set_scrollable ((GtkNotebook *) notebook, true);
    make_add_button (notebook);

    show_hide_playlist_tabs ();

    gtk_widget_add_events (notebook, GDK_SCROLL_MASK);
    g_signal_connect (notebook, "scroll-event", (GCallback) scroll_cb, nullptr);
    g_signal_connect (notebook, "destroy", (GCallback) destroy_cb, nullptr);

    return notebook;
}

void show_hide_playlist_tabs ()
{
    gtk_notebook_set_show_tabs ((GtkNotebook *) notebook, aud_get_bool ("gtkui",
     "playlist_tabs_visible") || aud_playlist_count () > 1);
}
