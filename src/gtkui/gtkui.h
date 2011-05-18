/*
 * gtkui.h
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

#ifndef GTKUI_H
#define GTKUI_H

#include <gtk/gtk.h>

/* menus.c */
GtkWidget * make_menu_bar (GtkAccelGroup * accel);
GtkWidget * make_menu_main (GtkAccelGroup * accel);
GtkWidget * make_menu_rclick (GtkAccelGroup * accel);
GtkWidget * make_menu_tab (GtkAccelGroup * accel);

/* ui_gtk.c */
void show_menu (gboolean show);
void show_infoarea (gboolean show);
void show_statusbar (gboolean show);
void popup_menu_rclick (guint button, guint32 time);
void popup_menu_tab (guint button, guint32 time);

#endif
