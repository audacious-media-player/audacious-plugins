/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include "config.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/glib-compat.h>
#include <audacious/gtk-compat.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudgui/list.h>
#include <libaudgui/libaudgui.h>

#include "gtkui_cfg.h"
#include "ui_manager.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "playlist_util.h"

static GtkWidget * notebook = NULL;
static GQueue follow_queue = G_QUEUE_INIT;
static gint bolded_playlist = -1;

static struct index *pages;
GtkWidget *ui_playlist_notebook_tab_title_editing = NULL;

static gint switch_handler = 0;

#if GTK_CHECK_VERSION (2, 10, 0)
#define HAVE_REORDER
static gint reorder_handler = 0;
#endif

GtkNotebook *ui_playlist_get_notebook(void)
{
    return GTK_NOTEBOOK(notebook);
}

static void tab_title_reset(GtkWidget *ebox)
{
    GtkWidget *label = g_object_get_data(G_OBJECT(ebox), "label");
    GtkWidget *entry = g_object_get_data(G_OBJECT(ebox), "entry");
    gtk_widget_hide(entry);
    gtk_widget_show(label);

    ui_playlist_notebook_tab_title_editing = NULL;
}

static void tab_title_save(GtkEntry *entry, gpointer ebox)
{
    GtkWidget *label = g_object_get_data(G_OBJECT(ebox), "label");

    aud_playlist_set_title(aud_playlist_get_active(), gtk_entry_get_text(entry));
    gtk_widget_hide(GTK_WIDGET(entry));
    gtk_widget_show(label);

    ui_playlist_notebook_tab_title_editing = NULL;
}

static gboolean tab_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    if (event->keyval == GDK_Escape)
        tab_title_reset(widget);

    return FALSE;
}

static gboolean tab_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
        ui_playlist_notebook_edit_tab_title(widget);

    if (event->type == GDK_BUTTON_PRESS && event->button == 2)
    {
        GtkWidget *page = g_object_get_data(G_OBJECT(widget), "page");
        audgui_confirm_playlist_delete(gtk_notebook_page_num(UI_PLAYLIST_NOTEBOOK, page));
    }

    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        GtkWidget *page = g_object_get_data(G_OBJECT(widget), "page");

        gtk_notebook_set_current_page(UI_PLAYLIST_NOTEBOOK, gtk_notebook_page_num(UI_PLAYLIST_NOTEBOOK, page));
        ui_manager_popup_menu_show(GTK_MENU(playlist_tab_menu), event->x_root, event->y_root + 2, 3, event->time);
    }

    return FALSE;
}

static void tab_changed (GtkNotebook * notebook, GtkWidget * page, gint
 page_num, void * unused)
{
    GtkWidget * treeview = playlist_get_treeview (page_num);

    if (treeview != NULL)
    {
        aud_playlist_set_active (page_num);

        if (ui_playlist_notebook_tab_title_editing != NULL)
            tab_title_reset(ui_playlist_notebook_tab_title_editing);
    }
}

#ifdef HAVE_REORDER
static void tab_reordered(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
    GtkWidget * widget = g_object_get_data ((GObject *) child, "treeview");
    g_return_if_fail (widget);
    aud_playlist_reorder (ui_playlist_widget_get_playlist (widget), page_num, 1);
}
#endif

static GtkLabel *get_tab_label(gint playlist)
{
    GtkWidget *page = gtk_notebook_get_nth_page(UI_PLAYLIST_NOTEBOOK, playlist);
    GtkWidget *ebox = gtk_notebook_get_tab_label(UI_PLAYLIST_NOTEBOOK, page);
    return GTK_LABEL(g_object_get_data(G_OBJECT(ebox), "label"));
}

static void set_tab_label (gint list, GtkLabel * label)
{
    const gchar * title = aud_playlist_get_title (list);

    if (list == aud_playlist_get_playing ())
    {
        gchar * markup = g_markup_printf_escaped ("<b>%s</b>", title);
        gtk_label_set_markup (label, markup);
        g_free (markup);
    }
    else
        gtk_label_set_text (label, title);
}

