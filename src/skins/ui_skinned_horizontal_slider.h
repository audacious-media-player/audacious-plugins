/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
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

#ifndef SKINS_UI_SKINNED_HORIZONTAL_SLIDER_H
#define SKINS_UI_SKINNED_HORIZONTAL_SLIDER_H

#include <gtk/gtk.h>

GtkWidget * hslider_new (int min, int max, SkinPixmapId si, int w, int h,
 int fx, int fy, int kw, int kh, int knx, int kny, int kpx, int kpy);
void hslider_set_frame (GtkWidget * hslider, int fx, int fy);
void hslider_set_knob (GtkWidget * hslider, int knx, int kny, int kpx, int
 kpy);
int hslider_get_pos (GtkWidget * hslider);
void hslider_set_pos (GtkWidget * hslider, int pos);
gboolean hslider_get_pressed (GtkWidget * hslider);
void hslider_set_pressed (GtkWidget * hslider, gboolean pressed);
void hslider_on_motion (GtkWidget * hslider, void (* callback) (void));
void hslider_on_release (GtkWidget * hslider, void (* callback) (void));

#endif
