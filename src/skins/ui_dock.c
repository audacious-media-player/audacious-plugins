/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team
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

#include "ui_dock.h"
#include "skins_cfg.h"
#include <gdk/gdk.h>
#include <stdlib.h>
#include <audacious/plugin.h>
#include "ui_skinned_window.h"

#include "platform/smartinclude.h"
#include "util.h"

#define SNAP_DISTANCE 10

static GList *dock_window_list = NULL;

struct _DockedWindow {
    GtkWindow *w;
    gint offset_x, offset_y;
};

typedef struct _DockedWindow DockedWindow;


static gint
docked_list_compare(DockedWindow * a, DockedWindow * b)
{
    if (a->w == b->w)
        return 0;
    return 1;
}

static void
snap_edge(gint * x, gint * y, gint w, gint h, gint bx, gint by,
          gint bw, gint bh)
{
    gint sd = SNAP_DISTANCE;

    if ((*x + w > bx - sd) && (*x + w < bx + sd) &&
        (*y > by - h - sd) && (*y < by + bh + sd)) {
        *x = bx - w;
        if ((*y > by - sd) && (*y < by + sd))
            *y = by;
        if ((*y + h > by + bh - sd) && (*y + h < by + bh + sd))
            *y = by + bh - h;
    }
    if ((*x > bx + bw - sd) && (*x < bx + bw + sd) &&
        (*y > by - h - sd) && (*y < by + bh + sd)) {
        *x = bx + bw;
        if ((*y > by - sd) && (*y < by + sd))
            *y = by;
        if ((*y + h > by + bh - sd) && (*y + h < by + bh + sd))
            *y = by + bh - h;
    }
}

static void
snap(gint * x, gint * y, gint w, gint h, gint bx, gint by, gint bw, gint bh)
{
    snap_edge(x, y, w, h, bx, by, bw, bh);
    snap_edge(y, x, h, w, by, bx, bh, bw);
}

static void
calc_snap_offset(GList * dlist, GList * wlist, gint x, gint y,
                 gint * off_x, gint * off_y)
{
    gint nx, ny, nw, nh, sx, sy, sw, sh;
    GtkWindow *w;
    GList *dnode, *wnode;
    DockedWindow temp, *dw;

    *off_x = 0;
    *off_y = 0;

    /*
     * FIXME: Why not break out of the loop when we find someting
     * to snap to?
     */
    for (dnode = dlist; dnode; dnode = g_list_next(dnode)) {
        dw = dnode->data;
        gtk_window_get_size(dw->w, &nw, &nh);

        nx = dw->offset_x + *off_x + x;
        ny = dw->offset_y + *off_y + y;

        /* Snap to screen edges */
        if (abs(nx) < SNAP_DISTANCE)
            *off_x -= nx;
        if (abs(ny) < SNAP_DISTANCE)
            *off_y -= ny;
        if (abs(nx + nw - gdk_screen_width()) < SNAP_DISTANCE)
            *off_x -= nx + nw - gdk_screen_width();
        if (abs(ny + nh - gdk_screen_height()) < SNAP_DISTANCE)
            *off_y -= ny + nh - gdk_screen_height();

        /* Snap to other windows */
        for (wnode = wlist; wnode; wnode = g_list_next(wnode)) {
            temp.w = wnode->data;
            if (g_list_find_custom
                (dlist, &temp, (GCompareFunc) docked_list_compare))
                /* These windows are already docked */
                continue;

            w = GTK_WINDOW(wnode->data);
            gtk_window_get_position(w, &sx, &sy);
            gtk_window_get_size(w, &sw, &sh);

            nx = dw->offset_x + *off_x + x;
            ny = dw->offset_y + *off_y + y;

            snap(&nx, &ny, nw, nh, sx, sy, sw, sh);

            *off_x += nx - (dw->offset_x + *off_x + x);
            *off_y += ny - (dw->offset_y + *off_y + y);
        }
    }
}


static gboolean
is_docked(gint a_x, gint a_y, gint a_w, gint a_h,
          gint b_x, gint b_y, gint b_w, gint b_h)
{
    if (((a_x == b_x + b_w) || (a_x + a_w == b_x)) &&
        (b_y + b_h >= a_y) && (b_y <= a_y + a_h))
        return TRUE;

    if (((a_y == b_y + b_h) || (a_y + a_h == b_y)) &&
        (b_x >= a_x - b_w) && (b_x <= a_x + a_w))
        return TRUE;

    return FALSE;
}

/*
 * Builds a list of all windows that are docked to the window "w".
 * Recursively adds all windows that are docked to the windows that are
 * docked to "w" and so on...
 * FIXME: init_off_?  ?
 */

static GList *
get_docked_list(GList * dlist, GList * wlist, GtkWindow * w,
                gint init_off_x, gint init_off_y)
{
    GList *node;
    DockedWindow *dwin, temp;
    gint w_x, w_y, w_width, w_height;
    gint t_x, t_y, t_width, t_height;


    gtk_window_get_position(w, &w_x, &w_y);
    gtk_window_get_size(w, &w_width, &w_height);
    if (!dlist) {
        dwin = g_new0(DockedWindow, 1);
        dwin->w = w;
        dlist = g_list_append(dlist, dwin);
    }

    for (node = wlist; node; node = g_list_next(node)) {
        temp.w = node->data;
        if (g_list_find_custom
            (dlist, &temp, (GCompareFunc) docked_list_compare))
            continue;

        gtk_window_get_position(GTK_WINDOW(node->data), &t_x, &t_y);
        gtk_window_get_size(GTK_WINDOW(node->data), &t_width, &t_height);
        if (is_docked
            (w_x, w_y, w_width, w_height, t_x, t_y, t_width, t_height)) {
            dwin = g_new0(DockedWindow, 1);
            dwin->w = node->data;

            dwin->offset_x = t_x - w_x + init_off_x;
            dwin->offset_y = t_y - w_y + init_off_y;

            dlist = g_list_append(dlist, dwin);

            dlist =
                get_docked_list(dlist, wlist, dwin->w, dwin->offset_x,
                                dwin->offset_y);
        }
    }
    return dlist;
}

