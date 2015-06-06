/*
 * drawing.h
 * Copyright 2011-2015 John Lindgren
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

#ifndef SKINS_DRAWING_H
#define SKINS_DRAWING_H

#include <gtk/gtk.h>

/* Window-less drawable widget; allows partly transparent widgets */
GtkWidget * drawing_area_new ();

#define DRAW_FUNC_BEGIN(n,T) static int n (GtkWidget * wid, GdkEventExpose * ev, T * data) { \
 cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (wid)); \
 if (! gtk_widget_get_has_window (wid)) { \
    GtkAllocation alloc; \
    gtk_widget_get_allocation (wid, & alloc); \
    cairo_translate (cr, alloc.x, alloc.y); \
    cairo_rectangle (cr, 0, 0, alloc.width, alloc.height); \
    cairo_clip (cr); \
 }

#define DRAW_FUNC_END cairo_destroy (cr); \
 return FALSE; }

#define DRAW_CONNECT(w,f,d) do { \
    g_signal_connect (w, "expose-event", (GCallback) f, d); \
 } while (0)

/* Adds a drawable widget to an input-only GtkEventBox */
#define DRAW_CONNECT_PROXY(w,f,d) do { \
    GtkWidget * proxy = drawing_area_new (); \
    gtk_container_add ((GtkContainer *) w, proxy); \
    gtk_widget_show (proxy); \
    g_signal_connect (proxy, "expose-event", (GCallback) f, d); \
 } while (0)

#endif // SKINS_DRAWING_H