void ui_playlist_notebook_edit_tab_title(GtkWidget *ebox)
{
    if (!gtk_notebook_get_show_tabs(UI_PLAYLIST_NOTEBOOK))
        return;

    if (ebox == NULL || !GTK_IS_EVENT_BOX(ebox))
    {
        GtkWidget *page = gtk_notebook_get_nth_page(UI_PLAYLIST_NOTEBOOK, aud_playlist_get_active());
        ebox = gtk_notebook_get_tab_label(UI_PLAYLIST_NOTEBOOK, page);
    }

    GtkWidget *label = g_object_get_data(G_OBJECT(ebox), "label");
    GtkWidget *entry = g_object_get_data(G_OBJECT(ebox), "entry");
    gtk_widget_hide(label);

    gtk_entry_set_text(GTK_ENTRY(entry), aud_playlist_get_title(aud_playlist_get_active()));
    gtk_widget_grab_focus(entry);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_show(entry);

    ui_playlist_notebook_tab_title_editing = ebox;
}

static void change_view (void)
{
    gint count = index_count (pages);
    if (! count)
        return;

    if (count > 1)
    {
        gtk_notebook_set_show_tabs (UI_PLAYLIST_NOTEBOOK, TRUE);

        for (gint i = 0; i < count; i ++)
            gtk_container_set_border_width (index_get (pages, i), 0);
    }
    else
    {
        gtk_notebook_set_show_tabs (UI_PLAYLIST_NOTEBOOK, FALSE);
        gtk_container_set_border_width (index_get (pages, 0), 0);
    }
}

void ui_playlist_notebook_create_tab(gint playlist)
{
    GtkWidget *scrollwin, *treeview;
    GtkWidget *label, *entry, *ebox, *hbox;
    gint position = aud_playlist_get_position (playlist);

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width ((GtkContainer *) scrollwin, 0);
    index_insert(pages, playlist, scrollwin);

    treeview = ui_playlist_widget_new(playlist);
    g_object_set_data(G_OBJECT(scrollwin), "treeview", treeview);

    gtk_container_add(GTK_CONTAINER(scrollwin), treeview);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwin), GTK_SHADOW_IN);
    gtk_widget_show_all(scrollwin);

    ebox = gtk_event_box_new();
    gtk_event_box_set_visible_window ((GtkEventBox *) ebox, FALSE);

    hbox = gtk_hbox_new(FALSE, 2);

    label = gtk_label_new ("");
    set_tab_label (playlist, (GtkLabel *) label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(ebox), hbox);
    gtk_widget_show_all(ebox);
    gtk_widget_hide(entry);

    g_object_set_data(G_OBJECT(ebox), "label", label);
    g_object_set_data(G_OBJECT(ebox), "entry", entry);
    g_object_set_data(G_OBJECT(ebox), "page", scrollwin);

    gtk_notebook_append_page(UI_PLAYLIST_NOTEBOOK, scrollwin, ebox);
#ifdef HAVE_REORDER
    gtk_notebook_set_tab_reorderable(UI_PLAYLIST_NOTEBOOK, scrollwin, TRUE);
#endif
    change_view ();

    if (position >= 0)
    {
        aud_playlist_select_all (playlist, FALSE);
        aud_playlist_entry_set_selected (playlist, position, TRUE);
        audgui_list_set_highlight (treeview, position);
        audgui_list_set_focus (treeview, position);
    }

    g_signal_connect(ebox, "button-press-event", G_CALLBACK(tab_button_press_cb), NULL);
    g_signal_connect(ebox, "key-press-event", G_CALLBACK(tab_key_press_cb), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(tab_title_save), ebox);
}

void ui_playlist_notebook_destroy_tab(gint playlist)
{
    GtkWidget *page = index_get(pages, playlist);

    gtk_notebook_remove_page(UI_PLAYLIST_NOTEBOOK, gtk_notebook_page_num(UI_PLAYLIST_NOTEBOOK, page));
    index_delete(pages, playlist, 1);
    change_view ();
}

void ui_playlist_notebook_populate(void)
{
    gint playlists = aud_playlist_count();
    gint count;

    pages = index_new();

    for (count = 0; count < playlists; count++)
        ui_playlist_notebook_create_tab(count);

    gtk_notebook_set_current_page (UI_PLAYLIST_NOTEBOOK, aud_playlist_get_active ());
    bolded_playlist = aud_playlist_get_playing ();

    if (! switch_handler)
        switch_handler = g_signal_connect (notebook, "switch-page", (GCallback)
         tab_changed, NULL);
#ifdef HAVE_REORDER
    if (! reorder_handler)
        reorder_handler = g_signal_connect (notebook, "page-reordered",
         (GCallback) tab_reordered, NULL);
#endif
}

