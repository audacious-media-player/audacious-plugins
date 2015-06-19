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

#include "skins_cfg.h"
#include "button.h"

void Button::draw (QPainter & cr)
{
    switch (m_type)
    {
    case Normal:
        if (m_pressed)
            skin_draw_pixbuf (cr, m_si2, m_px, m_py, 0, 0, m_w, m_h);
        else
            skin_draw_pixbuf (cr, m_si1, m_nx, m_ny, 0, 0, m_w, m_h);
        break;
    case Toggle:
        if (m_active)
        {
            if (m_pressed)
                skin_draw_pixbuf (cr, m_si2, m_ppx, m_ppy, 0, 0, m_w, m_h);
            else
                skin_draw_pixbuf (cr, m_si1, m_pnx, m_pny, 0, 0, m_w, m_h);
        }
        else
        {
            if (m_pressed)
                skin_draw_pixbuf (cr, m_si2, m_px, m_py, 0, 0, m_w, m_h);
            else
                skin_draw_pixbuf (cr, m_si1, m_nx, m_ny, 0, 0, m_w, m_h);
        }
        break;
    case Small:
        break;
    }
}

bool Button::button_press (QMouseEvent * event)
{
    /* pass events through to the parent widget only if neither the press nor
     * release signals are connected; sending one and not the other causes
     * problems (in particular with dragging windows around) */
    if (event->button () == Qt::LeftButton && (press || release))
    {
        m_pressed = true;
        if (press)
            press (this, event);
    }
    else if (event->button () == Qt::RightButton && (rpress || rrelease))
    {
        m_rpressed = true;
        if (rpress)
            rpress (this, event);
    }
    else
        return false;

    if (m_type != Small)
        queue_draw ();

    return true;
}

bool Button::button_release (QMouseEvent * event)
{
    if (event->button () == Qt::LeftButton && (press || release))
    {
        if (! m_pressed)
            return true;

        m_pressed = false;
        if (m_type == Toggle)
            m_active = ! m_active;
        if (release)
            release (this, event);
    }
    else if (event->button () == Qt::RightButton && (rpress || rrelease))
    {
        if (! m_rpressed)
            return true;

        m_rpressed = false;
        if (rrelease)
            rrelease (this, event);
    }
    else
        return false;

    if (m_type != Small)
        queue_draw ();

    return true;
}

Button::Button (Type type, int w, int h, int nx, int ny, int px, int py,
 int pnx, int pny, int ppx, int ppy, SkinPixmapId si1, SkinPixmapId si2) :
    m_type (type),
    m_w (w), m_h (h),
    m_nx (nx), m_ny (ny), m_px (px), m_py (py),
    m_pnx (pnx), m_pny (pny), m_ppx (ppx), m_ppy (ppy),
    m_si1 (si1), m_si2 (si2)
{
    set_scale (config.scale);
    add_input (w, h, false, type != Small);
}

void Button::set_active (bool active)
{
    if (m_active != active)
    {
        m_active = active;
        queue_draw ();
    }
}
