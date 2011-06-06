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

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <audacious/gtk-compat.h>

#if GTK_CHECK_VERSION (3, 0, 0)

#define DRAW_SIGNAL "draw"
#define DRAW_FUNC_BEGIN(n) static gboolean n (GtkWidget * wid, cairo_t * cr) { \
 g_return_val_if_fail (wid && cr, FALSE);
#define DRAW_FUNC_END return TRUE; }

#else

#define DRAW_SIGNAL "expose-event"
#define DRAW_FUNC_BEGIN(n) static gboolean n (GtkWidget * wid, GdkEventExpose * ev) { \
 g_return_val_if_fail (wid, FALSE); \
 cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (wid)); \
 g_return_val_if_fail (cr, FALSE);
#define DRAW_FUNC_END cairo_destroy (cr); \
 return TRUE; }

#endif

#endif
