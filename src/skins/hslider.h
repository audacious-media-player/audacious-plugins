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

#ifndef SKINS_UI_SKINNED_HORIZONTAL_SLIDER_H
#define SKINS_UI_SKINNED_HORIZONTAL_SLIDER_H

#include "widget.h"
#include "skin.h"

class HSlider : public Widget
{
public:
    HSlider (int min, int max, SkinPixmapId si, int w, int h, int fx, int fy,
     int kw, int kh, int knx, int kny, int kpx, int kpy);

    void set_frame (int fx, int fy);
    void set_knob (int knx, int kny, int kpx, int kpy);
    int get_pos () { return m_pos; }
    void set_pos (int pos);
    bool get_pressed () { return m_pressed; }
    void set_pressed (bool pressed);

    void on_move (void (* callback) ()) { move = callback; }
    void on_release (void (* callback) ()) { release = callback; }

private:
    void draw (cairo_t * cr);
    bool button_press (GdkEventButton * event);
    bool button_release (GdkEventButton * event);
    bool motion (GdkEventMotion * event);

    int m_min, m_max;
    SkinPixmapId m_si;
    int m_w, m_h;
    int m_fx, m_fy;
    int m_kw, m_kh;
    int m_knx, m_kny, m_kpx, m_kpy;

    int m_pos = 0;
    bool m_pressed = false;

    void (* move) () = nullptr;
    void (* release) () = nullptr;
};

#endif
