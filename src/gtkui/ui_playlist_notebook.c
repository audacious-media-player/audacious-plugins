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

#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>
#include <libaudgui/list.h>
#include <libaudgui/libaudgui.h>

#include "gtkui.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "playlist_util.h"

static GtkWidget * notebook = NULL;
static gint highlighted = -1;

static gint switch_handler = 0;
static gint reorder_handler = 0;

static void add_button_cb (GtkButton * button, void * unused)
{
    aud_playlist_insert (-1);
    aud_playlist_set_active (aud_playlist_count () - 1);
}

static void make_add_button (GtkWidget * notebook)
{
    GtkWidget * button = gtk_button_new ();
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_stock
     (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
    gtk_widget_set_can_focus (button, FALSE);

    g_signal_connect (button, "clicked", (GCallback) add_button_cb, NULL);
    gtk_widget_show_all (button);

    gtk_notebook_set_action_widget ((GtkNotebook *) notebook, button, GTK_PACK_END);
}

static void close_button_cb (GtkWidget * button, void * id)
{
    audgui_confirm_playlist_delete (aud_playlist_by_unique_id (GPOINTER_TO_INT (id)));
}

static GtkWidget * make_close_button (GtkWidget * ebox, gint list)
{
    GtkWidget * button = gtk_button_new ();
    GtkWidget * image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
    gtk_button_set_image ((GtkButton *) button, image);
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click ((GtkButton *) button, FALSE);
    gtk_widget_set_name (button, "gtkui-tab-close-button");
    g_object_set_data ((GObject *) ebox, "close_button", button);
    g_signal_connect (button, "clicked", (GCallback) close_button_cb,
     GINT_TO_POINTER (aud_playlist_get_unique_id (list)));

    GtkCssProvider * provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (provider,
     "#gtkui-tab-close-button {"
     " -GtkButton-default-border: 0;"
     " -GtkButton-default-outside-border: 0;"
     " -GtkButton-inner-border: 0;"
     " -GtkWidget-focus-padding: 0;"
     " -GtkWidget-focus-line-width: 0;"
     " margin: 0;"
     " padding: 0; }",
     -1, NULL);

    gtk_style_context_add_provider (gtk_widget_get_style_context (button),
     GTK_STYLE_PROVIDER (provider),
     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);

    if (aud_get_bool ("gtkui", "close_button_visible"))
        gtk_widget_show (button);

    return button;
}

void show_close_buttons (gboolean show)
{
    aud_set_bool ("gtkui", "close_button_visible", show);

    gint pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);

    for (gint i = 0; i < pages; i ++)
    {
        GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) notebook, i);
        GtkWidget * ebox = gtk_notebook_get_tab_label ((GtkNotebook *) notebook, page);
        GtkWidget * button = g_object_get_data ((GObject *) ebox, "close_button");

        if (show)
            gtk_widget_show (button);
        else
            gtk_widget_hide (button);
    }
}

GtkNotebook *ui_playlist_get_notebook(void)
{
    return GTK_NOTEBOOK(notebook);
}

static void save_column_widths ()
{
    int current = gtk_notebook_get_current_page ((GtkNotebook *) notebook);
    GtkWidget * treeview = playlist_get_treeview (current);

    char * widths, * expand;
    ui_playlist_widget_get_column_widths (treeview, & widths, & expand);

    aud_set_string ("gtkui", "column_widths", widths);
    aud_set_string ("gtkui", "column_expand", expand);

    free (widths);
    free (expand);
}

static void apply_column_widths (GtkWidget * treeview)
{
    char * widths = aud_get_string ("gtkui", "column_widths");
    char * expand = aud_get_string ("gtkui", "column_expand");

    if (widths && widths[0] && expand && expand[0])
        ui_playlist_widget_set_column_widths (treeview, widths, expand);

    free (widths);
    free (expand);
}

static void tab_title_reset(GtkWidget *ebox)
{
    GtkWidget *label = g_object_get_data(G_OBJECT(ebox), "label");
    GtkWidget *entry = g_object_get_data(G_OBJECT(ebox), "entry");
    gtk_widget_hide(entry);
    gtk_widget_show(label);
}

