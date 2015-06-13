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

protected:
    void set_gtk (GtkWidget * widget);

    virtual void draw (cairo_t * cr) {}

private:
    static void destroy_cb (GtkWidget * widget, Widget * me);
    static gboolean draw_cb (GtkWidget * widget, GdkEventExpose * event, Widget * me);

    GtkWidget * m_widget = nullptr;
};

#endif // SKINS_WIDGET_H
