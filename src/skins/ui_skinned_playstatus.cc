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

#include "drawing.h"
#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_skinned_playstatus.h"

void PlayStatus::draw (cairo_t * cr)
{
    if (m_status == STATUS_PLAY)
        skin_draw_pixbuf (cr, SKIN_PLAYPAUSE, 36, 0, 0, 0, 3, 9);
    else
        skin_draw_pixbuf (cr, SKIN_PLAYPAUSE, 27, 0, 0, 0, 2, 9);

    switch (m_status)
    {
    case STATUS_STOP:
        skin_draw_pixbuf (cr, SKIN_PLAYPAUSE, 18, 0, 2, 0, 9, 9);
        break;
    case STATUS_PAUSE:
        skin_draw_pixbuf (cr, SKIN_PLAYPAUSE, 9, 0, 2, 0, 9, 9);
        break;
    case STATUS_PLAY:
        skin_draw_pixbuf (cr, SKIN_PLAYPAUSE, 1, 0, 3, 0, 8, 9);
        break;
    }
}

PlayStatus::PlayStatus ()
{
    GtkWidget * playstatus = drawing_area_new ();
    gtk_widget_set_size_request (playstatus, 11 * config.scale, 9 * config.scale);
    set_gtk (playstatus);
}

void PlayStatus::set_status (PStatus status)
{
    m_status = status;
    gtk_widget_queue_draw (gtk ());
}
