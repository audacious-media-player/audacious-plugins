/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
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

#ifndef SKINS_UI_SKINNED_BUTTON_H
#define SKINS_UI_SKINNED_BUTTON_H

#include "widget.h"
#include "skin.h"

class Button;

typedef void (* ButtonCB) (Button * button, QMouseEvent * event);

class Button : public Widget
{
public:
    // transparent button
    Button (int w, int h) :
        Button (Small, w, h, 0, 0, 0, 0, 0, 0, 0, 0, SKIN_MAIN, SKIN_MAIN) {}

    // basic skinned button
    Button (int w, int h, int nx, int ny, int px, int py, SkinPixmapId si1, SkinPixmapId si2) :
        Button (Normal, w, h, nx, ny, px, py, 0, 0, 0, 0, si1, si2) {}

    // skinned toggle button
    Button (int w, int h, int nx, int ny, int px, int py, int pnx, int pny,
     int ppx, int ppy, SkinPixmapId si1, SkinPixmapId si2) :
        Button (Toggle, w, h, nx, ny, px, py, pnx, pny, ppx, ppy, si1, si2) {}

    void on_press (ButtonCB callback) { press = callback; }
    void on_release (ButtonCB callback) { release = callback; }
    void on_rpress (ButtonCB callback) { rpress = callback; }
    void on_rrelease (ButtonCB callback) { rrelease = callback; }

    bool get_active () { return m_active; }
    void set_active (bool active);

private:
    enum Type {Normal, Toggle, Small};

    Button (Type type, int w, int h, int nx, int ny, int px, int py, int pnx,
     int pny, int ppx, int ppy, SkinPixmapId si1, SkinPixmapId si2);

    void draw (QPainter & cr);
    bool button_press (QMouseEvent * event);
    bool button_release (QMouseEvent * event);

    Type m_type;
    int m_w, m_h;
    int m_nx, m_ny, m_px, m_py;
    int m_pnx, m_pny, m_ppx, m_ppy;
    SkinPixmapId m_si1, m_si2;

    bool m_pressed = false, m_rpressed = false, m_active = false;

    ButtonCB press = nullptr, release = nullptr;
    ButtonCB rpress = nullptr, rrelease = nullptr;
};

#endif
