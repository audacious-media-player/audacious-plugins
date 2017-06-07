/*
 * layout.c
 * Copyright 2011 John Lindgren
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define AUD_GLIB_INTEGRATION
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugins.h>
#include <libaudgui/libaudgui-gtk.h>

#include "layout.h"

enum {
    DOCK_LEFT,
    DOCK_RIGHT,
    DOCK_TOP,
    DOCK_BOTTOM,
    DOCKS
};

#define IS_VERTICAL(d) ((d) & 2)
#define IS_AFTER(d) ((d) & 1)

#define NULL_ON_DESTROY(w) g_signal_connect ((w), "destroy", (GCallback) \
 gtk_widget_destroyed, & (w))

typedef struct {
    String name;
    PluginHandle * plugin;
    GtkWidget * widget, * vbox, * paned, * window;
    int dock, x, y, w, h;
} Item;

static GList * items = nullptr;

static GtkWidget * layout = nullptr;
static GtkWidget * center = nullptr;
static GtkWidget * docks[DOCKS] = {nullptr, nullptr, nullptr, nullptr};
static GtkWidget * menu = nullptr;

static Item * item_new (const char * name)
{
    int dpi = audgui_get_dpi ();

    Item * item = new Item ();
    item->name = String (name);
    item->plugin = nullptr;
    item->widget = item->vbox = item->paned = item->window = nullptr;
    item->dock = item->x = item->y = -1;
    item->w = 3 * dpi;
    item->h = 2 * dpi;

    if (! strcmp (name, _("Search Tool")))
    {
        item->dock = DOCK_LEFT;
        item->w = 2 * dpi;
    }

    items = g_list_append (items, item);
    return item;
}

static int item_by_plugin (Item * item, PluginHandle * plugin)
{
    return (item->plugin != plugin);
}

static int item_by_widget (Item * item, GtkWidget * widget)
{
    return (item->widget != widget);
}

static int item_by_name (Item * item, const char * name)
{
    return strcmp (item->name, name);
}

GtkWidget * layout_new ()
{
    g_return_val_if_fail (! layout, nullptr);
    layout = gtk_alignment_new (0, 0, 1, 1);
    gtk_alignment_set_padding ((GtkAlignment *) layout, 3, 3, 3, 3);
    NULL_ON_DESTROY (layout);
    return layout;
}

void layout_add_center (GtkWidget * widget)
{
    g_return_if_fail (layout && ! center && widget);
    center = widget;
    gtk_container_add ((GtkContainer *) layout, center);
    NULL_ON_DESTROY (center);
}

static void layout_move (GtkWidget * widget, int dock);

static void layout_dock_left (GtkWidget * widget)
{
    layout_move (widget, DOCK_LEFT);
}

static void layout_dock_right (GtkWidget * widget)
{
    layout_move (widget, DOCK_RIGHT);
}

static void layout_dock_top (GtkWidget * widget)
{
    layout_move (widget, DOCK_TOP);
}

static void layout_dock_bottom (GtkWidget * widget)
{
    layout_move (widget, DOCK_BOTTOM);
}

static void layout_undock (GtkWidget * widget)
{
    layout_move (widget, -1);
}

static void layout_disable (GtkWidget * widget)
{
    g_return_if_fail (layout && center && widget);

    GList * node = g_list_find_custom (items, widget, (GCompareFunc) item_by_widget);
    g_return_if_fail (node);

    Item * item = (Item *) node->data;
    g_return_if_fail (item->plugin);

    aud_plugin_enable (item->plugin, false);
}

static gboolean menu_cb (GtkWidget * widget, GdkEventButton * event)
{
    g_return_val_if_fail (widget && event, false);

    if (event->type != GDK_BUTTON_PRESS || event->button != 3)
        return false;

    if (menu)
        gtk_widget_destroy (menu);

    menu = gtk_menu_new ();
    g_signal_connect (menu, "destroy", (GCallback) gtk_widget_destroyed, & menu);

    const char * names[6] = {N_("Dock at Left"), N_("Dock at Right"),
     N_("Dock at Top"), N_("Dock at Bottom"), N_("Undock"), N_("Disable")};
    void (* const funcs[6]) (GtkWidget * widget) = {layout_dock_left,
     layout_dock_right, layout_dock_top, layout_dock_bottom, layout_undock,
     layout_disable};

    for (int i = 0; i < 6; i ++)
    {
        GtkWidget * item = gtk_menu_item_new_with_label (_(names[i]));
        gtk_menu_shell_append ((GtkMenuShell *) menu, item);
        g_signal_connect_swapped (item, "activate", (GCallback) funcs[i], widget);
    }

    gtk_widget_show_all (menu);
    gtk_menu_popup ((GtkMenu *) menu, nullptr, nullptr, nullptr, nullptr, event->button, event->time);

    return true;
}

static GtkWidget * vbox_new (GtkWidget * widget, const char * name)
{
    g_return_val_if_fail (widget && name, nullptr);

    GtkWidget * vbox = gtk_vbox_new (false, 2);

    GtkWidget * ebox = gtk_event_box_new ();
    gtk_box_pack_start ((GtkBox *) vbox, ebox, false, false, 0);
    g_signal_connect_swapped (ebox, "button-press-event", (GCallback) menu_cb,
     widget);

    GtkWidget * label = gtk_label_new (nullptr);
    CharPtr markup (g_markup_printf_escaped ("<small><b>%s</b></small>", name));
    gtk_label_set_markup ((GtkLabel *) label, markup);
    gtk_misc_set_alignment ((GtkMisc *) label, 0, 0);
    gtk_container_add ((GtkContainer *) ebox, label);

    gtk_box_pack_start ((GtkBox *) vbox, widget, true, true, 0);

    gtk_widget_show_all (vbox);

    return vbox;
}

typedef struct {
    GtkWidget * widget;
    bool vertical;
    int w, h;
} RestoreSizeData;

static void restore_size_cb (GtkPaned * paned, GdkRectangle *, RestoreSizeData * d)
{
    GtkAllocation rect;
    gtk_widget_get_allocation (d->widget, & rect);

    int pos = gtk_paned_get_position (paned);
    pos -= d->vertical ? d->h - rect.height : d->w - rect.width;
    gtk_paned_set_position (paned, pos);

    g_signal_handlers_disconnect_by_data (paned, d);
}

static GtkWidget * paned_new (bool vertical, bool after, int w, int h)
{
    GtkWidget * paned = vertical ? gtk_vpaned_new () : gtk_hpaned_new ();

    GtkWidget * mine = gtk_alignment_new (0, 0, 1, 1);
    GtkWidget * next = gtk_alignment_new (0, 0, 1, 1);
    gtk_paned_pack1 ((GtkPaned *) paned, after ? next : mine, after, false);
    gtk_paned_pack2 ((GtkPaned *) paned, after ? mine : next, ! after, false);

    g_object_set_data ((GObject *) paned, "mine", mine);
    g_object_set_data ((GObject *) paned, "next", next);

    gtk_widget_show_all (paned);

    if (vertical ? h : w)
    {
        if (after)
        {
            /* hack to set the size of the second pane */
            auto d = g_new (RestoreSizeData, 1);
            d->widget = mine;
            d->vertical = vertical;
            d->w = w;
            d->h = h;

            g_signal_connect_data (paned, "size-allocate", (GCallback)
             restore_size_cb, d, (GClosureNotify) g_free, (GConnectFlags) 0);
        }
        else
            gtk_paned_set_position ((GtkPaned *) paned, vertical ? h : w);
    }

    return paned;
}

