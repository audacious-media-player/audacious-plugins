/*
 * drag-handle.c
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include "drag-handle.h"
#include "skins_cfg.h"

bool DragHandle::button_press (QMouseEvent * event)
{
    if (event->button () != Qt::LeftButton)
        return false;

    m_held = true;
    m_x_origin = event->globalX ();
    m_y_origin = event->globalY ();

    if (press)
        press ();

    return true;
}

bool DragHandle::button_release (QMouseEvent * event)
{
    if (event->button () != Qt::LeftButton)
        return false;

    m_held = false;
    return true;
}

bool DragHandle::motion (QMouseEvent * event)
{
    if (! m_held)
        return true;

    if (drag)
        drag ((event->globalX () - m_x_origin) / config.scale,
         (event->globalY () - m_y_origin) / config.scale);

    return true;
}

DragHandle::DragHandle (int w, int h, void (* press) (), void (* drag) (int x, int y)) :
    press (press), drag (drag)
{
    set_scale (config.scale);
    add_input (w, h, true, false);
}
