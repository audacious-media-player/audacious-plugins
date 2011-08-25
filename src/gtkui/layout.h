/*
 * layout.h
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

#ifndef AUD_GTKUI_LAYOUT_H
#define AUD_GTKUI_LAYOUT_H

#include <gtk/gtk.h>

void layout_save (void);
void layout_cleanup (void);

GtkWidget * layout_new (void);
void layout_add_center (GtkWidget * add);
void layout_add (GtkWidget * add, const gchar * name);
void layout_remove (GtkWidget * rem);

#endif
