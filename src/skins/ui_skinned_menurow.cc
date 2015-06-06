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

#include "drawing.h"
#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_skinned_menurow.h"

static struct {
    MenuRowItem selected;
    gboolean pushed;
} mr;

DRAW_FUNC_BEGIN (menurow_draw, void)
    if (mr.selected == MENUROW_NONE)
    {
        if (mr.pushed)
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 304, 0, 0, 0, 8, 43);
        else
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 312, 0, 0, 0, 8, 43);
    }
    else
        skin_draw_pixbuf (cr, SKIN_TITLEBAR, 304 + 8 * (mr.selected - 1), 44, 0,
         0, 8, 43);

    if (mr.pushed)
    {
        if (aud_get_bool ("skins", "always_on_top"))
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 312, 54, 0, 10, 8, 8);
        if (aud_get_bool ("skins", "double_size"))
            skin_draw_pixbuf (cr, SKIN_TITLEBAR, 328, 70, 0, 26, 8, 8);
    }
DRAW_FUNC_END

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

static gboolean menurow_button_press (GtkWidget * widget, GdkEventButton * event)
{
    if (event->button != 1)
        return FALSE;

    mr.pushed = TRUE;
    mr.selected = menurow_find_selected (event->x / config.scale, event->y / config.scale);

    mainwin_mr_change (mr.selected);

    gtk_widget_queue_draw (widget);
    return TRUE;
}

static gboolean menurow_button_release (GtkWidget * widget, GdkEventButton *
 event)
{
    if (event->button != 1)
        return FALSE;

    if (! mr.pushed)
        return TRUE;

    mainwin_mr_release (mr.selected, event);

    mr.pushed = FALSE;
    mr.selected = MENUROW_NONE;

    gtk_widget_queue_draw (widget);
    return TRUE;
}

static gboolean menurow_motion_notify (GtkWidget * widget, GdkEventMotion *
 event)
{
    if (! mr.pushed)
        return TRUE;

    mr.selected = menurow_find_selected (event->x / config.scale, event->y / config.scale);

    mainwin_mr_change (mr.selected);

    gtk_widget_queue_draw (widget);
    return TRUE;
}

GtkWidget * ui_skinned_menurow_new (void)
{
    GtkWidget * wid = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) wid, false);
    gtk_widget_set_size_request (wid, 8 * config.scale, 43 * config.scale);
    gtk_widget_add_events (wid, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
     | GDK_POINTER_MOTION_MASK);

    DRAW_CONNECT_PROXY (wid, menurow_draw, nullptr);
    g_signal_connect (wid, "button-press-event", (GCallback) menurow_button_press, nullptr);
    g_signal_connect (wid, "button-release-event", (GCallback) menurow_button_release, nullptr);
    g_signal_connect (wid, "motion-notify-event", (GCallback) menurow_motion_notify, nullptr);

    return wid;
}

void ui_skinned_menurow_update (GtkWidget * menurow)
{
    gtk_widget_queue_draw (menurow);
}
