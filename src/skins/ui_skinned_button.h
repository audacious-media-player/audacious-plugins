/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#ifndef SKINS_UI_SKINNED_BUTTON_H
#define SKINS_UI_SKINNED_BUTTON_H

#include <gtk/gtk.h>

#include "ui_skin.h"

typedef void (* ButtonCB) (GtkWidget * button, GdkEventButton * event);

GtkWidget * button_new (gint w, gint h, gint nx, gint ny, gint px,
 gint py, SkinPixmapId si1, SkinPixmapId si2);
GtkWidget * button_new_toggle (gint w, gint h, gint nx, gint ny,
 gint px, gint py, gint pnx, gint pny, gint ppx, gint ppy, SkinPixmapId si1,
 SkinPixmapId si2);
GtkWidget * button_new_small (gint w, gint h);

void button_on_press (GtkWidget * button, ButtonCB callback);
void button_on_release (GtkWidget * button, ButtonCB callback);
void button_on_rpress (GtkWidget * button, ButtonCB callback);
void button_on_rrelease (GtkWidget * button, ButtonCB callback);

gboolean button_get_active (GtkWidget * button);
void button_set_active (GtkWidget * button, gboolean active);

#endif