static void
free_docked_list(GList * dlist)
{
    GList *node;

    for (node = dlist; node; node = g_list_next(node))
        g_free(node->data);
    g_list_free(dlist);
}

static void move_skinned_window (SkinnedWindow * window, int x, int y)
{
    gtk_window_move (GTK_WINDOW (window), x, y);

    *(window->x) = x;
    *(window->y) = y;
}

static void
docked_list_move(GList * list, gint x, gint y)
{
    GList *node;
    DockedWindow *dw;

    for (node = list; node; node = g_list_next(node)) {
        dw = node->data;
        move_skinned_window (SKINNED_WINDOW (dw->w), x + dw->offset_x,
         y + dw->offset_y);
    }
}

static void move_attached (GtkWindow * window, GList * * others, int offset) {
  int x, y, width, height, x2, y2;
  GList * move, * scan, * next;
   gtk_window_get_position (window, & x, & y);
   gtk_window_get_size (window, & width, & height);
   move = 0;
   for (scan = * others; scan; scan = next) {
      next = g_list_next (scan);
      gtk_window_get_position (scan->data, & x2, & y2);
      if (y2 == y + height) {
         * others = g_list_remove_link (* others, scan);
         move = g_list_concat (move, scan);
      }
   }
   for (; move; move = g_list_delete_link (move, move))
      move_attached (move->data, others, offset);
   move_skinned_window (SKINNED_WINDOW (window), x, y + offset);
}

void dock_shade(GList *window_list, GtkWindow *window, gint new_height)
{
    gint x, y, width, height, x2, y2;
    GList *move, *others, *scan, *next;

    gtk_window_get_size(window, &width, &height);

   if (! config.show_wm_decorations) {
      gtk_window_get_position (window, & x, & y);
      others = g_list_copy (window_list);
      others = g_list_remove (others, window);
      move = 0;
      for (scan = others; scan; scan = next) {
         next = g_list_next (scan);
         gtk_window_get_position (scan->data, & x2, & y2);
         if (y2 == y + height) {
            others = g_list_remove_link (others, scan);
            move = g_list_concat (move, scan);
         }
      }
      for (; move; move = g_list_delete_link (move, move))
         move_attached (move->data, & others, new_height - height);
      g_list_free (others);
   }

    resize_window((GtkWidget *) window, width, new_height);
}

void
dock_move_press(GList * window_list, GtkWindow * w,
                GdkEventButton * event, gboolean move_list)
{
    gint mx, my;
    DockedWindow *dwin;

    if (config.show_wm_decorations)
        return;

    gtk_window_present(w);
    mx = event->x;
    my = event->y;
    g_object_set_data(G_OBJECT(w), "move_offset_x", GINT_TO_POINTER(mx));
    g_object_set_data(G_OBJECT(w), "move_offset_y", GINT_TO_POINTER(my));
    if (move_list)
        g_object_set_data(G_OBJECT(w), "docked_list",
                          get_docked_list(NULL, window_list, w, 0, 0));
    else {
        dwin = g_new0(DockedWindow, 1);
        dwin->w = w;
        g_object_set_data(G_OBJECT(w), "docked_list",
                          g_list_append(NULL, dwin));
    }
    g_object_set_data(G_OBJECT(w), "window_list", window_list);
    g_object_set_data(G_OBJECT(w), "is_moving", GINT_TO_POINTER(1));
}

void
dock_move_motion(GtkWindow * w, GdkEventMotion * event)
{
    gint offset_x, offset_y, x, y;
    GList *dlist;
    GList *window_list;

    if (!g_object_get_data(G_OBJECT(w), "is_moving"))
        return;

    offset_x =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "move_offset_x"));
    offset_y =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "move_offset_y"));
    dlist = g_object_get_data(G_OBJECT(w), "docked_list");
    window_list = g_object_get_data(G_OBJECT(w), "window_list");

    x = event->x_root - offset_x;
    y = event->y_root - offset_y;

    calc_snap_offset(dlist, window_list, x, y, &offset_x, &offset_y);
    x += offset_x;
    y += offset_y;

    docked_list_move(dlist, x, y);
}

void
dock_move_release(GtkWindow * w)
{
    GList *dlist;
    g_object_set_data(G_OBJECT(w), "is_moving", NULL);
    g_object_set_data(G_OBJECT(w), "move_offset_x", NULL);
    g_object_set_data(G_OBJECT(w), "move_offset_y", NULL);
    if ((dlist = g_object_get_data(G_OBJECT(w), "docked_list")) != NULL)
        free_docked_list(dlist);
    g_object_set_data(G_OBJECT(w), "docked_list", NULL);
    g_object_set_data(G_OBJECT(w), "window_list", NULL);
}

gboolean
dock_is_moving(GtkWindow * w)
{
    if (g_object_get_data(G_OBJECT(w), "is_moving"))
        return TRUE;
    return FALSE;
}

void dock_window_set_decorated (GtkWidget * widget)
{
    if (config.show_wm_decorations)
        dock_window_list = g_list_remove (dock_window_list, widget);
    else
        dock_window_list = g_list_append (dock_window_list, widget);

    gtk_window_set_decorated (GTK_WINDOW (widget), config.show_wm_decorations);
}

GList *
get_dock_window_list() {
    return dock_window_list;
}

void clear_dock_window_list (void)
{
    g_list_free (dock_window_list);
    dock_window_list = NULL;
}