static gboolean delete_cb (GtkWidget * widget)
{
    layout_disable (widget);
    return true;
}

static gboolean escape_cb (GtkWidget * widget, GdkEventKey * event)
{
    if (event->keyval == GDK_KEY_Escape)
    {
        layout_disable (widget);
        return true;
    }

    return false;
}

static GtkWidget * dock_get_parent (int dock)
{
    g_return_val_if_fail (dock >= 0 && dock < DOCKS, nullptr);

    for (int scan = dock; scan --; )
    {
        if (docks[scan])
            return (GtkWidget *) g_object_get_data ((GObject *) docks[scan], "next");
    }

    return layout;
}

static Item * item_get_prev (Item * item)
{
    GList * node = g_list_find (items, item);
    g_return_val_if_fail (node, nullptr);

    while ((node = node->prev))
    {
        Item * test = (Item *) node->data;
        if (test->widget && test->dock == item->dock)
            return test;
    }

    return nullptr;
}

static Item * item_get_next (Item * item)
{
    GList * node = g_list_find (items, item);
    g_return_val_if_fail (node, nullptr);

    while ((node = node->next))
    {
        Item * test = (Item *) node->data;
        if (test->widget && test->dock == item->dock)
            return test;
    }

    return nullptr;
}

static GtkWidget * item_get_parent (Item * item)
{
    Item * prev = item_get_prev (item);

    if (prev)
        return (GtkWidget *) g_object_get_data ((GObject *) prev->paned, "next");
    else
        return (GtkWidget *) g_object_get_data ((GObject *) docks[item->dock], "mine");
}

