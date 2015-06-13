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

#include "skins_cfg.h"
#include "ui_skinned_horizontal_slider.h"

void HSlider::draw (cairo_t * cr)
{
    skin_draw_pixbuf (cr, m_si, m_fx, m_fy, 0, 0, m_w, m_h);

    if (m_pressed)
        skin_draw_pixbuf (cr, m_si, m_kpx, m_kpy, m_pos, (m_h - m_kh) / 2, m_kw, m_kh);
    else
        skin_draw_pixbuf (cr, m_si, m_knx, m_kny, m_pos, (m_h - m_kh) / 2, m_kw, m_kh);
}

bool HSlider::button_press (GdkEventButton * event)
{
    if (event->button != 1)
        return false;

    m_pressed = true;
    m_pos = aud::clamp ((int) event->x / config.scale - m_kw / 2, m_min, m_max);

    if (move)
        move ();

    gtk_widget_queue_draw (gtk ());
    return true;
}

bool HSlider::button_release (GdkEventButton * event)
{
    if (event->button != 1)
        return false;

    if (! m_pressed)
        return true;

    m_pressed = false;
    m_pos = aud::clamp ((int) event->x / config.scale - m_kw / 2, m_min, m_max);

    if (release)
        release ();

    gtk_widget_queue_draw (gtk ());
    return true;
}

bool HSlider::motion (GdkEventMotion * event)
{
    if (! m_pressed)
        return true;

    m_pressed = true;
    m_pos = aud::clamp ((int) event->x / config.scale - m_kw / 2, m_min, m_max);

    if (move)
        move ();

    gtk_widget_queue_draw (gtk ());
    return true;
}

HSlider::HSlider (int min, int max, SkinPixmapId si, int w, int h, int fx,
 int fy, int kw, int kh, int knx, int kny, int kpx, int kpy) :
    m_min (min), m_max (max), m_si (si), m_w (w), m_h (h), m_fx (fx), m_fy (fy),
     m_kw (kw), m_kh (kh), m_knx (knx), m_kny (kny), m_kpx (kpx), m_kpy (kpy)
{
    GtkWidget * hslider = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) hslider, false);
    gtk_widget_set_size_request (hslider, w * config.scale, h * config.scale);
    gtk_widget_add_events (hslider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    set_gtk (hslider, true);
}

void HSlider::set_frame (int fx, int fy)
{
    m_fx = fx;
    m_fy = fy;
    gtk_widget_queue_draw (gtk ());
}

void HSlider::set_knob (int knx, int kny, int kpx, int kpy)
{
    m_knx = knx;
    m_kny = kny;
    m_kpx = kpx;
    m_kpy = kpy;
    gtk_widget_queue_draw (gtk ());
}

void HSlider::set_pos (int pos)
{
    if (m_pressed)
        return;

    m_pos = aud::clamp (pos, m_min, m_max);
    gtk_widget_queue_draw (gtk ());
}

void HSlider::set_pressed (bool pressed)
{
    m_pressed = pressed;
    gtk_widget_queue_draw (gtk ());
}
