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

#include <audacious/i18n.h>
#include <audacious/types.h>

#include "config.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_skin.h"
#include "ui_skinned_equalizer_slider.h"
#include "util.h"

#define UI_TYPE_SKINNED_EQUALIZER_SLIDER           (ui_skinned_equalizer_slider_get_type())
#define UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UI_TYPE_SKINNED_EQUALIZER_SLIDER, UiSkinnedEqualizerSliderPrivate))
typedef struct _UiSkinnedEqualizerSliderPrivate UiSkinnedEqualizerSliderPrivate;

enum {
    DOUBLED,
    LAST_SIGNAL
};

struct _UiSkinnedEqualizerSliderPrivate {
    SkinPixmapId     skin_index;
    gboolean         scaled;
    gint             position;
    gint             width, height;
    gboolean         pressed;
    gint             drag_y;
    gfloat           value; /* store gain as is to prevent truncation --asphyx */
};

static void ui_skinned_equalizer_slider_class_init         (UiSkinnedEqualizerSliderClass *klass);
static void ui_skinned_equalizer_slider_init               (UiSkinnedEqualizerSlider *equalizer_slider);
static void ui_skinned_equalizer_slider_destroy            (GtkObject *object);
static void ui_skinned_equalizer_slider_realize            (GtkWidget *widget);
static void ui_skinned_equalizer_slider_unrealize          (GtkWidget *widget);
static void ui_skinned_equalizer_slider_map                (GtkWidget *widget);
static void ui_skinned_equalizer_slider_unmap              (GtkWidget *widget);
static void ui_skinned_equalizer_slider_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_equalizer_slider_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_equalizer_slider_expose         (GtkWidget *widget, GdkEventExpose *event);
static gboolean ui_skinned_equalizer_slider_button_press   (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_equalizer_slider_button_release (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_equalizer_slider_motion_notify  (GtkWidget *widget, GdkEventMotion *event);
static gboolean ui_skinned_equalizer_slider_scroll         (GtkWidget *widget, GdkEventScroll *event);
static void ui_skinned_equalizer_slider_toggle_scaled      (UiSkinnedEqualizerSlider *equalizer_slider);
void ui_skinned_equalizer_slider_set_mainwin_text          (UiSkinnedEqualizerSlider * es);

static GtkWidgetClass *parent_class = NULL;
static guint equalizer_slider_signals[LAST_SIGNAL] = { 0 };

GType ui_skinned_equalizer_slider_get_type() {
    static GType equalizer_slider_type = 0;
    if (!equalizer_slider_type) {
        static const GTypeInfo equalizer_slider_info = {
            sizeof (UiSkinnedEqualizerSliderClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_equalizer_slider_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedEqualizerSlider),
            0,
            (GInstanceInitFunc) ui_skinned_equalizer_slider_init,
        };
        equalizer_slider_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedEqualizerSlider", &equalizer_slider_info, 0);
    }

    return equalizer_slider_type;
}

static void ui_skinned_equalizer_slider_class_init(UiSkinnedEqualizerSliderClass *klass) {
    GObjectClass *gobject_class;
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = ui_skinned_equalizer_slider_destroy;

    widget_class->realize = ui_skinned_equalizer_slider_realize;
    widget_class->unrealize = ui_skinned_equalizer_slider_unrealize;
    widget_class->map = ui_skinned_equalizer_slider_map;
    widget_class->unmap = ui_skinned_equalizer_slider_unmap;
    widget_class->expose_event = ui_skinned_equalizer_slider_expose;
    widget_class->size_request = ui_skinned_equalizer_slider_size_request;
    widget_class->size_allocate = ui_skinned_equalizer_slider_size_allocate;
    widget_class->button_press_event = ui_skinned_equalizer_slider_button_press;
    widget_class->button_release_event = ui_skinned_equalizer_slider_button_release;
    widget_class->motion_notify_event = ui_skinned_equalizer_slider_motion_notify;
    widget_class->scroll_event = ui_skinned_equalizer_slider_scroll;

    klass->scaled = ui_skinned_equalizer_slider_toggle_scaled;

    equalizer_slider_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedEqualizerSliderClass, scaled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_type_class_add_private (gobject_class, sizeof (UiSkinnedEqualizerSliderPrivate));
}

static void ui_skinned_equalizer_slider_init(UiSkinnedEqualizerSlider *equalizer_slider) {
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(equalizer_slider);
    priv->pressed = FALSE;

    equalizer_slider->event_window = NULL;
    GTK_WIDGET_SET_FLAGS(equalizer_slider, GTK_NO_WINDOW);
}

GtkWidget* ui_skinned_equalizer_slider_new(GtkWidget *fixed, gint x, gint y) {
    UiSkinnedEqualizerSlider *es = g_object_new (ui_skinned_equalizer_slider_get_type (), NULL);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(es);

    es->x = x;
    es->y = y;
    priv->width = 14;
    priv->height = 63;
    priv->skin_index = SKIN_EQMAIN;

    gtk_fixed_put(GTK_FIXED(fixed), GTK_WIDGET(es), es->x, es->y);

    return GTK_WIDGET(es);
}

static void ui_skinned_equalizer_slider_destroy(GtkObject *object) {
    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_EQUALIZER_SLIDER (object));

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_equalizer_slider_realize(GtkWidget *widget) {
    UiSkinnedEqualizerSlider *equalizer_slider;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_SKINNED_IS_EQUALIZER_SLIDER(widget));

    if (GTK_WIDGET_CLASS (parent_class)->realize)
        (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
    equalizer_slider = UI_SKINNED_EQUALIZER_SLIDER(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y;
    equalizer_slider->event_window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);

    gdk_window_set_user_data(equalizer_slider->event_window, widget);
}

static void ui_skinned_equalizer_slider_unrealize(GtkWidget *widget) {
    UiSkinnedEqualizerSlider *equalizer_slider = UI_SKINNED_EQUALIZER_SLIDER(widget);

    if ( equalizer_slider->event_window != NULL )
    {
        gdk_window_set_user_data( equalizer_slider->event_window , NULL );
        gdk_window_destroy( equalizer_slider->event_window );
        equalizer_slider->event_window = NULL;
    }

    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
        (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void ui_skinned_equalizer_slider_map (GtkWidget *widget)
{
    UiSkinnedEqualizerSlider *equalizer_slider = UI_SKINNED_EQUALIZER_SLIDER(widget);

    if (equalizer_slider->event_window != NULL)
        gdk_window_show (equalizer_slider->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->map)
        (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
}

static void ui_skinned_equalizer_slider_unmap (GtkWidget *widget)
{
    UiSkinnedEqualizerSlider *equalizer_slider = UI_SKINNED_EQUALIZER_SLIDER(widget);

    if (equalizer_slider->event_window != NULL)
        gdk_window_hide (equalizer_slider->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->unmap)
        (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void ui_skinned_equalizer_slider_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(widget);

    requisition->width = priv->width*(priv->scaled ? config.scale_factor : 1);
    requisition->height = priv->height*(priv->scaled ? config.scale_factor : 1);
}

static void ui_skinned_equalizer_slider_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedEqualizerSlider *equalizer_slider = UI_SKINNED_EQUALIZER_SLIDER (widget);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(equalizer_slider);

    widget->allocation = *allocation;
    widget->allocation.x *= (priv->scaled ? config.scale_factor : 1);
    widget->allocation.y *= (priv->scaled ? config.scale_factor : 1);
    if (GTK_WIDGET_REALIZED (widget))
        if (equalizer_slider->event_window)
            gdk_window_move_resize(equalizer_slider->event_window, widget->allocation.x, widget->allocation.y, allocation->width, allocation->height);

    equalizer_slider->x = widget->allocation.x/(priv->scaled ? config.scale_factor : 1);
    equalizer_slider->y = widget->allocation.y/(priv->scaled ? config.scale_factor : 1);
}

static gboolean ui_skinned_equalizer_slider_expose(GtkWidget *widget, GdkEventExpose *event) {
    UiSkinnedEqualizerSlider *es = UI_SKINNED_EQUALIZER_SLIDER (widget);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(es);
    g_return_val_if_fail (priv->width > 0 && priv->height > 0, FALSE);

    GdkPixbuf *obj = NULL;
    obj = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, priv->width, priv->height);

    gint frame;
    frame = 27 - ((priv->position * 27) / 50);
    if (frame < 14)
        skin_draw_pixbuf(widget, aud_active_skin, obj, priv->skin_index, (frame * 15) + 13, 164, 0, 0, priv->width, priv->height);
    else
        skin_draw_pixbuf(widget, aud_active_skin, obj, priv->skin_index, ((frame - 14) * 15) + 13, 229, 0, 0, priv->width, priv->height);

    if (priv->pressed)
        skin_draw_pixbuf(widget, aud_active_skin, obj, priv->skin_index, 0, 176, 1, priv->position, 11, 11);
    else
        skin_draw_pixbuf(widget, aud_active_skin, obj, priv->skin_index, 0, 164, 1, priv->position, 11, 11);

    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));
    pixbuf_draw (cr, obj, widget->allocation.x, widget->allocation.y,
     priv->scaled);
    cairo_destroy (cr);

    g_object_unref (obj);
    return FALSE;
}

static gboolean ui_skinned_equalizer_slider_button_press(GtkWidget *widget, GdkEventButton *event) {
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_EQUALIZER_SLIDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    UiSkinnedEqualizerSlider *es = UI_SKINNED_EQUALIZER_SLIDER (widget);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(es);

    gint y;

    if (event->type == GDK_BUTTON_PRESS) {
        if (event->button == 1) {
            priv->pressed = TRUE;
            y = event->y/(priv->scaled ? config.scale_factor : 1);

            if (y >= priv->position && y < priv->position + 11)
                priv->drag_y = y - priv->position;
            else {
                priv->position = y - 5;
                priv->drag_y = 5;
                if (priv->position < 0)
                    priv->position = 0;
                if (priv->position > 50)
                    priv->position = 50;
                if (priv->position >= 24 && priv->position <= 26)
                    priv->position = 25;

                priv->value = ((gfloat) (25 - priv->position) * EQUALIZER_MAX_GAIN / 25.0 );
                equalizerwin_eq_changed();
            }

            ui_skinned_equalizer_slider_set_mainwin_text(es);

            if (widget_really_drawable (widget))
                ui_skinned_equalizer_slider_expose (widget, 0);
        }
    }

    return TRUE;
}

static gboolean ui_skinned_equalizer_slider_button_release(GtkWidget *widget, GdkEventButton *event) {
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(widget);

    if (event->button == 1) {
        priv->pressed = FALSE;
        mainwin_release_info_text();

        if (widget_really_drawable (widget))
            ui_skinned_equalizer_slider_expose (widget, 0);
    }
    return TRUE;
}

static gboolean ui_skinned_equalizer_slider_motion_notify(GtkWidget *widget, GdkEventMotion *event) {
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_EQUALIZER_SLIDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    UiSkinnedEqualizerSlider *es = UI_SKINNED_EQUALIZER_SLIDER(widget);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(widget);

    if (priv->pressed) {
        gint y;

        y = event->y/(priv->scaled ? config.scale_factor : 1);
        priv->position = y - priv->drag_y;

        if (priv->position < 0)
            priv->position = 0;
        if (priv->position > 50)
            priv->position = 50;
        if (priv->position >= 24 && priv->position <= 26)
            priv->position = 25;

        priv->value = ((gfloat) (25 - priv->position) * EQUALIZER_MAX_GAIN / 25.0 );
        ui_skinned_equalizer_slider_set_mainwin_text(es);
        equalizerwin_eq_changed();

        if (widget_really_drawable (widget))
            ui_skinned_equalizer_slider_expose (widget, 0);
    }

    return TRUE;
}

static gboolean ui_skinned_equalizer_slider_scroll(GtkWidget *widget, GdkEventScroll *event) {
    g_return_val_if_fail (UI_SKINNED_IS_EQUALIZER_SLIDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(widget);

    if (event->direction == GDK_SCROLL_UP) {
        priv->position -= 2;

        if (priv->position < 0)
            priv->position = 0;
    }
    else {
        priv->position += 2;

        if (priv->position > 50)
            priv->position = 50;
    }

    priv->value = ((gfloat) (25 - priv->position) * EQUALIZER_MAX_GAIN / 25.0 );
    equalizerwin_eq_changed();

    if (widget_really_drawable (widget))
        ui_skinned_equalizer_slider_expose (widget, 0);

    return TRUE;
}

static void ui_skinned_equalizer_slider_toggle_scaled(UiSkinnedEqualizerSlider *equalizer_slider) {
    GtkWidget *widget = GTK_WIDGET (equalizer_slider);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(equalizer_slider);

    priv->scaled = !priv->scaled;

    gtk_widget_set_size_request(widget, priv->width*(priv->scaled ? config.scale_factor : 1),
    priv->height*(priv->scaled ? config.scale_factor : 1));

    if (widget_really_drawable (widget))
        ui_skinned_equalizer_slider_expose (widget, 0);
}

void ui_skinned_equalizer_slider_set_position(GtkWidget *widget, gfloat pos) {
    g_return_if_fail (UI_SKINNED_IS_EQUALIZER_SLIDER (widget));
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(widget);

    if (priv->pressed)
        return;

    priv->value = (pos > EQUALIZER_MAX_GAIN) ? EQUALIZER_MAX_GAIN : ((pos < -EQUALIZER_MAX_GAIN) ? -EQUALIZER_MAX_GAIN : pos);
    priv->position = 25 - (gint) ((pos * 25.0) / EQUALIZER_MAX_GAIN);

    if (priv->position < 0)
        priv->position = 0;

    if (priv->position > 50)
        priv->position = 50;

    if (priv->position >= 24 && priv->position <= 26)
        priv->position = 25;

    if (widget_really_drawable (widget))
        ui_skinned_equalizer_slider_expose (widget, 0);
}

gfloat ui_skinned_equalizer_slider_get_position(GtkWidget *widget) {
    g_return_val_if_fail (UI_SKINNED_IS_EQUALIZER_SLIDER (widget), -1);
    UiSkinnedEqualizerSliderPrivate *priv = UI_SKINNED_EQUALIZER_SLIDER_GET_PRIVATE(widget);
    return priv->value;
}

void ui_skinned_equalizer_slider_set_mainwin_text(UiSkinnedEqualizerSlider * es) {
    gint band = 0;
    const gchar * const bandname[11] = {N_("Preamp"), N_("31 Hz"), N_("63 Hz"),
     N_("125 Hz"), N_("250 Hz"), N_("500 Hz"), N_("1 kHz"), N_("2 kHz"),
     N_("4 kHz"), N_("8 kHz"), N_("16 kHz")};
    gchar *tmp;

    if (es->x > 21)
        band = ((es->x - 78) / 18) + 1;

    tmp =
        g_strdup_printf("EQ: %s: %+.1f DB", _(bandname[band]),
                        ui_skinned_equalizer_slider_get_position(GTK_WIDGET(es)));
    mainwin_lock_info_text(tmp);
    g_free(tmp);
}
