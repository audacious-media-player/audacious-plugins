/*
 * plugin-window.c
 * Copyright 2014 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

/* TODO (someday): implement proper docking for plugin windows */

#include "plugin-window.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui-gtk.h>

#include "main.h"
#include "window.h"

static GList * windows;

static gboolean delete_cb (GtkWidget * window, GdkEvent * event, PluginHandle * plugin)
{
    aud_plugin_enable (plugin, false);
    return true;
}

static gboolean escape_cb (GtkWidget * widget, GdkEventKey * event, PluginHandle * plugin)
{
    if (event->keyval != GDK_KEY_Escape)
        return false;

    aud_plugin_enable (plugin, false);
    return true;
}

static void add_dock_plugin (PluginHandle * plugin, void * unused)
{
    GtkWidget * widget = (GtkWidget *) aud_plugin_get_gtk_widget (plugin);

    if (widget)
    {
        GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title ((GtkWindow *) window, aud_plugin_get_name (plugin));
        gtk_window_set_transient_for ((GtkWindow *) window, (GtkWindow *) mainwin->gtk ());
        gtk_container_set_border_width ((GtkContainer *) window, 2);
        gtk_container_add ((GtkContainer *) window, widget);

        g_object_set_data ((GObject *) window, "skins-plugin-id", plugin);
        g_signal_connect (window, "delete-event", (GCallback) delete_cb, plugin);
        g_signal_connect (widget, "key-press-event", (GCallback) escape_cb, plugin);

        windows = g_list_prepend (windows, window);

        const char * basename = aud_plugin_get_basename (plugin);
        String pos_str = aud_get_str ("skins-layout", basename);
        int pos[4];

        if (pos_str && str_to_int_array (pos_str, pos, aud::n_elems (pos)))
        {
            pos[2] = audgui_to_native_dpi (pos[2]);
            pos[3] = audgui_to_native_dpi (pos[3]);

            gtk_window_set_default_size ((GtkWindow *) window, pos[2], pos[3]);
            gtk_window_move ((GtkWindow *) window, pos[0], pos[1]);
        }
        else
        {
            int dpi = audgui_get_dpi ();
            gtk_window_set_default_size ((GtkWindow *) window, 3 * dpi, 2 * dpi);
        }

        if (aud_ui_is_shown ())
            gtk_widget_show_all (window);
    }
}

static void save_window_size (GtkWidget * window)
{
    auto plugin = (PluginHandle *) g_object_get_data ((GObject *) window, "skins-plugin-id");

    if (! plugin || ! gtk_widget_get_visible (window))
        return;

    int pos[4];
    gtk_window_get_position ((GtkWindow *) window, & pos[0], & pos[1]);
    gtk_window_get_size ((GtkWindow *) window, & pos[2], & pos[3]);

    pos[2] = audgui_to_portable_dpi (pos[2]);
    pos[3] = audgui_to_portable_dpi (pos[3]);

    const char * basename = aud_plugin_get_basename (plugin);
    StringBuf pos_str = int_array_to_str (pos, aud::n_elems (pos));
    aud_set_str ("skins-layout", basename, pos_str);
}

static int find_cb (GtkWidget * window, PluginHandle * plugin)
{
    return (g_object_get_data ((GObject *) window, "skins-plugin-id") != plugin);
}

static void remove_dock_plugin (PluginHandle * plugin, void * unused)
{
    GList * node = g_list_find_custom (windows, plugin, (GCompareFunc) find_cb);

    if (node)
    {
        save_window_size ((GtkWidget *) node->data);
        gtk_widget_destroy ((GtkWidget *) node->data);
        windows = g_list_delete_link (windows, node);
    }
}

void create_plugin_windows ()
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin (plugin, nullptr);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin (plugin, nullptr);
    }

    hook_associate ("dock plugin enabled", (HookFunction) add_dock_plugin, nullptr);
    hook_associate ("dock plugin disabled", (HookFunction) remove_dock_plugin, nullptr);
}

void show_plugin_windows ()
{
    g_list_foreach (windows, (GFunc) gtk_widget_show_all, nullptr);
}

void focus_plugin_window (PluginHandle * plugin)
{
    GList * node = g_list_find_custom (windows, plugin, (GCompareFunc) find_cb);
    if (node)
        gtk_window_present ((GtkWindow *) node->data);

    aud_plugin_send_message (plugin, "grab focus", nullptr, 0);
}

void hide_plugin_windows ()
{
    g_list_foreach (windows, (GFunc) save_window_size, nullptr);
    g_list_foreach (windows, (GFunc) gtk_widget_hide, nullptr);
}

void destroy_plugin_windows ()
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin (plugin, nullptr);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin (plugin, nullptr);
    }

    hook_dissociate ("dock plugin enabled", (HookFunction) add_dock_plugin);
    hook_dissociate ("dock plugin disabled", (HookFunction) remove_dock_plugin);

    g_warn_if_fail (! windows);
}