static void tab_title_save(GtkEntry *entry, gpointer ebox)
{
    int id = GPOINTER_TO_INT (g_object_get_data ((GObject *) ebox, "playlist-id"));
    GtkWidget *label = g_object_get_data(G_OBJECT(ebox), "label");

    aud_playlist_set_title (aud_playlist_by_unique_id (id), gtk_entry_get_text (entry));
    gtk_widget_hide(GTK_WIDGET(entry));
    gtk_widget_show(label);
}

static gboolean tab_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    if (event->keyval == GDK_KEY_Escape)
        tab_title_reset(widget);

    return FALSE;
}

static gboolean tab_button_press_cb(GtkWidget *ebox, GdkEventButton *event, gpointer user_data)
{
    int id = GPOINTER_TO_INT (g_object_get_data ((GObject *) ebox, "playlist-id"));
    int playlist = aud_playlist_by_unique_id (id);

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
        aud_drct_play_playlist (playlist);

    if (event->type == GDK_BUTTON_PRESS && event->button == 2)
        audgui_confirm_playlist_delete (playlist);

    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
        popup_menu_tab (event->button, event->time, playlist);

    return FALSE;
}

static bool_t scroll_cb (GtkWidget * widget, GdkEventScroll * event)
{
    switch (event->direction)
    {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_LEFT:
        aud_playlist_set_active (aud_playlist_get_active () - 1);
        return TRUE;

    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_RIGHT:
        aud_playlist_set_active (aud_playlist_get_active () + 1);
        return TRUE;

    default:
        return FALSE;
    }
}

static void tab_changed (GtkNotebook * notebook, GtkWidget * page, gint
 page_num, void * unused)
{
    save_column_widths ();
    apply_column_widths (playlist_get_treeview (page_num));

    aud_playlist_set_active (page_num);
}

static void tab_reordered(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
    GtkWidget * widget = g_object_get_data ((GObject *) child, "treeview");
    g_return_if_fail (widget);
    aud_playlist_reorder (ui_playlist_widget_get_playlist (widget), page_num, 1);
}

static GtkLabel *get_tab_label(gint playlist)
{
    GtkWidget *page = gtk_notebook_get_nth_page(UI_PLAYLIST_NOTEBOOK, playlist);
    GtkWidget *ebox = gtk_notebook_get_tab_label(UI_PLAYLIST_NOTEBOOK, page);
    return GTK_LABEL(g_object_get_data(G_OBJECT(ebox), "label"));
}

static void set_tab_label (gint list, GtkLabel * label)
{
    gchar * title = aud_playlist_get_title (list);

    if (list == aud_playlist_get_playing ())
    {
        gchar * markup = g_markup_printf_escaped ("<b>%s</b>", title);
        gtk_label_set_markup (label, markup);
        g_free (markup);
    }
    else
        gtk_label_set_text (label, title);

    str_unref (title);
}

void ui_playlist_notebook_edit_tab_title (int playlist)
{
    GtkWidget * page = gtk_notebook_get_nth_page (UI_PLAYLIST_NOTEBOOK, playlist);
    GtkWidget * ebox = gtk_notebook_get_tab_label (UI_PLAYLIST_NOTEBOOK, page);

    GtkWidget *label = g_object_get_data(G_OBJECT(ebox), "label");
    GtkWidget *entry = g_object_get_data(G_OBJECT(ebox), "entry");
    gtk_widget_hide(label);

    gchar * title = aud_playlist_get_title (playlist);
    gtk_entry_set_text ((GtkEntry *) entry, title);
    str_unref (title);
    gtk_widget_grab_focus(entry);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_show(entry);
}

