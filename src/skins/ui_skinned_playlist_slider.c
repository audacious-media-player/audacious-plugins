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
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_playlist_slider.h"

static GtkWidget * pl_slider_list;
static gint pl_slider_height;
static gboolean pl_slider_pressed;

DRAW_FUNC_BEGIN (pl_slider_draw)
    GdkPixbuf * p = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 8,
     pl_slider_height);

    gint rows, first, focused;
    ui_skinned_playlist_row_info (pl_slider_list, & rows, & first, & focused);

    gint range = active_length - rows;

    gint y;
    if (active_length > rows)
        y = (first * (pl_slider_height - 19) + range / 2) / range;
    else
        y = 0;

    for (gint i = 0; i < pl_slider_height / 29; i ++)
        skin_draw_pixbuf (wid, aud_active_skin, p, SKIN_PLEDIT, 36, 42, 0, 29 *
         i, 8, 29);

    skin_draw_pixbuf (wid, aud_active_skin, p, SKIN_PLEDIT, pl_slider_pressed ?
     61 : 52, 53, 0, y, 8, 18);

    pixbuf_draw (cr, p, 0, 0, FALSE);

    g_object_unref (p);
DRAW_FUNC_END

static void pl_slider_set_pos (gint y)
{
    y = CLAMP (y, 0, pl_slider_height - 19);

    gint rows, first, focused;
    ui_skinned_playlist_row_info (pl_slider_list, & rows, & first, & focused);

    gint range = pl_slider_height - 19;

    ui_skinned_playlist_scroll_to (pl_slider_list, (y * (active_length - rows) +
     range / 2) / range);
}

static gboolean pl_slider_button_press (GtkWidget * slider, GdkEventButton *
 event)
{
    if (event->button != 1)
        return FALSE;

    pl_slider_pressed = TRUE;
    pl_slider_set_pos (event->y - 9);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean pl_slider_button_release (GtkWidget * slider, GdkEventButton *
 event)
{
    if (event->button != 1)
        return FALSE;

    if (! pl_slider_pressed)
        return TRUE;

    pl_slider_pressed = FALSE;
    pl_slider_set_pos (event->y - 9);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

static gboolean pl_slider_motion (GtkWidget * slider, GdkEventMotion * event)
{
    if (! pl_slider_pressed)
        return TRUE;

    pl_slider_set_pos (event->y - 9);

    gtk_widget_queue_draw (slider);
    return TRUE;
}

GtkWidget * ui_skinned_playlist_slider_new (GtkWidget * list, gint height)
{
    GtkWidget * slider = gtk_drawing_area_new ();
    gtk_widget_set_size_request (slider, 8, height);
    gtk_widget_add_events (slider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    pl_slider_list = list;
    pl_slider_height = height;

    g_signal_connect (slider, DRAW_SIGNAL, (GCallback) pl_slider_draw, NULL);
    g_signal_connect (slider, "button-press-event", (GCallback)
     pl_slider_button_press, NULL);
    g_signal_connect (slider, "button-release-event", (GCallback)
     pl_slider_button_release, NULL);
    g_signal_connect (slider, "motion-notify-event", (GCallback)
     pl_slider_motion, NULL);

    return slider;
}

void ui_skinned_playlist_slider_resize (GtkWidget * slider, gint height)
{
    pl_slider_height = height;
    gtk_widget_set_size_request (slider, 8, height);
    gtk_widget_queue_draw (slider);
}

void ui_skinned_playlist_slider_update (GtkWidget * slider)
{
    gtk_widget_queue_draw (slider);
}
