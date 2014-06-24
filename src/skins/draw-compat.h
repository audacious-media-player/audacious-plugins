/*
 * draw-compat.h
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

#ifndef SKINS_DRAW_COMPAT_H
#define SKINS_DRAW_COMPAT_H

#include <gtk/gtk.h>

static void widget_realized (GtkWidget * w)
{
    GdkWindow * window = gtk_widget_get_window (w);
    gdk_window_set_back_pixmap (window, nullptr, FALSE);
}

#define DRAW_SIGNAL "expose-event"
#define DRAW_FUNC_BEGIN(n) static int n (GtkWidget * wid, GdkEventExpose * ev) { \
 cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (wid));
#define DRAW_FUNC_END cairo_destroy (cr); \
 return TRUE; }

/* We set None as the background pixmap in order to avoid flickering.  Setting
 * a blank GtkStyle prevents GTK 2.x from overriding this. */

#define DRAW_CONNECT(w,f) do { \
    GtkStyle * style = gtk_style_new (); \
    gtk_widget_set_style (w, style); \
    g_object_unref (style); \
    g_signal_connect (w, "realize", (GCallback) widget_realized, nullptr); \
    g_signal_connect (w, DRAW_SIGNAL, (GCallback) f, nullptr); \
 } while (0);

#endif
