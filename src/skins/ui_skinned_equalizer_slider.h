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
 * along with this program;  If not, see <http://www.gnu.org/licenses>.
 */

#ifndef AUDACIOUS_UI_SKINNED_EQUALIZER_SLIDER_H
#define AUDACIOUS_UI_SKINNED_EQUALIZER_SLIDER_H

#include <gtk/gtk.h>

#define UI_SKINNED_EQUALIZER_SLIDER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_skinned_equalizer_slider_get_type (), UiSkinnedEqualizerSlider)
#define UI_SKINNED_EQUALIZER_SLIDER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_skinned_equalizer_slider_get_type (),  UiSkinnedEqualizerSliderClass)
#define UI_SKINNED_IS_EQUALIZER_SLIDER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_skinned_equalizer_slider_get_type ())

typedef struct _UiSkinnedEqualizerSlider        UiSkinnedEqualizerSlider;
typedef struct _UiSkinnedEqualizerSliderClass   UiSkinnedEqualizerSliderClass;

struct _UiSkinnedEqualizerSlider {
    GtkWidget   widget;
    GdkWindow   *event_window;
    gint        x, y;
};

struct _UiSkinnedEqualizerSliderClass {
    GtkWidgetClass    parent_class;
    void (* scaled)  (UiSkinnedEqualizerSlider *equalizer_slider);
};

GtkWidget* ui_skinned_equalizer_slider_new(GtkWidget *fixed, gint x, gint y);
GType ui_skinned_equalizer_slider_get_type(void);
void ui_skinned_equalizer_slider_set_position(GtkWidget *widget, gfloat pos);
gfloat ui_skinned_equalizer_slider_get_position(GtkWidget *widget);

#endif /* AUDACIOUS_UI_SKINNED_EQUALIZER_SLIDER_H */
