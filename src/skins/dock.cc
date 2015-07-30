/*
 * ui_dock.c
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

/*
 * Rough outline
 * =============
 *
 * When moving a window:
 * 1. If this is the main window, find (recursively) the windows docked to it.
 * 2. Adjust the offsets to snap to another window (not docked) or a screen edge.
 * 3. Move the docked windows by the same offsets.
 *
 * When resizing a window:
 * 1. Find (recursively) the windows docked to the bottom edge of this one.
 *    If we are making the window shorter, exclude windows that are docked to
 *    the bottom of another window (not docked) to avoid overlap.
 * 2. Move the docked windows by the difference in window height.
 * 3. Repeat the process for windows docked to the right edge of this one.
 */

#include <stdlib.h>

#include "window.h"

#define SNAP_DISTANCE 10

enum {
    DOCK_TYPE_LEFT = 1 << 0,
    DOCK_TYPE_RIGHT = 1 << 1,
    DOCK_TYPE_TOP = 1 << 2,
    DOCK_TYPE_BOTTOM = 1 << 3
};

#define DOCK_TYPE_ANY (DOCK_TYPE_LEFT | DOCK_TYPE_RIGHT | DOCK_TYPE_TOP | DOCK_TYPE_BOTTOM)

typedef struct {
    Window * window;
    int * x, * y;
    int w, h;
    bool docked;
} DockWindow;

static DockWindow windows[N_WINDOWS];
static int last_x, last_y;

static inline int least_abs (int a, int b)
{
    return (abs (a) < abs (b)) ? a : b;
}

void dock_add_window (int id, Window * window, int * x, int * y, int w, int h)
{
    DockWindow & dw = windows[id];

    dw.window = window;
    dw.x = x;
    dw.y = y;
    dw.w = w;
    dw.h = h;
}

void dock_remove_window (int id)
{
    windows[id].window = nullptr;
}

static void dock_sync ()
{
    for (DockWindow & dw : windows)
    {
        if (dw.window)
            dw.window->getPosition (dw.x, dw.y);
    }
}

static void clear_docked ()
{
    for (DockWindow & dw : windows)
        dw.docked = false;
}

static bool is_docked (DockWindow & dw, DockWindow & base, int type)
{
    return ((type & DOCK_TYPE_LEFT) && * dw.x + dw.w == * base.x) ||
           ((type & DOCK_TYPE_RIGHT) && * dw.x == * base.x + base.w) ||
           ((type & DOCK_TYPE_TOP) && * dw.y + dw.h == * base.y) ||
           ((type & DOCK_TYPE_BOTTOM) && * dw.y == * base.y + base.h);
}

static void find_docked (DockWindow & base, int type)
{
    for (DockWindow & dw : windows)
    {
        if (dw.docked || & dw == & base)
            continue;

        dw.docked = is_docked (dw, base, type);
        if (dw.docked)
            find_docked (dw, type);
    }
}

static void invert_docked ()
{
    for (DockWindow & dw : windows)
        dw.docked = ! dw.docked;
}

void dock_set_size (int id, int w, int h)
{
    DockWindow & base = windows[id];

    dock_sync ();

    if (h != base.h)
    {
        /* 1. Find the windows docked below this one. */

        clear_docked ();
        find_docked (base, DOCK_TYPE_BOTTOM);

        if (h < base.h)
        {
            /* 2. This part is tricky.  By flipping the docked flag on all the
                  windows, we consider the windows not docked to this one as a
                  docked group.  We then add (recursively) any other windows
                  docked to these ones (i.e. the windows docked to these *and*
                  to the one being shortened).  The one being shortened must be
                  excluded from the search.  Since at this point it is marked as
                  docked, it is excluded automatically.  Finally, flipping the
                  docked flag back again leaves us with only the windows docked
                  to the one being shortened and not to any others. */

            invert_docked ();

            for (DockWindow & dw : windows)
            {
                if (! dw.docked || & dw == & base)
                    continue;

                find_docked (dw, DOCK_TYPE_BOTTOM);
            }

            invert_docked ();
        }

        /* 3. Move the docked windows by the difference in height. */

        for (DockWindow & dw : windows)
        {
            if (! dw.docked)
                continue;

            * dw.y += h - base.h;
            if (dw.window)
                dw.window->move (* dw.x, * dw.y);
        }
    }

    if (w != base.w)
    {
        /* 4. Repeat the process for the windows docked to the right. */

        clear_docked ();
        find_docked (base, DOCK_TYPE_RIGHT);

        if (w < base.w)
        {
            invert_docked ();

            for (DockWindow & dw : windows)
            {
                if (! dw.docked || & dw == & base)
                    continue;

                find_docked (dw, DOCK_TYPE_RIGHT);
            }

            invert_docked ();
        }

        for (DockWindow & dw : windows)
        {
            if (! dw.docked)
                continue;

            * dw.x += w - base.w;
            if (dw.window)
                dw.window->move (* dw.x, * dw.y);
        }
    }

    /* 5. Set the window size.  (The actual resize is done by the caller.) */

    base.w = w;
    base.h = h;
}

