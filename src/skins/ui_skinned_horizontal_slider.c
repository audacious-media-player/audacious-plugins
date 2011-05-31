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

#include "draw-compat.h"
#include "ui_skin.h"
#include "ui_skinned_horizontal_slider.h"

typedef struct {
    gint min, max, pos;
    gboolean pressed;
    SkinPixmapId si;
    gint w, h;
    gint fx, fy;
    gint kw, kh;
    gint knx, kny, kpx, kpy;
    void (* on_motion) (void);
    void (* on_release) (void);
} HSliderData;

DRAW_FUNC_BEGIN (hslider_draw)
    HSliderData * data = g_object_get_data ((GObject *) wid, "hsliderdata");
    g_return_val_if_fail (data, FALSE);

    GdkPixbuf * p = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, data->w,
     data->h);

    skin_draw_pixbuf (wid, aud_active_skin, p, data->si, data->fx, data->fy,
     0, 0, data->w, data->h);

    if (data->pressed)
        skin_draw_pixbuf (wid, aud_active_skin, p, data->si, data->kpx,
         data->kpy, data->pos, (data->h - data->kh) / 2, data->kw, data->kh);
    else
        skin_draw_pixbuf (wid, aud_active_skin, p, data->si, data->knx,
         data->kny, data->pos, (data->h - data->kh) / 2, data->kw, data->kh);

    pixbuf_draw (cr, p, 0, 0, FALSE);

    g_object_unref (p);
DRAW_FUNC_END

static gboolean hslider_button_press (GtkWidget * hslider, GdkEventButton *
 event)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    data->pressed = TRUE;
    data->pos = event->x - data->kw / 2;
    data->pos = CLAMP (data->pos, data->min, data->max);

    if (data->on_motion)
        data->on_motion ();

    gtk_widget_queue_draw (hslider);
    return TRUE;
}

static gboolean hslider_button_release (GtkWidget * hslider, GdkEventButton *
 event)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    if (! data->pressed)
        return TRUE;

    data->pressed = FALSE;
    data->pos = event->x - data->kw / 2;
    data->pos = CLAMP (data->pos, data->min, data->max);

    if (data->on_release)
        data->on_release ();

    gtk_widget_queue_draw (hslider);
    return TRUE;
}

static gboolean hslider_motion_notify (GtkWidget * hslider, GdkEventMotion *
 event)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (! data->pressed)
        return TRUE;

    data->pressed = TRUE;
    data->pos = event->x - data->kw / 2;
    data->pos = CLAMP (data->pos, data->min, data->max);

    if (data->on_motion)
        data->on_motion ();

    gtk_widget_queue_draw (hslider);
    return TRUE;
}

static void hslider_destroy (GtkWidget * hslider)
{
    g_free (g_object_get_data ((GObject *) hslider, "hsliderdata"));
}

GtkWidget * hslider_new (gint min, gint max, SkinPixmapId si, gint w, gint h,
 gint fx, gint fy, gint kw, gint kh, gint knx, gint kny, gint kpx, gint kpy)
{
    GtkWidget * hslider = gtk_drawing_area_new ();
    gtk_widget_set_size_request (hslider, w, h);
    gtk_widget_add_events (hslider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect (hslider, DRAW_SIGNAL, (GCallback) hslider_draw, NULL);

    g_signal_connect (hslider, "button-press-event", (GCallback)
     hslider_button_press, NULL);
    g_signal_connect (hslider, "button-release-event", (GCallback)
     hslider_button_release, NULL);
    g_signal_connect (hslider, "motion-notify-event", (GCallback)
     hslider_motion_notify, NULL);
    g_signal_connect (hslider, "destroy", (GCallback) hslider_destroy, NULL);

    HSliderData * data = g_malloc0 (sizeof (HSliderData));
    data->min = min;
    data->max = max;
    data->pos = min;
    data->si = si;
    data->w = w;
    data->h = h;
    data->fx = fx;
    data->fy = fy;
    data->kw = kw;
    data->kh = kh;
    data->knx = knx;
    data->kny = kny;
    data->kpx = kpx;
    data->kpy = kpy;
    g_object_set_data ((GObject *) hslider, "hsliderdata", data);

    return hslider;
}

void hslider_set_frame (GtkWidget * hslider, gint fx, gint fy)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_if_fail (data);

    data->fx = fx;
    data->fy = fy;
    gtk_widget_queue_draw (hslider);
}

void hslider_set_knob (GtkWidget * hslider, gint knx, gint kny, gint kpx, gint
 kpy)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_if_fail (data);

    data->knx = knx;
    data->kny = kny;
    data->kpx = kpx;
    data->kpy = kpy;
    gtk_widget_queue_draw (hslider);
}

gint hslider_get_pos (GtkWidget * hslider)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_val_if_fail (data, 0);

    return data->pos;
}

void hslider_set_pos (GtkWidget * hslider, gint pos)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_if_fail (data);

    if (data->pressed)
        return;

    data->pos = CLAMP (pos, data->min, data->max);
    gtk_widget_queue_draw (hslider);
}

gboolean hslider_get_pressed (GtkWidget * hslider)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_val_if_fail (data, FALSE);

    return data->pressed;
}

void hslider_set_pressed (GtkWidget * hslider, gboolean pressed)
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_if_fail (data);

    data->pressed = pressed;
    gtk_widget_queue_draw (hslider);
}

void hslider_on_motion (GtkWidget * hslider, void (* callback) (void))
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_if_fail (data);

    data->on_motion = callback;
}

void hslider_on_release (GtkWidget * hslider, void (* callback) (void))
{
    HSliderData * data = g_object_get_data ((GObject *) hslider, "hsliderdata");
    g_return_if_fail (data);

    data->on_release = callback;
}
