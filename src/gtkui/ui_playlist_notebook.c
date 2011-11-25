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
#include <audacious/gtk-compat.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudgui/list.h>
#include <libaudgui/libaudgui.h>

#include "gtkui.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "playlist_util.h"

#define CURRENT_POS (-2)

static GtkWidget * notebook = NULL;
static GQueue follow_queue = G_QUEUE_INIT;
static gint highlighted = -1;

static struct index *pages;
GtkWidget *ui_playlist_notebook_tab_title_editing = NULL;

static gint switch_handler = 0;
static gint reorder_handler = 0;

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
        popup_menu_tab (event->button, event->time);
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

    g_free (title);
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

    gchar * title = aud_playlist_get_title (aud_playlist_get_active ());
    gtk_entry_set_text ((GtkEntry *) entry, title);
    g_free (title);
    gtk_widget_grab_focus(entry);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_show(entry);

    ui_playlist_notebook_tab_title_editing = ebox;
}

static void change_view (void)
{
    gtk_notebook_set_show_tabs (UI_PLAYLIST_NOTEBOOK, (index_count (pages) > 1));
}

void ui_playlist_notebook_create_tab(gint playlist)
{
    GtkWidget *scrollwin, *treeview;
    GtkWidget *label, *entry, *ebox, *hbox;
    gint position = aud_playlist_get_position (playlist);

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    index_insert(pages, playlist, scrollwin);

    treeview = ui_playlist_widget_new(playlist);
    g_object_set_data(G_OBJECT(scrollwin), "treeview", treeview);

    gtk_container_add(GTK_CONTAINER(scrollwin), treeview);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
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

    gtk_notebook_insert_page (UI_PLAYLIST_NOTEBOOK, scrollwin, ebox, playlist);
    gtk_notebook_set_tab_reorderable(UI_PLAYLIST_NOTEBOOK, scrollwin, TRUE);
    change_view ();

    g_object_set_data ((GObject *) treeview, "playlist-id",
     GINT_TO_POINTER (aud_playlist_get_unique_id (playlist)));

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
    gtk_widget_grab_focus (gtk_bin_get_child ((GtkBin *)
     gtk_notebook_get_nth_page (UI_PLAYLIST_NOTEBOOK, aud_playlist_get_active ())));
    highlighted = aud_playlist_get_unique_id (aud_playlist_get_playing ());

    if (! switch_handler)
        switch_handler = g_signal_connect (notebook, "switch-page", (GCallback)
         tab_changed, NULL);
    if (! reorder_handler)
        reorder_handler = g_signal_connect (notebook, "page-reordered",
         (GCallback) tab_reordered, NULL);
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
        ui_playlist_notebook_destroy_tab (-- n_pages);
}

static void do_follow (void)
{
    while (! g_queue_is_empty (& follow_queue))
    {
        gint list = aud_playlist_by_unique_id (GPOINTER_TO_INT (g_queue_pop_head
         (& follow_queue)));
        gint row = GPOINTER_TO_INT (g_queue_pop_head (& follow_queue));

        if (list < 0)
            continue;

        GtkWidget * widget = playlist_get_treeview (list);

        if (row == CURRENT_POS)
        {
            row = aud_playlist_get_position (list);
            audgui_list_set_highlight (widget, row);

            if (! aud_get_bool ("gtkui", "autoscroll"))
                continue;
        }

        audgui_list_set_focus (widget, row);
    }
}

static void add_remove_pages (void)
{
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
            ui_playlist_notebook_destroy_tab (i);
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
                g_signal_handlers_block_by_func (notebook, tab_reordered, NULL);
                gtk_notebook_reorder_child ((GtkNotebook *) notebook, page, i);
                g_signal_handlers_unblock_by_func (notebook, tab_reordered, NULL);
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

        gint at, count;
        gint level = aud_playlist_updated_range (list, & at, & count);

        if (level)
            ui_playlist_widget_update (playlist_get_treeview (list), level, at, count);
    }

    gtk_notebook_set_current_page ((GtkNotebook *) notebook, aud_playlist_get_active ());

    do_follow ();
}

void playlist_set_focus (gint list, gint row)
{
    g_queue_push_tail (& follow_queue, GINT_TO_POINTER
     (aud_playlist_get_unique_id (list)));
    g_queue_push_tail (& follow_queue, GINT_TO_POINTER (row));

    if (! aud_playlist_update_pending ())
        do_follow ();
}

void ui_playlist_notebook_position (void * data, void * user)
{
    gint list = GPOINTER_TO_INT (data);

    if (aud_get_bool ("gtkui", "autoscroll"))
    {
        aud_playlist_select_all (list, FALSE);

        if (aud_playlist_get_position (list) >= 0)
            aud_playlist_entry_set_selected (list, aud_playlist_get_position (list), TRUE);
    }

    playlist_set_focus (list, CURRENT_POS);
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
    notebook = NULL;
    g_queue_clear (& follow_queue);
    index_free (pages);
    switch_handler = 0;
    reorder_handler = 0;
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

void playlist_show_headers (gboolean show)
{
    gboolean old = aud_get_bool ("gtkui", "playlist_headers");
    if (old == show)
        return;

    aud_set_bool ("gtkui", "playlist_headers", show);
    ui_playlist_notebook_empty ();
    ui_playlist_notebook_populate ();
}