void dock_move_start (int id, int x, int y)
{
    DockWindow & dw = windows[id];

    dock_sync ();

    last_x = x;
    last_y = y;

    /* 1. If this is the main window, find the windows docked to it. */

    clear_docked ();
    dw.docked = true;
    if (id == WINDOW_MAIN)
        find_docked (dw, DOCK_TYPE_ANY);
}

void dock_move (int x, int y)
{
    int hori, vert;

    if (x == last_x && y == last_y)
        return;

    /* 2. Nominally move all the windows in the group the requested distance,
          and update the reference point. */

    hori = x - last_x;
    vert = y - last_y;

    for (DockWindow & dw : windows)
    {
        if (! dw.docked)
            continue;

        * dw.x += hori;
        * dw.y += vert;
    }

    last_x = x;
    last_y = y;


    /* 3. Find the least snap distance we must move so that
          (a) a window in the group is touching a screen edge or
          (b) a window in the group is touching a window not in the group. */

    hori = SNAP_DISTANCE + 1;
    vert = SNAP_DISTANCE + 1;

    GdkScreen * screen = gdk_screen_get_default ();
    int monitors = gdk_screen_get_n_monitors (screen);

    for (int m = 0; m < monitors; m ++)
    {
        GdkRectangle rect;
        gdk_screen_get_monitor_geometry (screen, m, & rect);

        for (DockWindow & dw : windows)
        {
            if (! dw.docked)
                continue;

            /* We only test half the combinations here, as it is not very
               helpful to have e.g. the right edge of a window touching the left
               edge of a monitor (think about it). */

            hori = least_abs (hori, rect.x - * dw.x);
            hori = least_abs (hori, (rect.x + rect.width) - (* dw.x + dw.w));
            vert = least_abs (vert, rect.y - * dw.y);
            vert = least_abs (vert, (rect.y + rect.height) - (* dw.y + dw.h));
        }
    }

    for (DockWindow & dw : windows)
    {
        if (! dw.docked)
            continue;

        for (DockWindow & dw2 : windows)
        {
            if (dw2.docked)
                continue;

            hori = least_abs (hori, * dw2.x - * dw.x);
            hori = least_abs (hori, * dw2.x - (* dw.x + dw.w));
            hori = least_abs (hori, (* dw2.x + dw2.w) - * dw.x);
            hori = least_abs (hori, (* dw2.x + dw2.w) - (* dw.x + dw.w));
            vert = least_abs (vert, * dw2.y - * dw.y);
            vert = least_abs (vert, * dw2.y - (* dw.y + dw.h));
            vert = least_abs (vert, (* dw2.y + dw2.h) - * dw.y);
            vert = least_abs (vert, (* dw2.y + dw2.h) - (* dw.y + dw.h));
        }
    }

    /* 4. If the snap distances are within range, nominally move all the windows
          in the group, and update the reference point again. */

    if (abs (hori) > SNAP_DISTANCE)
        hori = 0;
    if (abs (vert) > SNAP_DISTANCE)
        vert = 0;

    for (DockWindow & dw : windows)
    {
        if (! dw.docked)
            continue;

        * dw.x += hori;
        * dw.y += vert;
    }

    last_x += hori;
    last_y += vert;

    /* 5. Really move the windows. */

    for (DockWindow & dw : windows)
    {
        if (dw.docked && dw.window)
            dw.window->move (* dw.x, * dw.y);
    }
}

void dock_change_scale (int old_scale, int new_scale)
{
    DockWindow & main = windows[WINDOW_MAIN];

    for (DockWindow & dw : windows)
    {
        dw.w = dw.w * new_scale / old_scale;
        dw.h = dw.h * new_scale / old_scale;

        if (& dw != & main)
        {
            * dw.x = * main.x + (* dw.x - * main.x) * new_scale / old_scale;
            * dw.y = * main.y + (* dw.y - * main.y) * new_scale / old_scale;
        }
    }
}
