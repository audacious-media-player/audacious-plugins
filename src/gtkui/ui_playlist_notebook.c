/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2010  Audacious development team
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <audacious/debug.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>

#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "ui_playlist_model.h"
#include "playlist_util.h"

static GtkWidget *notebook;
static struct index *pages;
GtkWidget *ui_playlist_notebook_tab_title_editing = NULL;

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

    return FALSE;
}

static void tab_changed(GtkNotebook * notebook, GtkNotebookPage * notebook_page, gint page_num, void *unused)
{
    GtkTreeView *treeview = playlist_get_treeview(page_num);

    if (treeview != NULL)
    {
        aud_playlist_set_active (page_num);

        if (ui_playlist_notebook_tab_title_editing != NULL)
            tab_title_reset(ui_playlist_notebook_tab_title_editing);
    }
}

static void tab_reordered(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
    GtkTreeView *treeview = playlist_get_treeview_from_page(child);

    if (treeview == NULL)
        return;

    aud_playlist_reorder (treeview_get_playlist (treeview), page_num, 1);
}

static GtkLabel *get_tab_label(gint playlist)
{
    GtkWidget *page = gtk_notebook_get_nth_page(UI_PLAYLIST_NOTEBOOK, playlist);
    GtkWidget *ebox = gtk_notebook_get_tab_label(UI_PLAYLIST_NOTEBOOK, page);
    return GTK_LABEL(g_object_get_data(G_OBJECT(ebox), "label"));
}

void ui_playlist_notebook_add_tab_label_markup(gint playlist, gboolean new_title)
{
    static gint last_playlist = -1;
    static GtkLabel *last_label = NULL;

    if (last_playlist == playlist && !new_title)
        return;
    else
    {
        if (last_playlist > -1 && last_label != NULL && !new_title)
            gtk_label_set_text(last_label, aud_playlist_get_title(last_playlist));

        GtkLabel *label = get_tab_label(playlist);

        if (!GTK_IS_LABEL(label))
            return;

        gchar *markup = g_markup_printf_escaped("<b>%s</b>", aud_playlist_get_title(playlist));
        gtk_label_set_markup(label, markup);
        g_free(markup);

        last_playlist = playlist;
        last_label = label;
    }
}

void ui_playlist_notebook_edit_tab_title(GtkWidget *ebox)
{
    if (!gtk_notebook_get_show_tabs(UI_PLAYLIST_NOTEBOOK))
        return;

    if (ebox == NULL)
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
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwin), GTK_SHADOW_IN);
    gtk_widget_show_all(scrollwin);

    ebox = gtk_event_box_new();
    GTK_WIDGET_SET_FLAGS(ebox, GTK_NO_WINDOW);

    hbox = gtk_hbox_new(FALSE, 2);

    label = gtk_label_new(aud_playlist_get_title(playlist));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(ebox), hbox);
    gtk_widget_show_all(ebox);
    gtk_widget_hide(entry);

    g_object_set_data(G_OBJECT(ebox), "label", label);
    g_object_set_data(G_OBJECT(ebox), "entry", entry);

    gtk_notebook_append_page(UI_PLAYLIST_NOTEBOOK, scrollwin, ebox);
    gtk_notebook_set_show_tabs(UI_PLAYLIST_NOTEBOOK, index_count(pages) > 1 ? TRUE : FALSE);
    gtk_notebook_set_tab_reorderable(UI_PLAYLIST_NOTEBOOK, scrollwin, TRUE);

    if (position >= 0)
    {
        aud_playlist_select_all (playlist, FALSE);
        aud_playlist_entry_set_selected (playlist, position, TRUE);
        treeview_set_focus_now ((GtkTreeView *) treeview, position);
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
    gtk_notebook_set_show_tabs(UI_PLAYLIST_NOTEBOOK, index_count(pages) > 1 ? TRUE : FALSE);
}

void ui_playlist_notebook_populate(void)
{
    gint playlists = aud_playlist_count();
    gint count;

    pages = index_new();

    for (count = 0; count < playlists; count++)
        ui_playlist_notebook_create_tab(count);

    gtk_notebook_set_current_page (UI_PLAYLIST_NOTEBOOK, aud_playlist_get_active ());

    g_signal_connect (UI_PLAYLIST_NOTEBOOK, "switch-page", (GCallback)
     tab_changed, NULL);
    g_signal_connect (UI_PLAYLIST_NOTEBOOK, "page-reordered", (GCallback)
     tab_reordered, NULL);
}

void ui_playlist_notebook_update(gpointer hook_data, gpointer user_data)
{
    gint type = GPOINTER_TO_INT(hook_data);
    gint lists = aud_playlist_count ();

    if (type == PLAYLIST_UPDATE_STRUCTURE)
    {
        AUDDBG("playlist order update\n");

        GtkLabel *label;
        gint i, n_pages;

        n_pages = gtk_notebook_get_n_pages(UI_PLAYLIST_NOTEBOOK);

        while (n_pages < lists)
            ui_playlist_notebook_create_tab (n_pages ++);
        while (n_pages > lists)
            ui_playlist_notebook_destroy_tab (-- n_pages);

        for (i = 0; i < n_pages; i++)
        {
            if (i == aud_playlist_get_playing())
                ui_playlist_notebook_add_tab_label_markup(i, TRUE);
            else
            {
                label = get_tab_label(i);

                if (GTK_IS_LABEL(label))
                    gtk_label_set_text(label, aud_playlist_get_title(i));
            }

            ui_playlist_model_set_playlist ((UiPlaylistModel *)
             gtk_tree_view_get_model (playlist_get_treeview (i)), i);
        }

        gtk_notebook_set_current_page(UI_PLAYLIST_NOTEBOOK, aud_playlist_get_active());
        gtk_widget_grab_focus(GTK_WIDGET(playlist_get_active_treeview()));
    }

    gint list, at, count;
    if (aud_playlist_update_range (& list, & at, & count))
        treeview_update (playlist_get_treeview (list), type, at, count);
    else
    {
        for (list = 0; list < lists; list ++)
            treeview_update (playlist_get_treeview (list), type, 0,
             aud_playlist_entry_count (list));
    }
}

void ui_playlist_notebook_position (void * data, void * user)
{
    treeview_update_position (playlist_get_treeview (GPOINTER_TO_INT (data)));
}

static void destroy_cb (void)
{
    index_free (pages);
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
