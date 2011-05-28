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

#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_skinned_playstatus.h"
#include "util.h"

#define UI_TYPE_SKINNED_PLAYSTATUS           (ui_skinned_playstatus_get_type())

enum {
    DOUBLED,
    LAST_SIGNAL
};

static void ui_skinned_playstatus_class_init         (UiSkinnedPlaystatusClass *klass);
static void ui_skinned_playstatus_init               (UiSkinnedPlaystatus *playstatus);
static void ui_skinned_playstatus_destroy            (GtkObject *object);
static void ui_skinned_playstatus_realize            (GtkWidget *widget);
static void ui_skinned_playstatus_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_playstatus_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_playstatus_expose         (GtkWidget *widget, GdkEventExpose *event);
static void ui_skinned_playstatus_toggle_scaled      (UiSkinnedPlaystatus *playstatus);

static GtkWidgetClass *parent_class = NULL;
static guint playstatus_signals[LAST_SIGNAL] = { 0 };

GType ui_skinned_playstatus_get_type() {
    static GType playstatus_type = 0;
    if (!playstatus_type) {
        static const GTypeInfo playstatus_info = {
            sizeof (UiSkinnedPlaystatusClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_playstatus_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedPlaystatus),
            0,
            (GInstanceInitFunc) ui_skinned_playstatus_init,
        };
        playstatus_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedPlaystatus", &playstatus_info, 0);
    }

    return playstatus_type;
}

static void ui_skinned_playstatus_class_init(UiSkinnedPlaystatusClass *klass) {
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = ui_skinned_playstatus_destroy;

    widget_class->realize = ui_skinned_playstatus_realize;
    widget_class->expose_event = ui_skinned_playstatus_expose;
    widget_class->size_request = ui_skinned_playstatus_size_request;
    widget_class->size_allocate = ui_skinned_playstatus_size_allocate;

    klass->scaled = ui_skinned_playstatus_toggle_scaled;

    playstatus_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedPlaystatusClass, scaled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void ui_skinned_playstatus_init(UiSkinnedPlaystatus *playstatus) {
    playstatus->width = 11;
    playstatus->height = 9;

    GTK_WIDGET_SET_FLAGS(playstatus, GTK_NO_WINDOW);
}

GtkWidget* ui_skinned_playstatus_new(GtkWidget *fixed, gint x, gint y) {
    UiSkinnedPlaystatus *playstatus = g_object_new (ui_skinned_playstatus_get_type (), NULL);

    playstatus->x = x;
    playstatus->y = y;

    playstatus->scaled = FALSE;

    gtk_fixed_put(GTK_FIXED(fixed), GTK_WIDGET(playstatus), playstatus->x, playstatus->y);

    return GTK_WIDGET(playstatus);
}

static void ui_skinned_playstatus_destroy(GtkObject *object) {
    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_PLAYSTATUS (object));

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_playstatus_realize(GtkWidget *widget) {
    if (GTK_WIDGET_CLASS (parent_class)->realize)
        (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
}

static void ui_skinned_playstatus_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedPlaystatus *playstatus = UI_SKINNED_PLAYSTATUS(widget);

    requisition->width = playstatus->width*(playstatus->scaled ? config.scale_factor : 1);
    requisition->height = playstatus->height*(playstatus->scaled ? config.scale_factor : 1);
}

static void ui_skinned_playstatus_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedPlaystatus *playstatus = UI_SKINNED_PLAYSTATUS (widget);

    widget->allocation = *allocation;
    widget->allocation.x *= (playstatus->scaled ? config.scale_factor : 1);
    widget->allocation.y *= (playstatus->scaled ? config.scale_factor : 1);

    playstatus->x = widget->allocation.x/(playstatus->scaled ? config.scale_factor : 1);
    playstatus->y = widget->allocation.y/(playstatus->scaled ? config.scale_factor : 1);
}

static gboolean ui_skinned_playstatus_expose(GtkWidget *widget, GdkEventExpose *event) {
    UiSkinnedPlaystatus *playstatus = UI_SKINNED_PLAYSTATUS (widget);
    g_return_val_if_fail (playstatus->width > 0 && playstatus->height > 0, FALSE);

    GdkPixbuf *obj = NULL;
    obj = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, playstatus->width, playstatus->height);

    if (playstatus->status == STATUS_STOP && playstatus->buffering == TRUE)
        playstatus->buffering = FALSE;
    if (playstatus->status == STATUS_PLAY && playstatus->buffering == TRUE)
        skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_PLAYPAUSE, 39, 0, 0, 0, 3, playstatus->height);
    else if (playstatus->status == STATUS_PLAY)
        skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_PLAYPAUSE, 36, 0, 0, 0, 3, playstatus->height);
    else
        skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_PLAYPAUSE, 27, 0, 0, 0, 2, playstatus->height);
    switch (playstatus->status) {
    case STATUS_STOP:
        skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_PLAYPAUSE, 18, 0, 2, 0, 9, playstatus->height);
        break;
    case STATUS_PAUSE:
        skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_PLAYPAUSE, 9, 0, 2, 0, 9, playstatus->height);
        break;
    case STATUS_PLAY:
        skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_PLAYPAUSE, 1, 0, 3, 0, 8, playstatus->height);
        break;
    }

    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));
    pixbuf_draw (cr, obj, widget->allocation.x, widget->allocation.y,
     playstatus->scaled);
    cairo_destroy (cr);

    g_object_unref (obj);
    return FALSE;
}

static void ui_skinned_playstatus_toggle_scaled(UiSkinnedPlaystatus *playstatus) {
    GtkWidget *widget = GTK_WIDGET (playstatus);

    playstatus->scaled = !playstatus->scaled;
    gtk_widget_set_size_request(widget, playstatus->width*(playstatus->scaled ? config.scale_factor : 1), playstatus->height*(playstatus->scaled ? config.scale_factor : 1));

    if (widget_really_drawable (widget))
        ui_skinned_playstatus_expose (widget, 0);
}

void ui_skinned_playstatus_set_status(GtkWidget *widget, PStatus status) {
    g_return_if_fail (UI_SKINNED_IS_PLAYSTATUS (widget));
    UiSkinnedPlaystatus *playstatus = UI_SKINNED_PLAYSTATUS (widget);

    playstatus->status = status;

    if (widget_really_drawable (widget))
        ui_skinned_playstatus_expose (widget, 0);
}

void ui_skinned_playstatus_set_buffering(GtkWidget *widget, gboolean status) {
    g_return_if_fail (UI_SKINNED_IS_PLAYSTATUS (widget));
    UiSkinnedPlaystatus *playstatus = UI_SKINNED_PLAYSTATUS (widget);

    playstatus->buffering = status;

    if (widget_really_drawable (widget))
        ui_skinned_playstatus_expose (widget, 0);
}

void ui_skinned_playstatus_set_size(GtkWidget *widget, gint width, gint height) {
    g_return_if_fail (UI_SKINNED_IS_PLAYSTATUS (widget));
    UiSkinnedPlaystatus *playstatus = UI_SKINNED_PLAYSTATUS (widget);

    playstatus->width = width;
    playstatus->height = height;

    gtk_widget_set_size_request(widget, width*(playstatus->scaled ? config.scale_factor : 1), height*(playstatus->scaled ? config.scale_factor : 1));
}
