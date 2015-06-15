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

void PlaylistSlider::draw (cairo_t * cr)
{
    int rows, first;
    m_list->row_info (& rows, & first);

    int range = active_length - rows;

    int y;
    if (active_length > rows)
        y = (first * (m_height - 19) + range / 2) / range;
    else
        y = 0;

    for (int i = 0; i < m_height / 29; i ++)
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 36, 42, 0, 29 * i, 8, 29);

    skin_draw_pixbuf (cr, SKIN_PLEDIT, m_pressed ? 61 : 52, 53, 0, y, 8, 18);
}

void PlaylistSlider::set_pos (int y)
{
    y = aud::clamp (y, 0, m_height - 19);

    int rows, first;
    m_list->row_info (& rows, & first);

    int range = m_height - 19;
    m_list->scroll_to ((y * (active_length - rows) + range / 2) / range);
}

bool PlaylistSlider::button_press (GdkEventButton * event)
{
    if (event->button != 1)
        return false;

    m_pressed = true;
    set_pos (event->y / config.scale - 9);

    gtk_widget_queue_draw (gtk_dr ());
    return true;
}

bool PlaylistSlider::button_release (GdkEventButton * event)
{
    if (event->button != 1)
        return false;

    if (! m_pressed)
        return true;

    m_pressed = false;
    set_pos (event->y / config.scale - 9);

    gtk_widget_queue_draw (gtk_dr ());
    return true;
}

bool PlaylistSlider::motion (GdkEventMotion * event)
{
    if (! m_pressed)
        return true;

    set_pos (event->y / config.scale - 9);

    gtk_widget_queue_draw (gtk_dr ());
    return true;
}

PlaylistSlider::PlaylistSlider (PlaylistWidget * list, int height) :
    m_list (list), m_height (height)
{
    GtkWidget * slider = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) slider, false);
    gtk_widget_set_size_request (slider, 8 * config.scale, height * config.scale);
    gtk_widget_add_events (slider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    set_gtk (slider, true);
}

void PlaylistSlider::resize (int height)
{
    m_height = height;
    gtk_widget_set_size_request (gtk (), 8 * config.scale, height * config.scale);
    gtk_widget_queue_draw (gtk_dr ());
}

void PlaylistSlider::update ()
{
    gtk_widget_queue_draw (gtk_dr ());
}
