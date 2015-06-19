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

#include "skins_cfg.h"
#include "skin.h"
#include "monostereo.h"

void MonoStereo::draw (QPainter & cr)
{
    switch (m_num_channels)
    {
    case 0:
        skin_draw_pixbuf (cr, SKIN_MONOSTEREO, 29, 12, 0, 0, 27, 12);
        skin_draw_pixbuf (cr, SKIN_MONOSTEREO, 0, 12, 27, 0, 29, 12);
        break;
    case 1:
        skin_draw_pixbuf (cr, SKIN_MONOSTEREO, 29, 0, 0, 0, 27, 12);
        skin_draw_pixbuf (cr, SKIN_MONOSTEREO, 0, 12, 27, 0, 29, 12);
        break;
    default:
        skin_draw_pixbuf (cr, SKIN_MONOSTEREO, 29, 12, 0, 0, 27, 12);
        skin_draw_pixbuf (cr, SKIN_MONOSTEREO, 0, 0, 27, 0, 29, 12);
        break;
    }
}

MonoStereo::MonoStereo ()
{
    set_scale (config.scale);
    add_drawable (56, 12);
}

void MonoStereo::set_num_channels (int num_channels)
{
    m_num_channels = num_channels;
    queue_draw ();
}