void ui_playlist_notebook_create_tab(gint playlist)
{
    GtkWidget *scrollwin, *treeview;
    GtkWidget *label, *entry, *ebox, *hbox;
    GtkAdjustment *vscroll;
    gint position = aud_playlist_get_position (playlist);

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    vscroll = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrollwin));

    treeview = ui_playlist_widget_new(playlist);
    apply_column_widths (treeview);

    g_object_set_data(G_OBJECT(scrollwin), "treeview", treeview);

    gtk_container_add(GTK_CONTAINER(scrollwin), treeview);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_show_all(scrollwin);

    ebox = gtk_event_box_new();
    gtk_event_box_set_visible_window ((GtkEventBox *) ebox, FALSE);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

    label = gtk_label_new ("");
    set_tab_label (playlist, (GtkLabel *) label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(ebox), hbox);
    gtk_widget_show_all(ebox);
    gtk_widget_hide(entry);

    GtkWidget * button = make_close_button (ebox, playlist);
    gtk_box_pack_end ((GtkBox *) hbox, button, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(ebox), "label", label);
    g_object_set_data(G_OBJECT(ebox), "entry", entry);
    g_object_set_data(G_OBJECT(ebox), "page", scrollwin);

    gtk_notebook_insert_page (UI_PLAYLIST_NOTEBOOK, scrollwin, ebox, playlist);
    gtk_notebook_set_tab_reorderable(UI_PLAYLIST_NOTEBOOK, scrollwin, TRUE);

    int id = aud_playlist_get_unique_id (playlist);
    g_object_set_data ((GObject *) ebox, "playlist-id", GINT_TO_POINTER (id));
    g_object_set_data ((GObject *) treeview, "playlist-id", GINT_TO_POINTER (id));

    if (position >= 0)
    {
        aud_playlist_select_all (playlist, FALSE);
        aud_playlist_entry_set_selected (playlist, position, TRUE);
        aud_playlist_set_focus (playlist, position);
        audgui_list_set_highlight (treeview, position);
        audgui_list_set_focus (treeview, position);
    }

    g_signal_connect(ebox, "button-press-event", G_CALLBACK(tab_button_press_cb), NULL);
    g_signal_connect(ebox, "key-press-event", G_CALLBACK(tab_key_press_cb), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(tab_title_save), ebox);
    g_signal_connect_swapped (vscroll, "value-changed",
     G_CALLBACK(ui_playlist_widget_scroll), treeview);

    /* we have to connect to "scroll-event" on the notebook, the tabs, AND the
     * close buttons (sigh) */
    gtk_widget_add_events (ebox, GDK_SCROLL_MASK);
    gtk_widget_add_events (button, GDK_SCROLL_MASK);
    g_signal_connect (ebox, "scroll-event", (GCallback) scroll_cb, NULL);
    g_signal_connect (button, "scroll-event", (GCallback) scroll_cb, NULL);
}

void ui_playlist_notebook_populate(void)
{
    gint playlists = aud_playlist_count();
    gint count;

    for (count = 0; count < playlists; count++)
        ui_playlist_notebook_create_tab(count);

    gtk_notebook_set_current_page (UI_PLAYLIST_NOTEBOOK, aud_playlist_get_active ());
    highlighted = aud_playlist_get_unique_id (aud_playlist_get_playing ());

    if (! switch_handler)
        switch_handler = g_signal_connect (notebook, "switch-page", (GCallback)
         tab_changed, NULL);
    if (! reorder_handler)
        reorder_handler = g_signal_connect (notebook, "page-reordered",
         (GCallback) tab_reordered, NULL);

    gtk_widget_grab_focus (playlist_get_treeview (aud_playlist_get_active ()));
}

void ui_playlist_notebook_empty (void)
{
    if (switch_handler)
        g_signal_handler_disconnect (notebook, switch_handler);
    switch_handler = 0;
    if (reorder_handler)
        g_signal_handler_disconnect (notebook, reorder_handler);
    reorder_handler = 0;

    gint n_pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);
    while (n_pages)
        gtk_notebook_remove_page ((GtkNotebook *) notebook, -- n_pages);
}

