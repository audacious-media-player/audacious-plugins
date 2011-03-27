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

#include "ui_skin.h"
#include "ui_skinned_monostereo.h"
#include "skins_cfg.h"
#include "util.h"

enum {
    DOUBLED,
    LAST_SIGNAL
};

static void ui_skinned_monostereo_class_init         (UiSkinnedMonoStereoClass *klass);
static void ui_skinned_monostereo_init               (UiSkinnedMonoStereo *monostereo);
static void ui_skinned_monostereo_destroy            (GtkObject *object);
static void ui_skinned_monostereo_realize            (GtkWidget *widget);
static void ui_skinned_monostereo_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_monostereo_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_monostereo_expose         (GtkWidget *widget, GdkEventExpose *event);
static void ui_skinned_monostereo_toggle_scaled      (UiSkinnedMonoStereo *monostereo);

static GtkWidgetClass *parent_class = NULL;
static guint monostereo_signals[LAST_SIGNAL] = { 0 };

GType ui_skinned_monostereo_get_type() {
    static GType monostereo_type = 0;
    if (!monostereo_type) {
        static const GTypeInfo monostereo_info = {
            sizeof (UiSkinnedMonoStereoClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_monostereo_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedMonoStereo),
            0,
            (GInstanceInitFunc) ui_skinned_monostereo_init,
        };
        monostereo_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedMonoStereo", &monostereo_info, 0);
    }

    return monostereo_type;
}

static void ui_skinned_monostereo_class_init(UiSkinnedMonoStereoClass *klass) {
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = ui_skinned_monostereo_destroy;

    widget_class->realize = ui_skinned_monostereo_realize;
    widget_class->expose_event = ui_skinned_monostereo_expose;
    widget_class->size_request = ui_skinned_monostereo_size_request;
    widget_class->size_allocate = ui_skinned_monostereo_size_allocate;

    klass->scaled = ui_skinned_monostereo_toggle_scaled;

    monostereo_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedMonoStereoClass, scaled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void ui_skinned_monostereo_init(UiSkinnedMonoStereo *monostereo) {
    monostereo->width = 56;
    monostereo->height = 12;

    GTK_WIDGET_SET_FLAGS(monostereo, GTK_NO_WINDOW);
}

GtkWidget* ui_skinned_monostereo_new(GtkWidget *fixed, gint x, gint y, SkinPixmapId si) {
    UiSkinnedMonoStereo *monostereo = g_object_new (ui_skinned_monostereo_get_type (), NULL);

    monostereo->x = x;
    monostereo->y = y;
    monostereo->skin_index = si;
    monostereo->scaled = FALSE;

    gtk_fixed_put(GTK_FIXED(fixed), GTK_WIDGET(monostereo), monostereo->x, monostereo->y);

    return GTK_WIDGET(monostereo);
}

static void ui_skinned_monostereo_destroy(GtkObject *object) {
    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_MONOSTEREO (object));

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_monostereo_realize(GtkWidget *widget) {
    if (GTK_WIDGET_CLASS (parent_class)->realize)
        (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
}

static void ui_skinned_monostereo_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedMonoStereo *monostereo = UI_SKINNED_MONOSTEREO(widget);

    requisition->width = monostereo->width*(monostereo->scaled ? config.scale_factor : 1);
    requisition->height = monostereo->height*(monostereo->scaled ? config.scale_factor : 1);
}

static void ui_skinned_monostereo_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedMonoStereo *monostereo = UI_SKINNED_MONOSTEREO (widget);

    widget->allocation = *allocation;
    widget->allocation.x *= (monostereo->scaled ? config.scale_factor : 1);
    widget->allocation.y *= (monostereo->scaled ? config.scale_factor : 1);

    monostereo->x = widget->allocation.x/(monostereo->scaled ? config.scale_factor : 1);
    monostereo->y = widget->allocation.y/(monostereo->scaled ? config.scale_factor : 1);
}

static gboolean ui_skinned_monostereo_expose(GtkWidget *widget, GdkEventExpose *event) {
    UiSkinnedMonoStereo *monostereo = UI_SKINNED_MONOSTEREO (widget);
    g_return_val_if_fail (monostereo->width > 0 && monostereo->height > 0, FALSE);

    GdkPixbuf *obj = NULL;
    obj = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, monostereo->width, monostereo->height);

    switch (monostereo->num_channels) {
    case -1:
    case 0:
        skin_draw_pixbuf(widget, aud_active_skin, obj, monostereo->skin_index, 29, 12, 0, 0, 27, 12);
        skin_draw_pixbuf(widget, aud_active_skin, obj, monostereo->skin_index, 0, 12, 27, 0, 29, 12);
        break;
    case 1:
        skin_draw_pixbuf(widget, aud_active_skin, obj, monostereo->skin_index, 29, 0, 0, 0, 27, 12);
        skin_draw_pixbuf(widget, aud_active_skin, obj, monostereo->skin_index, 0, 12, 27, 0, 29, 12);
        break;
    default:
        skin_draw_pixbuf(widget, aud_active_skin, obj, monostereo->skin_index, 29, 12, 0, 0, 27, 12);
        skin_draw_pixbuf(widget, aud_active_skin, obj, monostereo->skin_index, 0, 0, 27, 0, 29, 12);
        break;
    }

    ui_skinned_widget_draw_with_coordinates(widget, obj, monostereo->width, monostereo->height,
                                            widget->allocation.x,
                                            widget->allocation.y,
                                            monostereo->scaled);

    g_object_unref(obj);

    return FALSE;
}

static void ui_skinned_monostereo_toggle_scaled(UiSkinnedMonoStereo *monostereo) {
    GtkWidget *widget = GTK_WIDGET (monostereo);

    monostereo->scaled = !monostereo->scaled;
    gtk_widget_set_size_request(widget, monostereo->width*(monostereo->scaled ? config.scale_factor : 1), monostereo->height*(monostereo->scaled ? config.scale_factor : 1));

    if (widget_really_drawable (widget))
        ui_skinned_monostereo_expose (widget, 0);
}

void ui_skinned_monostereo_set_num_channels(GtkWidget *widget, gint nch) {
    g_return_if_fail (UI_SKINNED_IS_MONOSTEREO (widget));
    UiSkinnedMonoStereo *monostereo = UI_SKINNED_MONOSTEREO (widget);

    monostereo->num_channels = nch;

    if (widget_really_drawable (widget))
        ui_skinned_monostereo_expose (widget, 0);
}
