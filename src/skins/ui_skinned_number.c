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

#include "ui_skinned_number.h"
#include "skins_cfg.h"
#include "util.h"

#include <string.h>
#include <ctype.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmarshal.h>

#define UI_TYPE_SKINNED_NUMBER           (ui_skinned_number_get_type())

enum {
    DOUBLED,
    LAST_SIGNAL
};

static void ui_skinned_number_class_init         (UiSkinnedNumberClass *klass);
static void ui_skinned_number_init               (UiSkinnedNumber *number);
static void ui_skinned_number_destroy            (GtkObject *object);
static void ui_skinned_number_realize            (GtkWidget *widget);
static void ui_skinned_number_unrealize          (GtkWidget *widget);
static void ui_skinned_number_map                (GtkWidget *widget);
static void ui_skinned_number_unmap              (GtkWidget *widget);
static void ui_skinned_number_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_number_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_number_expose         (GtkWidget *widget, GdkEventExpose *event);
static void ui_skinned_number_toggle_scaled  (UiSkinnedNumber *number);

static GtkWidgetClass *parent_class = NULL;
static guint number_signals[LAST_SIGNAL] = { 0 };

GType ui_skinned_number_get_type() {
    static GType number_type = 0;
    if (!number_type) {
        static const GTypeInfo number_info = {
            sizeof (UiSkinnedNumberClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_number_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedNumber),
            0,
            (GInstanceInitFunc) ui_skinned_number_init,
        };
        number_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedNumber", &number_info, 0);
    }

    return number_type;
}

static void ui_skinned_number_class_init(UiSkinnedNumberClass *klass) {
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = ui_skinned_number_destroy;

    widget_class->realize = ui_skinned_number_realize;
    widget_class->unrealize = ui_skinned_number_unrealize;
    widget_class->map = ui_skinned_number_map;
    widget_class->unmap = ui_skinned_number_unmap;
    widget_class->expose_event = ui_skinned_number_expose;
    widget_class->size_request = ui_skinned_number_size_request;
    widget_class->size_allocate = ui_skinned_number_size_allocate;

    klass->scaled = ui_skinned_number_toggle_scaled;

    number_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedNumberClass, scaled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void ui_skinned_number_init(UiSkinnedNumber *number) {
    number->width = 9;
    number->height = 13;

    number->event_window = NULL;
    GTK_WIDGET_SET_FLAGS(number, GTK_NO_WINDOW);
}

GtkWidget* ui_skinned_number_new(GtkWidget *fixed, gint x, gint y, SkinPixmapId si) {
    UiSkinnedNumber *number = g_object_new (ui_skinned_number_get_type (), NULL);

    number->x = x;
    number->y = y;
    number->num = 0;
    number->skin_index = si;

    number->scaled = FALSE;

    gtk_fixed_put(GTK_FIXED(fixed), GTK_WIDGET(number), number->x, number->y);

    return GTK_WIDGET(number);
}

static void ui_skinned_number_destroy(GtkObject *object) {
    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_NUMBER (object));

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_number_realize(GtkWidget *widget) {
    UiSkinnedNumber *number;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_SKINNED_IS_NUMBER(widget));

    if (GTK_WIDGET_CLASS (parent_class)->realize)
        (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
    number = UI_SKINNED_NUMBER(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= GDK_BUTTON_PRESS_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y;
    number->event_window = gdk_window_new (widget->window, &attributes, attributes_mask);

    gdk_window_set_user_data(number->event_window, widget);
}

static void ui_skinned_number_unrealize(GtkWidget *widget) {
    UiSkinnedNumber *number = UI_SKINNED_NUMBER(widget);

    if ( number->event_window != NULL )
    {
        gdk_window_set_user_data( number->event_window , NULL );
        gdk_window_destroy( number->event_window );
        number->event_window = NULL;
    }

    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
        (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void ui_skinned_number_map (GtkWidget *widget)
{
    UiSkinnedNumber *number = UI_SKINNED_NUMBER (widget);

    if (number->event_window != NULL)
        gdk_window_show (number->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->map)
        (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
}

static void ui_skinned_number_unmap (GtkWidget *widget)
{
    UiSkinnedNumber *number = UI_SKINNED_NUMBER (widget);

    if (number->event_window != NULL)
        gdk_window_hide (number->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->unmap)
        (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void ui_skinned_number_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedNumber *number = UI_SKINNED_NUMBER(widget);

    requisition->width = number->width * ( number->scaled ? config.scale_factor : 1 );
    requisition->height = number->height*( number->scaled ? config.scale_factor : 1);
}

static void ui_skinned_number_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedNumber *number = UI_SKINNED_NUMBER (widget);

    widget->allocation = *allocation;
    widget->allocation.x *= (number->scaled ? config.scale_factor: 1 );
    widget->allocation.y *= (number->scaled ? config.scale_factor: 1 );
    if (GTK_WIDGET_REALIZED (widget))
        if (number->event_window)
            gdk_window_move_resize(number->event_window, widget->allocation.x, widget->allocation.y, allocation->width, allocation->height);

    number->x = widget->allocation.x/(number->scaled ? config.scale_factor : 1);
    number->y = widget->allocation.y/(number->scaled ? config.scale_factor : 1);
}

static gboolean ui_skinned_number_expose(GtkWidget *widget, GdkEventExpose *event) {
    UiSkinnedNumber *number = UI_SKINNED_NUMBER (widget);
    g_return_val_if_fail (number->width > 0 && number->height > 0, FALSE);

    GdkPixbuf *obj;
    obj = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, number->width, number->height);

    if (number->num > 11 || number->num < 0)
        number->num = 10;

    skin_draw_pixbuf(widget, aud_active_skin, obj,
                     number->skin_index, number->num * 9, 0,
                     0, 0, number->width, number->height);

    ui_skinned_widget_draw_with_coordinates(widget, obj, number->width, number->height,
                                            widget->allocation.x,
                                            widget->allocation.y,
                                            number->scaled);

    g_object_unref(obj);

    return FALSE;
}

static void ui_skinned_number_toggle_scaled(UiSkinnedNumber *number) {
    GtkWidget *widget = GTK_WIDGET (number);
    number->scaled = !number->scaled;

    gtk_widget_set_size_request(widget, number->width * ( number->scaled ? config.scale_factor : 1),
        number->height * ( number->scaled ? config.scale_factor : 1) );

    if (widget_really_drawable (widget))
        ui_skinned_number_expose (widget, 0);
}

void ui_skinned_number_set (GtkWidget * widget, gchar c)
{
    UiSkinnedNumber * number = (UiSkinnedNumber *) widget;
    gint value = (c >= '0' && c <= '9') ? c - '0' : (c == '-') ? 11 : 10;

    if (number->num == value)
        return;

    number->num = value;

    if (widget_really_drawable (widget))
        ui_skinned_number_expose (widget, 0);
}

void ui_skinned_number_set_size(GtkWidget *widget, gint width, gint height) {
    g_return_if_fail(UI_SKINNED_IS_NUMBER(widget));
    UiSkinnedNumber *number = UI_SKINNED_NUMBER (widget);

    number->width = width;
    number->height = height;

    gtk_widget_set_size_request(widget, width*(number->scaled ? config.scale_factor : 1 ),
    height*(number->scaled ? config.scale_factor : 1 ));
}
