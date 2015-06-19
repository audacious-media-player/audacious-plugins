/*
 * widget.cc
 * Copyright 2015 John Lindgren
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

#include "widget.h"

void Widget::add_input (int width, int height, bool track_motion, bool drawable)
{
    resize (width, height);
    setMouseTracking (track_motion);
    m_drawable = drawable;
}

void Widget::add_drawable (int width, int height)
{
    resize (width, height);
    m_drawable = true;
}

void Widget::paintEvent (QPaintEvent *)
{
    if (m_drawable)
    {
        QPainter p (this);
        if (m_scale != 1)
            p.setTransform (QTransform ().scale (m_scale, m_scale));

        draw (p);
    }
}
