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
#include "skin.h"
#include "menurow.h"

void MenuRow::draw (QPainter & cr)
{
    if (m_selected == MENUROW_NONE)
    {
        if (m_pushed)
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 304, 0, 0, 0, 8, 43);
        else
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 312, 0, 0, 0, 8, 43);
    }
    else
        skin_draw_pixbuf (cr, SKIN_TITLEBAR, 304 + 8 * (m_selected - 1), 44, 0, 0, 8, 43);

    if (m_pushed)
    {
        if (aud_get_bool ("skins", "always_on_top"))
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 312, 54, 0, 10, 8, 8);
        if (aud_get_bool ("skins", "double_size"))
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 328, 70, 0, 26, 8, 8);
    }
}

static MenuRowItem menurow_find_selected (int x, int y)
{
    if (x >= 0 && x < 8)
    {
        if (y >= 0 && y < 10)
            return MENUROW_OPTIONS;
        if (y >= 10 && y < 18)
            return MENUROW_ALWAYS;
        if (y >= 18 && y < 26)
            return MENUROW_FILEINFOBOX;
        if (y >= 26 && y < 34)
            return MENUROW_SCALE;
        if (y >= 34 && y < 43)
            return MENUROW_VISUALIZATION;
    }

    return MENUROW_NONE;
}

bool MenuRow::button_press (QMouseEvent * event)
{
    if (event->button () != Qt::LeftButton)
        return false;

    m_pushed = true;
    m_selected = menurow_find_selected (event->x () / config.scale, event->y () / config.scale);

    mainwin_mr_change (m_selected);

    queue_draw ();
    return true;
}

bool MenuRow::button_release (QMouseEvent * event)
{
    if (event->button () != Qt::LeftButton)
        return false;

    if (! m_pushed)
        return true;

    mainwin_mr_release (m_selected, event);

    m_pushed = false;
    m_selected = MENUROW_NONE;

    queue_draw ();
    return true;
}

bool MenuRow::motion (QMouseEvent * event)
{
    if (! m_pushed)
        return true;

    m_selected = menurow_find_selected (event->x () / config.scale, event->y () / config.scale);

    mainwin_mr_change (m_selected);

    queue_draw ();
    return true;
}

MenuRow::MenuRow ()
{
    set_scale (config.scale);
    add_input (8, 43, true, true);
}
