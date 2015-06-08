/*
 * ui_skinned_window.h
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

#ifndef SKINS_UI_SKINNED_WINDOW_H
#define SKINS_UI_SKINNED_WINDOW_H

#include <gtk/gtk.h>

enum {
    WINDOW_MAIN,
    WINDOW_EQ,
    WINDOW_PLAYLIST,
    N_WINDOWS
};

typedef void (* DrawFunc) (GtkWidget *, cairo_t *);

GtkWidget * window_new (int id, int * x, int * y, int w, int h, bool shaded, DrawFunc draw);
void window_set_size (GtkWidget * window, int w, int h);
void window_set_shapes (GtkWidget * window, GdkRegion * shape, GdkRegion * sshape);
void window_set_shaded (GtkWidget * window, bool shaded);
void window_put_widget (GtkWidget * window, bool shaded, GtkWidget * widget, int x, int y);
void window_move_widget (GtkWidget * window, bool shaded, GtkWidget * widget, int x, int y);
void window_show_all (GtkWidget * window);

void dock_add_window (int id, GtkWidget * window, int * x, int * y, int w, int h);
void dock_remove_window (int id);
void dock_set_size (int id, int w, int h);
void dock_move_start (int id, int x, int y);
void dock_move (int x, int y);
void dock_change_scale (int old_scale, int new_scale);

#endif
