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

#include <libaudcore/audstrings.h>
#include <libaudcore/equalizer.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "drawing.h"
#include "skins_cfg.h"
#include "ui_main.h"
#include "ui_skin.h"
#include "ui_skinned_equalizer_slider.h"

struct EqSliderData {
    String name;
    int band;
    int pos;
    float val;
    bool pressed;
};

DRAW_FUNC_BEGIN (eq_slider_draw, EqSliderData)
    int frame = 27 - data->pos * 27 / 50;
    if (frame < 14)
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 13 + 15 * frame, 164, 0, 0, 14, 63);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 13 + 15 * (frame - 14), 229, 0, 0, 14, 63);

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

    mainwin_show_status_message (str_printf ("%s: %+.1f dB", (const char *) data->name, data->val));
}

static gboolean eq_slider_button_press (GtkWidget * slider,
 GdkEventButton * event, EqSliderData * data)
{
    if (event->button != 1)
        return false;

    data->pressed = true;

    eq_slider_moved (data, event->y / config.scale - 5);
    gtk_widget_queue_draw (slider);
    return true;
}

static gboolean eq_slider_button_release (GtkWidget * slider,
 GdkEventButton * event, EqSliderData * data)
{
    if (event->button != 1)
        return false;

    if (! data->pressed)
        return true;

    data->pressed = false;

    eq_slider_moved (data, event->y / config.scale - 5);
    gtk_widget_queue_draw (slider);
    return true;
}

static gboolean eq_slider_motion (GtkWidget * slider, GdkEventMotion * event, EqSliderData * data)
{
    if (! data->pressed)
        return true;

    eq_slider_moved (data, event->y / config.scale - 5);
    gtk_widget_queue_draw (slider);
    return true;
}

static gboolean eq_slider_scroll (GtkWidget * slider, GdkEventScroll * event, EqSliderData * data)
{
    if (event->direction == GDK_SCROLL_UP)
        eq_slider_moved (data, data->pos - 2);
    else
        eq_slider_moved (data, data->pos + 2);

    gtk_widget_queue_draw (slider);
    return true;
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

static void eq_slider_free (EqSliderData * data)
{
    delete data;
}

GtkWidget * eq_slider_new (const char * name, int band)
{
    GtkWidget * slider = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) slider, false);
    gtk_widget_set_size_request (slider, 14 * config.scale, 63 * config.scale);
    gtk_widget_add_events (slider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);

    EqSliderData * data = new EqSliderData ();
    data->name = String (name);
    data->band = band;
    g_object_set_data_full ((GObject *) slider, "eqsliderdata", data,
     (GDestroyNotify) eq_slider_free);

    DRAW_CONNECT_PROXY (slider, eq_slider_draw, data);
    g_signal_connect (slider, "button-press-event", (GCallback) eq_slider_button_press, data);
    g_signal_connect (slider, "button-release-event", (GCallback) eq_slider_button_release, data);
    g_signal_connect (slider, "motion-notify-event", (GCallback) eq_slider_motion, data);
    g_signal_connect (slider, "scroll-event", (GCallback) eq_slider_scroll, data);

    return slider;
}
