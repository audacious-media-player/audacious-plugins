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

#include <libaudcore/objects.h>

#include "drawing.h"
#include "skins_cfg.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_playlist_slider.h"

static GtkWidget * pl_slider_list;
static int pl_slider_height;
static gboolean pl_slider_pressed;

DRAW_FUNC_BEGIN (pl_slider_draw, void)
    int rows, first;
    ui_skinned_playlist_row_info (pl_slider_list, & rows, & first);

    int range = active_length - rows;

    int y;
    if (active_length > rows)
        y = (first * (pl_slider_height - 19) + range / 2) / range;
    else
        y = 0;

    for (int i = 0; i < pl_slider_height / 29; i ++)
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 36, 42, 0, 29 * i, 8, 29);

    skin_draw_pixbuf (cr, SKIN_PLEDIT, pl_slider_pressed ? 61 : 52, 53, 0, y, 8, 18);
DRAW_FUNC_END

static void pl_slider_set_pos (int y)
{
    y = aud::clamp (y, 0, pl_slider_height - 19);

    int rows, first;
    ui_skinned_playlist_row_info (pl_slider_list, & rows, & first);

    int range = pl_slider_height - 19;

    ui_skinned_playlist_scroll_to (pl_slider_list, (y * (active_length - rows) +
     range / 2) / range);
}

static gboolean pl_slider_button_press (GtkWidget * slider, GdkEventButton * event)
{
    if (event->button != 1)
        return FALSE;

    pl_slider_pressed = TRUE;
    pl_slider_set_pos (event->y / config.scale - 9);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean pl_slider_button_release (GtkWidget * slider, GdkEventButton * event)
{
    if (event->button != 1)
        return FALSE;

    if (! pl_slider_pressed)
        return TRUE;

    pl_slider_pressed = FALSE;
    pl_slider_set_pos (event->y / config.scale - 9);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean pl_slider_motion (GtkWidget * slider, GdkEventMotion * event)
{
    if (! pl_slider_pressed)
        return TRUE;

    pl_slider_set_pos (event->y / config.scale - 9);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

GtkWidget * ui_skinned_playlist_slider_new (GtkWidget * list, int height)
{
    GtkWidget * slider = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) slider, false);
    gtk_widget_set_size_request (slider, 8 * config.scale, height * config.scale);
    gtk_widget_add_events (slider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    pl_slider_list = list;
    pl_slider_height = height;

    DRAW_CONNECT_PROXY (slider, pl_slider_draw, nullptr);
    g_signal_connect (slider, "button-press-event", (GCallback) pl_slider_button_press, nullptr);
    g_signal_connect (slider, "button-release-event", (GCallback) pl_slider_button_release, nullptr);
    g_signal_connect (slider, "motion-notify-event", (GCallback) pl_slider_motion, nullptr);

    return slider;
}

void ui_skinned_playlist_slider_resize (GtkWidget * slider, int height)
{
    pl_slider_height = height;
    gtk_widget_set_size_request (slider, 8 * config.scale, height * config.scale);
    gtk_widget_queue_draw (slider);
}

void ui_skinned_playlist_slider_update (GtkWidget * slider)
{
    gtk_widget_queue_draw (slider);
}
