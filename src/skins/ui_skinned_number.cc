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

#include "draw-compat.h"
#include "ui_skin.h"
#include "ui_skinned_number.h"

typedef struct {
    gint w, h;
    gint num;
} NumberData;

DRAW_FUNC_BEGIN (number_draw)
    NumberData * data = (NumberData *) g_object_get_data ((GObject *) wid, "numberdata");
    g_return_val_if_fail (data, FALSE);

    skin_draw_pixbuf (cr, SKIN_NUMBERS, data->num * 9, 0, 0, 0, data->w, data->h);
DRAW_FUNC_END

static void number_destroy (GtkWidget * number)
{
    g_free (g_object_get_data ((GObject *) number, "numberdata"));
}

GtkWidget * ui_skinned_number_new (void)
{
    GtkWidget * number = gtk_drawing_area_new ();
    gtk_widget_set_size_request (number, 9, 13);

    gtk_widget_add_events (number, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK);

    DRAW_CONNECT (number, number_draw);
    g_signal_connect (number, "destroy", (GCallback) number_destroy, NULL);

    NumberData * data = g_new0 (NumberData, 1);
    data->w = 9;
    data->h = 13;
    g_object_set_data ((GObject *) number, "numberdata", data);

    return number;
}

void ui_skinned_number_set (GtkWidget * number, gchar c)
{
    NumberData * data = (NumberData *) g_object_get_data ((GObject *) number, "numberdata");
    g_return_if_fail (data);

    gint value = (c >= '0' && c <= '9') ? c - '0' : (c == '-') ? 11 : 10;

    if (data->num == value)
        return;

    data->num = value;
    gtk_widget_queue_draw (number);
}

void ui_skinned_number_set_size (GtkWidget * number, gint width, gint height)
{
    NumberData * data = (NumberData *) g_object_get_data ((GObject *) number, "numberdata");
    g_return_if_fail (data);

    data->w = width;
    data->h = height;

    gtk_widget_set_size_request (number, width, height);
    gtk_widget_queue_draw (number);
}
