/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007  Audacious development team.
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

#ifndef UISVIS_H
#define UISVIS_H

#include <gtk/gtk.h>

#define UI_SVIS(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_svis_get_type (), UiSVis)
#define UI_SVIS_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_svis_get_type (), UiSVisClass)
#define UI_IS_SVIS(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_svis_get_type ())

typedef struct _UiSVis        UiSVis;
typedef struct _UiSVisClass   UiSVisClass;

struct _UiSVis {
    GtkWidget        widget;

    gint             x, y, width, height;
    gint             data[75];
    gint             refresh_delay;
    gboolean         scaled;
    GtkWidget        *fixed;
    gboolean         visible_window;
    GdkWindow        *event_window;
};

struct _UiSVisClass {
    GtkWidgetClass          parent_class;
    void (* scaled)        (UiSVis *vis);
};

GtkWidget* ui_svis_new (GtkWidget *fixed, gint x, gint y);
GType ui_svis_get_type(void);
void ui_svis_clear_data(GtkWidget *widget);
void ui_svis_timeout_func(GtkWidget *widget, guchar * data);

#endif
