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

#include <libaudcore/equalizer.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "draw-compat.h"
#include "skins_cfg.h"
#include "ui_main.h"
#include "ui_skin.h"
#include "ui_skinned_equalizer_slider.h"

typedef struct {
    char * name;
    int band;
    int pos;
    float val;
    gboolean pressed;
} EqSliderData;

DRAW_FUNC_BEGIN (eq_slider_draw)
    EqSliderData * data = (EqSliderData *) g_object_get_data ((GObject *) wid, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    int frame = 27 - data->pos * 27 / 50;
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

static void eq_slider_moved (EqSliderData * data, int pos)
{
    data->pos = aud::clamp (pos, 0, 50);
    if (data->pos == 24 || data->pos == 26)
        data->pos = 25;

    data->val = (float) (25 - data->pos) * AUD_EQ_MAX_GAIN / 25;

    if (data->band < 0)
        aud_set_double (nullptr, "equalizer_preamp", data->val);
    else
        aud_eq_set_band (data->band, data->val);

    char buf[100];
    snprintf (buf, sizeof buf, "%s: %+.1f dB", data->name, data->val);
    mainwin_show_status_message (buf);
}

static gboolean eq_slider_button_press (GtkWidget * slider, GdkEventButton *
 event)
{
    EqSliderData * data = (EqSliderData *) g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    data->pressed = TRUE;

    eq_slider_moved (data, event->y / config.scale - 5);
    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean eq_slider_button_release (GtkWidget * slider, GdkEventButton *
 event)
{
    EqSliderData * data = (EqSliderData *) g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    if (! data->pressed)
        return TRUE;

    data->pressed = FALSE;

    eq_slider_moved (data, event->y / config.scale - 5);
    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean eq_slider_motion (GtkWidget * slider, GdkEventMotion * event)
{
    EqSliderData * data = (EqSliderData *) g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (! data->pressed)
        return TRUE;

    eq_slider_moved (data, event->y / config.scale - 5);
    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean eq_slider_scroll (GtkWidget * slider, GdkEventScroll * event)
{
    EqSliderData * data = (EqSliderData *) g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_val_if_fail (data, FALSE);

    if (event->direction == GDK_SCROLL_UP)
        eq_slider_moved (data, data->pos - 2);
    else
        eq_slider_moved (data, data->pos + 2);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

void eq_slider_set_val (GtkWidget * slider, float val)
{
    EqSliderData * data = (EqSliderData *) g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_if_fail (data);

    if (data->pressed)
        return;

    data->val = val;
    data->pos = 25 - (int) (val * 25 / AUD_EQ_MAX_GAIN);
    data->pos = aud::clamp (data->pos, 0, 50);

    gtk_widget_queue_draw (slider);
}

static void eq_slider_destroy (GtkWidget * slider)
{
    EqSliderData * data = (EqSliderData *) g_object_get_data ((GObject *) slider, "eqsliderdata");
    g_return_if_fail (data);

    g_free (data->name);
    g_free (data);
}

GtkWidget * eq_slider_new (const char * name, int band)
{
    GtkWidget * slider = gtk_drawing_area_new ();
    gtk_widget_set_size_request (slider, 14 * config.scale, 63 * config.scale);
    gtk_widget_add_events (slider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);

    DRAW_CONNECT (slider, eq_slider_draw);

    g_signal_connect (slider, "button-press-event", (GCallback)
     eq_slider_button_press, nullptr);
    g_signal_connect (slider, "button-release-event", (GCallback)
     eq_slider_button_release, nullptr);
    g_signal_connect (slider, "motion-notify-event", (GCallback)
     eq_slider_motion, nullptr);
    g_signal_connect (slider, "scroll-event", (GCallback) eq_slider_scroll,
     nullptr);
    g_signal_connect (slider, "destroy", (GCallback) eq_slider_destroy, nullptr);

    EqSliderData * data = g_new0 (EqSliderData, 1);
    data->name = g_strdup (name);
    data->band = band;
    g_object_set_data ((GObject *) slider, "eqsliderdata", data);

    return slider;
}
