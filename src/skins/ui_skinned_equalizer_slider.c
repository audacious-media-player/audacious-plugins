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

#include <libaudcore/i18n.h>
#include <audacious/types.h>

#include "draw-compat.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_skin.h"
#include "ui_skinned_equalizer_slider.h"

typedef struct {
    gchar * name;
    gint pos;
    gfloat val;
    gboolean pressed;
} EqSliderData;

DRAW_FUNC_BEGIN (eq_slider_draw)
    EqSliderData * data = g_object_get_data ((GObject *) wid, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    gint frame = 27 - data->pos * 27 / 50;
    if (frame < 14)
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 13 + 15 * frame, 164, 0, 0, 14, 63);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 13 + 15 * (frame - 14), 229, 0, 0,
         14, 63);

    if (data->pressed)
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 176, 1, data->pos, 11, 11);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 164, 1, data->pos, 11, 11);
DRAW_FUNC_END

static void eq_slider_moved (EqSliderData * data, gint pos)
{
    data->pos = CLAMP (pos, 0, 50);
    if (data->pos == 24 || data->pos == 26)
        data->pos = 25;

    data->val = (gfloat) (25 - data->pos) * AUD_EQ_MAX_GAIN / 25;

    equalizerwin_eq_changed ();

    gchar buf[100];
    snprintf (buf, sizeof buf, "%s: %+.1f dB", data->name, data->val);
    mainwin_show_status_message (buf);
}

static gboolean eq_slider_button_press (GtkWidget * slider, GdkEventButton *
 event)
{
    EqSliderData * data = g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    data->pressed = TRUE;

    eq_slider_moved (data, event->y - 5);
    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean eq_slider_button_release (GtkWidget * slider, GdkEventButton *
 event)
{
    EqSliderData * data = g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    if (! data->pressed)
        return TRUE;

    data->pressed = FALSE;

    eq_slider_moved (data, event->y - 5);
    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean eq_slider_motion (GtkWidget * slider, GdkEventMotion * event)
{
    EqSliderData * data = g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (! data->pressed)
        return TRUE;

    eq_slider_moved (data, event->y - 5);
    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean eq_slider_scroll (GtkWidget * slider, GdkEventScroll * event)
{
    EqSliderData * data = g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->direction == GDK_SCROLL_UP)
        eq_slider_moved (data, data->pos - 2);
    else
        eq_slider_moved (data, data->pos + 2);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

void eq_slider_set_val (GtkWidget * slider, gfloat val)
{
    EqSliderData * data = g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_if_fail (data);

    if (data->pressed)
        return;

    data->val = val;
    data->pos = 25 - (gint) (val * 25 / AUD_EQ_MAX_GAIN);
    data->pos = CLAMP (data->pos, 0, 50);

    gtk_widget_queue_draw (slider);
}

gfloat eq_slider_get_val (GtkWidget * slider)
{
    EqSliderData * data = g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, 0);

    return data->val;
}

static void eq_slider_destroy (GtkWidget * slider)
{
    EqSliderData * data = g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_if_fail (data);

    g_free (data->name);
    g_free (data);
}

GtkWidget * eq_slider_new (const gchar * name)
{
    GtkWidget * slider = gtk_drawing_area_new ();
    gtk_widget_set_size_request (slider, 14, 63);
    gtk_widget_add_events (slider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);

    DRAW_CONNECT (slider, eq_slider_draw);

    g_signal_connect (slider, "button-press-event", (GCallback)
     eq_slider_button_press, NULL);
    g_signal_connect (slider, "button-release-event", (GCallback)
     eq_slider_button_release, NULL);
    g_signal_connect (slider, "motion-notify-event", (GCallback)
     eq_slider_motion, NULL);
    g_signal_connect (slider, "scroll-event", (GCallback) eq_slider_scroll,
     NULL);
    g_signal_connect (slider, "destroy", (GCallback) eq_slider_destroy, NULL);

    EqSliderData * data = g_malloc0 (sizeof (EqSliderData));
    data->name = g_strdup (name);
    g_object_set_data ((GObject *) slider, "eqsliderdata", data);

    return slider;
}