static void item_add (Item * item)
{
    g_return_if_fail (item->name && item->widget && item->vbox && ! item->paned
     && ! item->window && item->dock < DOCKS);

    if (item->dock < 0)
    {
        item->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        NULL_ON_DESTROY (item->window);

        gtk_window_set_title ((GtkWindow *) item->window, item->name);
        gtk_container_set_border_width ((GtkContainer *) item->window, 2);

        g_signal_connect_swapped (item->window, "delete-event", (GCallback)
         delete_cb, item->widget);
        g_signal_connect_swapped (item->window, "key-press-event", (GCallback)
         escape_cb, item->widget);

        if (item->x >= 0 && item->y >= 0)
            gtk_window_move ((GtkWindow *) item->window, item->x, item->y);
        if (item->w > 0 && item->h > 0)
            gtk_window_set_default_size ((GtkWindow *) item->window, item->w, item->h);

        gtk_container_add ((GtkContainer *) item->window, item->vbox);
        gtk_widget_show_all (item->window);
    }
    else
    {
        /* Screwy logic to figure out where we need to add a GtkPaned and which
         * widget goes in which pane of it. */
        bool swap = false;
        Item * where = item;
        GtkWidget * parent, * paned;

        if (docks[item->dock])
        {
            if (! item_get_next (item))
            {
                swap = true;
                where = item_get_prev (item);
                g_return_if_fail (where && ! where->paned);
            }

            parent = item_get_parent (where);
            g_return_if_fail (parent);

            paned = paned_new (! IS_VERTICAL (where->dock), false, where->w, where->h);
            where->paned = paned;
            NULL_ON_DESTROY (where->paned);
        }
        else
        {
            parent = dock_get_parent (item->dock);
            g_return_if_fail (parent);

            paned = paned_new (IS_VERTICAL (item->dock), IS_AFTER (item->dock), item->w, item->h);
            docks[item->dock] = paned;
            NULL_ON_DESTROY (docks[item->dock]);
        }

        GtkWidget * mine = (GtkWidget *) g_object_get_data ((GObject *) paned, "mine");
        GtkWidget * next = (GtkWidget *) g_object_get_data ((GObject *) paned, "next");
        GtkWidget * child = gtk_bin_get_child ((GtkBin *) parent);
        g_return_if_fail (mine && next && child);

        g_object_ref (child);
        gtk_container_remove ((GtkContainer *) parent, child);
        gtk_container_add ((GtkContainer *) parent, paned);
        gtk_container_add ((GtkContainer *) (swap ? next : mine), item->vbox);
        gtk_container_add ((GtkContainer *) (swap ? mine : next), child);
        g_object_unref (child);
    }
}

static void item_remove (Item * item)
{
    g_return_if_fail (item->widget && item->vbox);

    if (item->dock < 0)
    {
        g_return_if_fail (item->window);
        gtk_container_remove ((GtkContainer *) item->window, item->vbox);
        gtk_widget_destroy (item->window);
    }
    else
    {
        /* Screwy logic to figure out which GtkPaned we need to remove and which
         * pane of it has the widget we need to keep. */
        bool swap = false;
        Item * where = item;
        GtkWidget * parent, * paned;

        Item * prev = item_get_prev (item);
        if (item->paned || prev)
        {
            if (! item->paned)
            {
                swap = true;
                where = item_get_prev (item);
                g_return_if_fail (where && where->paned);
            }

            parent = item_get_parent (where);
            g_return_if_fail (parent);

            paned = where->paned;
        }
        else
        {
            parent = dock_get_parent (item->dock);
            g_return_if_fail (parent);

            paned = docks[item->dock];
        }

        GtkWidget * mine = (GtkWidget *) g_object_get_data ((GObject *) paned, "mine");
        GtkWidget * next = (GtkWidget *) g_object_get_data ((GObject *) paned, "next");
        GtkWidget * child = gtk_bin_get_child ((GtkBin *) (swap ? mine : next));
        g_return_if_fail (mine && next && child);

        g_object_ref (child);
        gtk_container_remove ((GtkContainer *) (swap ? next : mine), item->vbox);
        gtk_container_remove ((GtkContainer *) (swap ? mine : next), child);
        gtk_container_remove ((GtkContainer *) parent, paned);
        gtk_container_add ((GtkContainer *) parent, child);
        g_object_unref (child);
    }
}

