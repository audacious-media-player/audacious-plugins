/*
 * widget.cc
 * Copyright 2015 John Lindgren
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

#include "widget.h"

typedef GtkWidget DrawingArea;
typedef GtkWidgetClass DrawingAreaClass;

G_DEFINE_TYPE (DrawingArea, drawing_area, GTK_TYPE_WIDGET)

static void drawing_area_class_init (DrawingAreaClass *) {}

static void drawing_area_init (DrawingArea * widget)
{
    gtk_widget_set_has_window ((GtkWidget *) widget, false);
}

static GtkWidget * drawing_area_new ()
{
    return (GtkWidget *) g_object_new (drawing_area_get_type (), nullptr);
}

void Widget::set_input (GtkWidget * widget)
{
    m_widget = widget;
    g_signal_connect (widget, "destroy", (GCallback) Widget::destroy_cb, this);
    g_signal_connect (widget, "key-press-event", (GCallback) Widget::keypress_cb, this);
    g_signal_connect (widget, "button-press-event", (GCallback) Widget::button_press_cb, this);
    g_signal_connect (widget, "button-release-event", (GCallback) Widget::button_release_cb, this);
    g_signal_connect (widget, "scroll-event", (GCallback) Widget::scroll_cb, this);
    g_signal_connect (widget, "motion-notify-event", (GCallback) Widget::motion_cb, this);
    g_signal_connect (widget, "leave-notify-event", (GCallback) Widget::leave_cb, this);
    g_signal_connect (widget, "delete-event", (GCallback) Widget::close_cb, this);
}

void Widget::set_drawable (GtkWidget * widget)
{
    m_drawable = widget;
    g_signal_connect (widget, "realize", (GCallback) Widget::realize_cb, this);
    g_signal_connect (widget, "expose-event", (GCallback) Widget::draw_cb, this);

    if (! m_widget)
    {
        m_widget = widget;
        g_signal_connect (widget, "destroy", (GCallback) Widget::destroy_cb, this);
    }
}

void Widget::add_input (int width, int height, bool track_motion, bool drawable)
{
    int events = GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK;

    if (track_motion)
        events |= GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK;

    GtkWidget * widget = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) widget, false);
    gtk_widget_set_size_request (widget, width * m_scale, height * m_scale);
    gtk_widget_add_events (widget, events);
    gtk_widget_show (widget);
    set_input (widget);

    if (drawable)
    {
        GtkWidget * area = drawing_area_new ();
        gtk_container_add ((GtkContainer *) widget, area);
        gtk_widget_show (area);
        set_drawable (area);
    }
}

void Widget::add_drawable (int width, int height)
{
    GtkWidget * widget = drawing_area_new ();
    gtk_widget_set_size_request (widget, width * m_scale, height * m_scale);
    gtk_widget_show (widget);
    set_drawable (widget);
}

void Widget::draw_now ()
{
    if (m_drawable && gtk_widget_is_drawable (m_drawable))
        draw_cb (m_drawable, nullptr, this);
}

gboolean Widget::draw_cb (GtkWidget * widget, GdkEventExpose * event, Widget * me)
{
    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));

    if (! gtk_widget_get_has_window (widget))
    {
        GtkAllocation alloc;
        gtk_widget_get_allocation (widget, & alloc);
        cairo_translate (cr, alloc.x, alloc.y);
        cairo_rectangle (cr, 0, 0, alloc.width, alloc.height);
        cairo_clip (cr);
    }

    if (me->m_scale != 1)
        cairo_scale (cr, me->m_scale, me->m_scale);

    me->draw (cr);

    cairo_destroy (cr);
    return false;
}
