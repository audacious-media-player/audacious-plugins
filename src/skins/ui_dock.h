/*
 * ui_dock.h
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

#ifndef SKINS_UI_DOCK_H
#define SKINS_UI_DOCK_H

#include <gtk/gtk.h>

void dock_add_window (GtkWidget * window, int * x, int * y, int w, int h,
 gboolean main);
void dock_remove_window (GtkWidget * window);

void dock_set_size (GtkWidget * window, int w, int h);

void dock_move_start (GtkWidget * window, int x, int y);
void dock_move (int x, int y);

#endif
