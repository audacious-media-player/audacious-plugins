/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
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

#ifndef AUDACIOUS_UI_SKINNED_HORIZONTAL_SLIDER_H
#define AUDACIOUS_UI_SKINNED_HORIZONTAL_SLIDER_H

#include <gtk/gtk.h>
#include "ui_skin.h"

#define UI_SKINNED_HORIZONTAL_SLIDER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_skinned_horizontal_slider_get_type (), UiSkinnedHorizontalSlider)
#define UI_SKINNED_HORIZONTAL_SLIDER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_skinned_horizontal_slider_get_type (), UiSkinnedHorizontalSliderClass)
#define UI_SKINNED_IS_HORIZONTAL_SLIDER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_skinned_horizontal_slider_get_type ())

typedef struct _UiSkinnedHorizontalSlider        UiSkinnedHorizontalSlider;
typedef struct _UiSkinnedHorizontalSliderClass   UiSkinnedHorizontalSliderClass;

struct _UiSkinnedHorizontalSlider {
    GtkWidget   widget;
    GdkWindow   *event_window;
    gboolean    pressed;
    gint        x, y;
    gint        knob_nx, knob_ny, knob_px, knob_py;
};

struct _UiSkinnedHorizontalSliderClass {
    GtkWidgetClass    parent_class;
    void (* motion)   (UiSkinnedHorizontalSlider *horizontal_slider);
    void (* release)  (UiSkinnedHorizontalSlider *horizontal_slider);
    void (* scaled)  (UiSkinnedHorizontalSlider *horizontal_slider);
};
GtkWidget* ui_skinned_horizontal_slider_new(GtkWidget *fixed, gint x, gint y, gint w, gint h, gint knx, gint kny,
                                            gint kpx, gint kpy, gint kw, gint kh, gint fh,
                                            gint fo, gint min, gint max, gint(*fcb) (gint), SkinPixmapId si);
GType ui_skinned_horizontal_slider_get_type(void);
void ui_skinned_horizontal_slider_set_position(GtkWidget *widget, gint pos);
gint ui_skinned_horizontal_slider_get_position(GtkWidget *widget);

#endif /* AUDACIOUS_UI_SKINNED_HORIZONTAL_SLIDER_H */
