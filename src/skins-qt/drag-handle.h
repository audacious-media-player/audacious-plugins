/*
 * drag-handle.h
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

#ifndef SKINS_DRAG_HANDLE_H
#define SKINS_DRAG_HANDLE_H

#include "widget.h"

class DragHandle : public Widget
{
public:
    DragHandle (int w, int h, void (* press) (), void (* drag) (int x, int y));

private:
    bool button_press (GdkEventButton * event);
    bool button_release (GdkEventButton * event);
    bool motion (GdkEventMotion * event);

    void (* press) ();
    void (* drag) (int x_offset, int y_offset);

    bool m_held = false;
    int m_x_origin = 0, m_y_origin = 0;
};

#endif