static void size_changed_cb (GtkWidget * widget, GdkRectangle * rect, Item * item)
{
    item->w = rect->width;
    item->h = rect->height;

    if (item->dock < 0)
    {
        g_return_if_fail (item->window);
        gtk_window_get_position ((GtkWindow *) item->window, & item->x, & item->y);
    }
}

void layout_add (PluginHandle * plugin, GtkWidget * widget)
{
    g_return_if_fail (layout && center && plugin && widget);

    const char * name = aud_plugin_get_name (plugin);
    g_return_if_fail (name);

    GList * node = g_list_find_custom (items, name, (GCompareFunc) item_by_name);
    Item * item = node ? (Item *) node->data : nullptr;

    if (item)
    {
        g_return_if_fail (! item->widget && ! item->vbox && ! item->window);
        if (item->dock >= DOCKS)
            item->dock = -1;
    }
    else
        item = item_new (name);

    item->plugin = plugin;
    item->widget = widget;
    NULL_ON_DESTROY (item->widget);
    item->vbox = vbox_new (widget, name);
    NULL_ON_DESTROY (item->vbox);

    g_signal_connect (item->vbox, "size-allocate", (GCallback) size_changed_cb, item);

    item_add (item);
}

static void layout_move (GtkWidget * widget, int dock)
{
    g_return_if_fail (layout && center && widget && dock < DOCKS);

    GList * node = g_list_find_custom (items, widget, (GCompareFunc) item_by_widget);
    g_return_if_fail (node);
    Item * item = (Item *) node->data;

    g_return_if_fail (item->vbox);
    g_object_ref (item->vbox);

    item_remove (item);
    items = g_list_remove_link (items, node);
    item->dock = dock;
    items = g_list_concat (items, node);
    item_add (item);

    g_object_unref (item->vbox);
}

void layout_remove (PluginHandle * plugin)
{
    g_return_if_fail (layout && center && plugin);

    GList * node = g_list_find_custom (items, plugin, (GCompareFunc) item_by_plugin);
    if (! node)
        return;

    /* menu may hold pointers to this widget */
    if (menu)
        gtk_widget_destroy (menu);

    item_remove ((Item *) node->data);
}

void layout_focus (PluginHandle * plugin)
{
    g_return_if_fail (layout && center && plugin);

    GList * node = g_list_find_custom (items, plugin, (GCompareFunc) item_by_plugin);
    if (! node)
        return;

    Item * item = (Item *) node->data;
    g_return_if_fail (item);

    if (item->window)
        gtk_window_present ((GtkWindow *) item->window);

    aud_plugin_send_message (plugin, "grab focus", nullptr, 0);
}

void layout_save ()
{
    int i = 0;

    for (GList * node = items; node; node = node->next)
    {
        Item * item = (Item *) node->data;
        g_return_if_fail (item && item->name);

        char key[32], value[64];

        snprintf (key, sizeof key, "item%d_name", i);
        aud_set_str ("gtkui-layout", key, item->name);

        int temp_w = audgui_to_portable_dpi (item->w);
        int temp_h = audgui_to_portable_dpi (item->h);

        snprintf (key, sizeof key, "item%d_pos", i);
        snprintf (value, sizeof value, "%d,%d,%d,%d,%d", item->dock, item->x,
         item->y, temp_w, temp_h);
        aud_set_str ("gtkui-layout", key, value);

        i ++;
    }

    aud_set_int ("gtkui-layout", "item_count", i);
}

void layout_load ()
{
    g_return_if_fail (! items);

    int count = aud_get_int ("gtkui-layout", "item_count");

    for (int i = 0; i < count; i ++)
    {
        char key[32];

        snprintf (key, sizeof key, "item%d_name", i);
        String name = aud_get_str ("gtkui-layout", key);
        Item * item = item_new (name);

        int temp_w = 0, temp_h = 0;

        snprintf (key, sizeof key, "item%d_pos", i);
        String pos = aud_get_str ("gtkui-layout", key);
        sscanf (pos, "%d,%d,%d,%d,%d", & item->dock, & item->x, & item->y, & temp_w, & temp_h);

        if (temp_w)
            item->w = audgui_to_native_dpi (temp_w);
        if (temp_h)
            item->h = audgui_to_native_dpi (temp_h);
    }
}

void layout_cleanup ()
{
    for (GList * node = items; node; node = node->next)
    {
        Item * item = (Item *) node->data;
        g_return_if_fail (! item->widget && ! item->vbox && ! item->window);
        delete item;
    }

    g_list_free (items);
    items = nullptr;
}
