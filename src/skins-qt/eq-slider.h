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

#ifndef SKINS_UI_SKINNED_EQUALIZER_SLIDER_H
#define SKINS_UI_SKINNED_EQUALIZER_SLIDER_H

#include <libaudcore/objects.h>

#include "widget.h"

class EqSlider : public Widget
{
public:
    EqSlider (const char * name, int band);
    void set_value (float value);

private:
    void draw (cairo_t * cr);
    bool button_press (GdkEventButton * event);
    bool button_release (GdkEventButton * event);
    bool scroll (GdkEventScroll * event);
    bool motion (GdkEventMotion * event);

    void moved (int pos);

    String m_name;
    int m_band;

    int m_pos = 0;
    float m_value = 0;
    bool m_pressed = false;
};

#endif
