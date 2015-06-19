/*
 * widget.h
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

#ifndef SKINS_WIDGET_H
#define SKINS_WIDGET_H

#include <gtk/gtk.h>

class Widget
{
public:
    virtual ~Widget () {}

    GtkWidget * gtk () { return m_widget; }
    GtkWidget * gtk_dr () { return m_drawable; }

    void show () { gtk_widget_show (m_widget); }
    void hide () { gtk_widget_hide (m_widget); }

    void setVisible (bool visible)
        { gtk_widget_set_visible (m_widget, visible); }
    void queue_draw ()
        { gtk_widget_queue_draw (m_drawable); }

protected:
    void set_input (GtkWidget * widget);
    void set_drawable (GtkWidget * widget);
    void add_input (int width, int height, bool track_motion, bool drawable);
    void add_drawable (int width, int height);

    void set_scale (int scale) { m_scale = scale; }
    void resize (int width, int height)
        { gtk_widget_set_size_request (m_widget, width * m_scale, height * m_scale); }

    void draw_now ();

    virtual void realize () {}
    virtual void draw (cairo_t * cr) {}
    virtual bool keypress (GdkEventKey * event) { return false; }
    virtual bool button_press (GdkEventButton * event) { return false; }
    virtual bool button_release (GdkEventButton * event) { return false; }
    virtual bool scroll (GdkEventScroll * event) { return false; }
    virtual bool motion (GdkEventMotion * event) { return false; }
    virtual bool leave () { return false; }
    virtual bool close () { return false; }

private:
    static void destroy_cb (GtkWidget * widget, Widget * me)
        { delete me; }
    static void realize_cb (GtkWidget * widget, Widget * me)
        { me->realize (); }

    static gboolean draw_cb (GtkWidget * widget, GdkEventExpose * event, Widget * me);

    static gboolean keypress_cb (GtkWidget * widget, GdkEventKey * event, Widget * me)
        { return me->keypress (event); }
    static gboolean button_press_cb (GtkWidget * widget, GdkEventButton * event, Widget * me)
        { return me->button_press (event); }
    static gboolean button_release_cb (GtkWidget * widget, GdkEventButton * event, Widget * me)
        { return me->button_release (event); }
    static gboolean scroll_cb (GtkWidget * widget, GdkEventScroll * event, Widget * me)
        { return me->scroll (event); }
    static gboolean motion_cb (GtkWidget * widget, GdkEventMotion * event, Widget * me)
        { return me->motion (event); }
    static gboolean leave_cb (GtkWidget * widget, GdkEventCrossing * event, Widget * me)
        { return me->leave (); }
    static gboolean close_cb (GtkWidget * widget, GdkEvent * event, Widget * me)
        { return me->close (); }

    GtkWidget * m_widget = nullptr;
    GtkWidget * m_drawable = nullptr;
    int m_scale = 1;
};

#endif // SKINS_WIDGET_H
