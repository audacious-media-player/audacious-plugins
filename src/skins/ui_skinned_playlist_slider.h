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

#ifndef AUDACIOUS_UI_SKINNED_PLAYLIST_SLIDER_H
#define AUDACIOUS_UI_SKINNED_PLAYLIST_SLIDER_H

#include <gtk/gtk.h>

#define UI_SKINNED_PLAYLIST_SLIDER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_skinned_playlist_slider_get_type (), UiSkinnedPlaylistSlider)
#define UI_SKINNED_PLAYLIST_SLIDER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_skinned_playlist_slider_get_type (), UiSkinnedPlaylistSliderClass)
#define UI_SKINNED_IS_PLAYLIST_SLIDER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_skinned_playlist_slider_get_type ())

typedef struct _UiSkinnedPlaylistSlider        UiSkinnedPlaylistSlider;
typedef struct _UiSkinnedPlaylistSliderClass   UiSkinnedPlaylistSliderClass;

struct _UiSkinnedPlaylistSlider {
    GtkWidget   widget;
    gboolean    pressed;
    gint        x, y;
};

struct _UiSkinnedPlaylistSliderClass {
    GtkWidgetClass    parent_class;
};

GtkWidget * ui_skinned_playlist_slider_new (GtkWidget * fixed, gint x, gint y,
 gint h, GtkWidget * list);
GType ui_skinned_playlist_slider_get_type(void);
void ui_skinned_playlist_slider_move_relative(GtkWidget *widget, gint x);
void ui_skinned_playlist_slider_resize_relative(GtkWidget *widget, gint h);
void ui_skinned_playlist_slider_update (GtkWidget * widget);

#endif /* AUDACIOUS_UI_SKINNED_PLAYLIST_SLIDER_H */