void ui_playlist_notebook_empty (void)
{
    if (switch_handler)
        g_signal_handler_disconnect (notebook, switch_handler);
    switch_handler = 0;
#ifdef HAVE_REORDER
    if (reorder_handler)
        g_signal_handler_disconnect (notebook, reorder_handler);
    reorder_handler = 0;
#endif

    gint n_pages = gtk_notebook_get_n_pages ((GtkNotebook *) notebook);
    while (n_pages)
        ui_playlist_notebook_destroy_tab (-- n_pages);
}

static void do_follow (void)
{
    gint lists = aud_playlist_count ();
    gint playing = aud_playlist_get_playing ();

    if (bolded_playlist != playing)
    {
        if (bolded_playlist >= 0)
            set_tab_label (bolded_playlist, get_tab_label (bolded_playlist));
        if (playing >= 0)
            set_tab_label (playing, get_tab_label (playing));

        bolded_playlist = playing;
    }

    while (! g_queue_is_empty (& follow_queue))
    {
        gint list = GPOINTER_TO_INT (g_queue_pop_head (& follow_queue));
        gint row = GPOINTER_TO_INT (g_queue_pop_head (& follow_queue));

        if (list >= lists)
            continue;

        GtkWidget * widget = playlist_get_treeview (list);
        if (row == aud_playlist_get_position (list))
            audgui_list_set_highlight (widget, row);

        if (config.autoscroll)
            audgui_list_set_focus (widget, row);
    }
}

void ui_playlist_notebook_update(gpointer hook_data, gpointer user_data)
{
    gint type = GPOINTER_TO_INT(hook_data);
    gint lists = aud_playlist_count ();

    if (type == PLAYLIST_UPDATE_STRUCTURE)
    {
        AUDDBG("playlist order update\n");
        gint i, n_pages;

        n_pages = gtk_notebook_get_n_pages(UI_PLAYLIST_NOTEBOOK);

        while (n_pages < lists)
            ui_playlist_notebook_create_tab (n_pages ++);
        while (n_pages > lists)
            ui_playlist_notebook_destroy_tab (-- n_pages);

        for (i = 0; i < n_pages; i++)
        {
            set_tab_label (i, get_tab_label (i));
            ui_playlist_widget_set_playlist (playlist_get_treeview (i), i);
        }

        gtk_notebook_set_current_page(UI_PLAYLIST_NOTEBOOK, aud_playlist_get_active());
        bolded_playlist = aud_playlist_get_playing ();
    }

    gint list, at, count;
    if (aud_playlist_update_range (& list, & at, & count))
        ui_playlist_widget_update (playlist_get_treeview (list), type, at, count);
    else
    {
        for (list = 0; list < lists; list ++)
            ui_playlist_widget_update (playlist_get_treeview (list), type, 0,
             aud_playlist_entry_count (list));
    }

    do_follow ();
}

void playlist_follow (gint list, gint row)
{
    if (config.autoscroll)
    {
        aud_playlist_select_all (list, FALSE);
        if (row >= 0)
            aud_playlist_entry_set_selected (list, row, TRUE);
    }

    g_queue_push_tail (& follow_queue, GINT_TO_POINTER (list));
    g_queue_push_tail (& follow_queue, GINT_TO_POINTER (row));

    if (! aud_playlist_update_pending ())
        do_follow ();
}

void ui_playlist_notebook_position (void * data, void * user)
{
    gint list = GPOINTER_TO_INT (data);
    playlist_follow (list, aud_playlist_get_position (list));
}

static void destroy_cb (void)
{
    notebook = NULL;
    g_queue_clear (& follow_queue);
    index_free (pages);
    switch_handler = 0;
#ifdef HAVE_REORDER
    reorder_handler = 0;
#endif
}

GtkWidget *ui_playlist_notebook_new()
{
    AUDDBG("playlist notebook create\n");

    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(UI_PLAYLIST_NOTEBOOK, TRUE);
    gtk_notebook_set_show_border(UI_PLAYLIST_NOTEBOOK, FALSE);

    g_signal_connect (notebook, "destroy", (GCallback) destroy_cb, NULL);
    return notebook;
}

void playlist_show_headers (GtkToggleAction * a)
{
    if (config.playlist_headers == gtk_toggle_action_get_active (a))
        return;
    config.playlist_headers = gtk_toggle_action_get_active (a);
    ui_playlist_notebook_empty ();
    ui_playlist_notebook_populate ();
}