static void add_remove_pages (void)
{
    g_signal_handlers_block_by_func (notebook, (void *) tab_changed, NULL);
    g_signal_handlers_block_by_func (notebook, (void *) tab_reordered, NULL);

    save_column_widths ();

    gint lists = aud_playlist_count ();
    gint pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);

    /* scan through existing treeviews */
    for (gint i = 0; i < pages; )
    {
        GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) notebook, i);
        GtkWidget * tree = g_object_get_data ((GObject *) page, "treeview");
        gint tree_id = GPOINTER_TO_INT (g_object_get_data ((GObject *) tree, "playlist-id"));

        /* do we have an orphaned treeview? */
        if (aud_playlist_by_unique_id (tree_id) < 0)
        {
            gtk_notebook_remove_page ((GtkNotebook *) notebook, i);
            pages --;
            continue;
        }

        /* do we have the right treeview? */
        gint list_id = aud_playlist_get_unique_id (i);

        if (tree_id == list_id)
        {
            ui_playlist_widget_set_playlist (tree, i);
            i ++;
            continue;
        }

        /* look for the right treeview */
        gint found = FALSE;

        for (gint j = i + 1; j < pages; j ++)
        {
            page = gtk_notebook_get_nth_page ((GtkNotebook *) notebook, j);
            tree = g_object_get_data ((GObject *) page, "treeview");
            tree_id = GPOINTER_TO_INT (g_object_get_data ((GObject *) tree, "playlist-id"));

            /* found it? move it to the right place */
            if (tree_id == list_id)
            {
                gtk_notebook_reorder_child ((GtkNotebook *) notebook, page, i);
                found = TRUE;
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

    int active = aud_playlist_get_active ();
    apply_column_widths (playlist_get_treeview (active));
    gtk_notebook_set_current_page ((GtkNotebook *) notebook, active);

    g_signal_handlers_unblock_by_func (notebook, (void *) tab_changed, NULL);
    g_signal_handlers_unblock_by_func (notebook, (void *) tab_reordered, NULL);
}

void ui_playlist_notebook_update (void * data, void * user)
{
    gint global_level = GPOINTER_TO_INT (data);

    if (global_level == PLAYLIST_UPDATE_STRUCTURE)
        add_remove_pages ();

    gint lists = aud_playlist_count ();

    for (gint list = 0; list < lists; list ++)
    {
        if (global_level >= PLAYLIST_UPDATE_METADATA)
            set_tab_label (list, get_tab_label (list));

        GtkWidget * treeview = playlist_get_treeview (list);

        gint at, count;
        gint level = aud_playlist_updated_range (list, & at, & count);

        if (level)
            ui_playlist_widget_update (treeview, level, at, count);

        audgui_list_set_highlight (treeview, aud_playlist_get_position (list));
    }

    gtk_notebook_set_current_page ((GtkNotebook *) notebook, aud_playlist_get_active ());
}

void ui_playlist_notebook_position (void * data, void * user)
{
    gint list = GPOINTER_TO_INT (data);
    gint row = aud_playlist_get_position (list);

    if (aud_get_bool ("gtkui", "autoscroll"))
    {
        aud_playlist_select_all (list, FALSE);
        aud_playlist_entry_set_selected (list, row, TRUE);
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
    gint new = aud_playlist_get_unique_id (aud_playlist_get_playing ());

    if (highlighted == new)
        return;

    gint pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);

    for (gint i = 0; i < pages; i ++)
    {
        GtkWidget * page = gtk_notebook_get_nth_page ((GtkNotebook *) notebook, i);
        GtkWidget * tree = g_object_get_data ((GObject *) page, "treeview");
        gint tree_id = GPOINTER_TO_INT (g_object_get_data ((GObject *) tree, "playlist-id"));

        if (tree_id == highlighted || tree_id == new)
            set_tab_label (i, get_tab_label (i));
    }

    highlighted = new;
}

static void destroy_cb (void)
{
    hook_dissociate ("config save", (HookFunction) save_column_widths);

    notebook = NULL;
    switch_handler = 0;
    reorder_handler = 0;
}

GtkWidget * ui_playlist_notebook_new (void)
{
    notebook = gtk_notebook_new ();
    gtk_notebook_set_scrollable ((GtkNotebook *) notebook, TRUE);
    make_add_button (notebook);

    hook_associate ("config save", (HookFunction) save_column_widths, NULL);

    gtk_widget_add_events (notebook, GDK_SCROLL_MASK);
    g_signal_connect (notebook, "scroll-event", (GCallback) scroll_cb, NULL);

    g_signal_connect (notebook, "destroy", (GCallback) destroy_cb, NULL);
    return notebook;
}

void playlist_show_headers (gboolean show)
{
    gboolean old = aud_get_bool ("gtkui", "playlist_headers");
    if (old == show)
        return;

    aud_set_bool ("gtkui", "playlist_headers", show);
    ui_playlist_notebook_empty ();
    ui_playlist_notebook_populate ();
}
