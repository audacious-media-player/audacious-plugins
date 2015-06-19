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

#include <libaudcore/runtime.h>

#include "skins_cfg.h"
#include "main.h"
#include "skin.h"
#include "number.h"

void SkinnedNumber::draw (cairo_t * cr)
{
    skin_draw_pixbuf (cr, SKIN_NUMBERS, m_num * 9, 0, 0, 0, 9, 13);
}

bool SkinnedNumber::button_press (GdkEventButton * event)
{
    return change_timer_mode_cb (event);
}

SkinnedNumber::SkinnedNumber ()
{
    set_scale (config.scale);
    add_input (9, 13, false, true);
}

void SkinnedNumber::set (char c)
{
    int value = (c >= '0' && c <= '9') ? c - '0' : (c == '-') ? 11 : 10;

    if (m_num != value)
    {
        m_num = value;
        queue_draw ();
    }
}
