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

#include <libaudcore/objects.h>
#include <libaudcore/playlist.h>

#include "skins_cfg.h"
#include "skin.h"
#include "playlist-widget.h"
#include "playlist-slider.h"

void PlaylistSlider::draw (QPainter & cr)
{
    int rows, first;
    m_list->row_info (& rows, & first);

    int range = m_length - rows;

    int y;
    if (m_length > rows)
        y = (first * (m_height - 19) + range / 2) / range;
    else
        y = 0;

    for (int i = 0; i < m_height / 29; i ++)
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 36, 42, 0, 29 * i, 8, 29);

    skin_draw_pixbuf (cr, SKIN_PLEDIT, m_pressed ? 61 : 52, 53, 0, y, 8, 18);
}

void PlaylistSlider::set_pos (int y)
{
    y = aud::clamp (y, 0, m_height - 19);

    int rows, first;
    m_list->row_info (& rows, & first);

    int range = m_height - 19;
    m_list->scroll_to ((y * (m_length - rows) + range / 2) / range);
}

bool PlaylistSlider::button_press (QMouseEvent * event)
{
    if (event->button () != Qt::LeftButton)
        return false;

    m_pressed = true;
    set_pos (event->y () / config.scale - 9);

    queue_draw ();
    return true;
}

bool PlaylistSlider::button_release (QMouseEvent * event)
{
    if (event->button () != Qt::LeftButton)
        return false;

    if (! m_pressed)
        return true;

    m_pressed = false;
    set_pos (event->y () / config.scale - 9);

    queue_draw ();
    return true;
}

bool PlaylistSlider::motion (QMouseEvent * event)
{
    if (! m_pressed)
        return true;

    set_pos (event->y () / config.scale - 9);

    queue_draw ();
    return true;
}

PlaylistSlider::PlaylistSlider (PlaylistWidget * list, int height) :
    m_list (list), m_height (height),
    m_length (Playlist::active_playlist ().n_entries ())
{
    set_scale (config.scale);
    add_input (8, height, true, true);
}

void PlaylistSlider::resize (int height)
{
    m_height = height;
    Widget::resize (8, height);
    queue_draw ();
}

void PlaylistSlider::refresh ()
{
    m_length = Playlist::active_playlist ().n_entries ();
    queue_draw ();
}
