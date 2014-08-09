/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2009-2011 John Lindgren
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

#ifndef SKINS_UI_SKINNED_PLAYLIST_H
#define SKINS_UI_SKINNED_PLAYLIST_H

#include <gtk/gtk.h>

GtkWidget * ui_skinned_playlist_new (int width, int height, const char * font);
void ui_skinned_playlist_set_slider (GtkWidget * list, GtkWidget * slider);
void ui_skinned_playlist_resize (GtkWidget * list, int w, int h);
void ui_skinned_playlist_set_font (GtkWidget * list, const char * font);
void ui_skinned_playlist_update (GtkWidget * list);
gboolean ui_skinned_playlist_key (GtkWidget * list, GdkEventKey * event);
void ui_skinned_playlist_row_info (GtkWidget * list, int * rows, int * first);
void ui_skinned_playlist_scroll_to (GtkWidget * list, int row);
void ui_skinned_playlist_set_focused (GtkWidget * list, int row);
void ui_skinned_playlist_hover (GtkWidget * list, int x, int y);
int ui_skinned_playlist_hover_end (GtkWidget * list);

#endif
