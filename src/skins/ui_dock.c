/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

/*
 * NOT MUCH HERE RIGHT NOW, REWRITE IN PROGRESS ...
 *
 * Coarse outline
 * ==============
 *
 * When moving a window:
 * 1. If this is the main window, find the windows docked to it.
 * 2. Adjust the offsets to snap to another window (not docked) or a screen edge.
 * 3. If this is the main window, move docked windows by the same offsets.
 *
 * When resizing a window:
 * 1. Find the windows docked to the bottom edge.
 * 2. Repeat the last step recursively for each window found.
 * 3. Move these windows by the difference in window height.
 * 4. Repeat the last three steps for the right edge.
 */

#include "ui_dock.h"

#define SNAP_DISTANCE 10

typedef struct {
    GtkWidget * window;
    gint * x, * y;
    gint w, h;
    gboolean main;
} DockWindow;

static GSList * windows;
static DockWindow * moving;
static gint origin_x, origin_y;

static DockWindow * find_window (GSList * list, GtkWidget * window)
{
    for (GSList * node = list; node; node = node->next)
    {
        DockWindow * dw = node->data;
        if (dw->window == window)
            return dw;
    }

    return NULL;
}

void dock_add_window (GtkWidget * window, gint * x, gint * y, gint w, gint h,
 gboolean main)
{
    DockWindow * dw = g_slice_new (DockWindow);
    dw->window = window;
    dw->x = x;
    dw->y = y;
    dw->w = w;
    dw->h = h;
    dw->main = main;

    windows = g_slist_prepend (windows, dw);
}

void dock_remove_window (GtkWidget * window)
{
    DockWindow * dw = find_window (windows, window);
    g_return_if_fail (dw);

    windows = g_slist_remove (windows, dw);
    g_slice_free (DockWindow, dw);
}

void dock_set_size (GtkWidget * window, gint w, gint h)
{
    DockWindow * dw = find_window (windows, window);
    g_return_if_fail (dw);

    dw->w = w;
    dw->h = h;
}

void dock_move_start (GtkWidget * window, gint x, gint y)
{
    DockWindow * dw = find_window (windows, window);
    g_return_if_fail (dw);

    moving = dw;
    origin_x = x;
    origin_y = y;
}

void dock_move (gint x, gint y)
{
    g_return_if_fail (moving);

    if (x == origin_x && y == origin_y)
        return;

    * moving->x += x - origin_x;
    * moving->y += y - origin_y;

    gtk_window_move ((GtkWindow *) moving->window, * moving->x, * moving->y);
}

void dock_move_end (void)
{
    moving = NULL;
}
