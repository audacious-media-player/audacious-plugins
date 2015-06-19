/*
 * ui_skinned_window.c
 * Copyright 2011 John Lindgren
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

#include "window.h"
#include "menus.h"
#include "plugin.h"
#include "skins_cfg.h"

void Window::apply_shape ()
{
    if (gtk_widget_get_realized (gtk ()))
        gdk_window_shape_combine_region (gtk_widget_get_window (gtk ()),
         m_is_shaded ? m_sshape.get () : m_shape.get (), 0, 0);
}

void Window::realize ()
{
    gdk_window_set_back_pixmap (gtk_widget_get_window (gtk ()), nullptr, false);
    apply_shape ();
}

bool Window::button_press (GdkEventButton * event)
{
    /* pass double clicks through; they are handled elsewhere */
    if (event->button != 1 || event->type == GDK_2BUTTON_PRESS)
        return false;

    if (m_is_moving)
        return true;

    dock_move_start (m_id, event->x_root, event->y_root);
    m_is_moving = true;
    return true;
}

bool Window::button_release (GdkEventButton * event)
{
    if (event->button != 1)
        return false;

    m_is_moving = false;
    return true;
}

bool Window::motion (GdkEventMotion * event)
{
    if (! m_is_moving)
        return true;

    dock_move (event->x_root, event->y_root);
    return true;
}

bool Window::close ()
{
    skins_close ();
    return true;
}

Window::~Window ()
{
    dock_remove_window (m_id);

    g_object_unref (m_normal);
    g_object_unref (m_shaded);
}

Window::Window (int id, int * x, int * y, int w, int h, bool shaded) :
    m_id (id),
    m_is_shaded (shaded)
{
    w *= config.scale;
    h *= config.scale;

    GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated ((GtkWindow *) window, false);
    gtk_window_set_resizable ((GtkWindow *) window, false);
    gtk_window_move ((GtkWindow *) window, * x, * y);
    gtk_widget_set_size_request (window, w, h);
    gtk_window_resize ((GtkWindow *) window, w, h);

    if (id != WINDOW_MAIN)
    {
        gtk_window_set_skip_pager_hint ((GtkWindow *) window, true);
        gtk_window_set_skip_taskbar_hint ((GtkWindow *) window, true);
    }

    gtk_widget_set_app_paintable (window, true);
    gtk_widget_add_events (window, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
    gtk_window_add_accel_group ((GtkWindow *) window, menu_get_accel_group ());

    /* We set None as the background pixmap in order to avoid flickering.
     * Setting a blank GtkStyle prevents GTK 2.x from overriding this. */
    GtkStyle * style = gtk_style_new ();
    gtk_widget_set_style (window, style);
    g_object_unref (style);

    set_input (window);
    set_drawable (window);
    set_scale (config.scale);

    m_normal = gtk_fixed_new ();
    g_object_ref_sink (m_normal);
    gtk_widget_show (m_normal);

    m_shaded = gtk_fixed_new ();
    g_object_ref_sink (m_shaded);
    gtk_widget_show (m_shaded);

    if (shaded)
        gtk_container_add ((GtkContainer *) window, m_shaded);
    else
        gtk_container_add ((GtkContainer *) window, m_normal);

    dock_add_window (id, this, x, y, w, h);
}

void Window::resize (int w, int h)
{
    w *= config.scale;
    h *= config.scale;

    gtk_widget_set_size_request (gtk (), w, h);
    gtk_window_resize ((GtkWindow *) gtk (), w, h);
    dock_set_size (m_id, w, h);
}

void Window::set_shapes (GdkRegion * shape, GdkRegion * sshape)
{
    m_shape.capture (shape);
    m_sshape.capture (sshape);
    apply_shape ();
}

void Window::set_shaded (bool shaded)
{
    if (m_is_shaded == shaded)
        return;

    if (shaded)
    {
        gtk_container_remove ((GtkContainer *) gtk (), m_normal);
        gtk_container_add ((GtkContainer *) gtk (), m_shaded);
    }
    else
    {
        gtk_container_remove ((GtkContainer *) gtk (), m_shaded);
        gtk_container_add ((GtkContainer *) gtk (), m_normal);
    }

    m_is_shaded = shaded;
    apply_shape ();
}

void Window::put_widget (bool shaded, Widget * widget, int x, int y)
{
    GtkWidget * fixed = shaded ? m_shaded : m_normal;
    gtk_fixed_put ((GtkFixed *) fixed, widget->gtk (), x * config.scale, y * config.scale);
}

void Window::move_widget (bool shaded, Widget * widget, int x, int y)
{
    GtkWidget * fixed = shaded ? m_shaded : m_normal;
    gtk_fixed_move ((GtkFixed *) fixed, widget->gtk (), x * config.scale, y * config.scale);
}
