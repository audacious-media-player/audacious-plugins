/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
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
#include "skins_cfg.h"
#include "ui_skinned_button.h"

enum {BUTTON_TYPE_NORMAL, BUTTON_TYPE_TOGGLE, BUTTON_TYPE_SMALL};

typedef struct {
    int type;
    int w, h;
    int nx, ny, px, py;
    int pnx, pny, ppx, ppy;
    SkinPixmapId si1, si2;
    gboolean pressed, rpressed, active;
    ButtonCB on_press, on_release, on_rpress, on_rrelease;
} ButtonData;

DRAW_FUNC_BEGIN (button_draw)
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) wid, "buttondata");
    g_return_val_if_fail (data, FALSE);

    switch (data->type)
    {
    case BUTTON_TYPE_NORMAL:
        if (data->pressed)
            skin_draw_pixbuf (cr, data->si2, data->px, data->py, 0, 0, data->w, data->h);
        else
            skin_draw_pixbuf (cr, data->si1, data->nx, data->ny, 0, 0, data->w, data->h);
        break;
    case BUTTON_TYPE_TOGGLE:
        if (data->active)
        {
            if (data->pressed)
                skin_draw_pixbuf (cr, data->si2, data->ppx, data->ppy, 0, 0, data->w, data->h);
            else
                skin_draw_pixbuf (cr, data->si1, data->pnx, data->pny, 0, 0, data->w, data->h);
        }
        else
        {
            if (data->pressed)
                skin_draw_pixbuf (cr, data->si2, data->px, data->py, 0, 0, data->w, data->h);
            else
                skin_draw_pixbuf (cr, data->si1, data->nx, data->ny, 0, 0, data->w, data->h);
        }
        break;
    }
DRAW_FUNC_END

static gboolean button_press (GtkWidget * button, GdkEventButton * event)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_val_if_fail (data, FALSE);

    /* pass events through to the parent widget only if neither the press nor
     * release signals are connected; sending one and not the other causes
     * problems (in particular with dragging windows around) */
    if (event->button == 1 && (data->on_press || data->on_release))
    {
        data->pressed = TRUE;
        if (data->on_press)
            data->on_press (button, event);
    }
    else if (event->button == 3 && (data->on_rpress || data->on_rrelease))
    {
        data->rpressed = TRUE;
        if (data->on_rpress)
            data->on_rpress (button, event);
    }
    else
        return FALSE;

    if (data->type != BUTTON_TYPE_SMALL)
        gtk_widget_queue_draw (button);

    return TRUE;
}

static gboolean button_release (GtkWidget * button, GdkEventButton * event)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_val_if_fail (data, FALSE);

    if (event->button == 1 && (data->on_press || data->on_release))
    {
        if (! data->pressed)
            return TRUE;

        data->pressed = FALSE;
        if (data->type == BUTTON_TYPE_TOGGLE)
            data->active = ! data->active;
        if (data->on_release)
            data->on_release (button, event);
    }
    else if (event->button == 3 && (data->on_rpress || data->on_rrelease))
    {
        if (! data->rpressed)
            return TRUE;

        data->rpressed = FALSE;
        if (data->on_rrelease)
            data->on_rrelease (button, event);
    }
    else
        return FALSE;

    if (data->type != BUTTON_TYPE_SMALL)
        gtk_widget_queue_draw (button);

    return TRUE;
}

static void button_destroy (GtkWidget * button)
{
    g_free (g_object_get_data ((GObject *) button, "buttondata"));
}

static GtkWidget * button_new_base (int type, int w, int h)
{
    GtkWidget * button;

    if (type == BUTTON_TYPE_SMALL)
    {
        button = gtk_event_box_new ();
        gtk_event_box_set_visible_window ((GtkEventBox *) button, FALSE);
    }
    else
        button = gtk_drawing_area_new ();

    gtk_widget_set_size_request (button, w * config.scale, h * config.scale);
    gtk_widget_add_events (button, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK);

    if (type != BUTTON_TYPE_SMALL)
        DRAW_CONNECT (button, button_draw);

    g_signal_connect (button, "button-press-event", (GCallback) button_press,
     nullptr);
    g_signal_connect (button, "button-release-event", (GCallback)
     button_release, nullptr);
    g_signal_connect (button, "destroy", (GCallback) button_destroy, nullptr);

    ButtonData * data = g_new0 (ButtonData, 1);
    data->type = type;
    data->w = w;
    data->h = h;
    g_object_set_data ((GObject *) button, "buttondata", data);

    return button;
}

GtkWidget * button_new (int w, int h, int nx, int ny, int px, int py,
 SkinPixmapId si1, SkinPixmapId si2)
{
    GtkWidget * button = button_new_base (BUTTON_TYPE_NORMAL, w, h);
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_val_if_fail (data, nullptr);

    data->nx = nx;
    data->ny = ny;
    data->px = px;
    data->py = py;
    data->si1 = si1;
    data->si2 = si2;

    return button;
}

GtkWidget * button_new_toggle (int w, int h, int nx, int ny, int px, int
 py, int pnx, int pny, int ppx, int ppy, SkinPixmapId si1, SkinPixmapId si2)
{
    GtkWidget * button = button_new_base (BUTTON_TYPE_TOGGLE, w, h);
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_val_if_fail (data, nullptr);

    data->nx = nx;
    data->ny = ny;
    data->px = px;
    data->py = py;
    data->pnx = pnx;
    data->pny = pny;
    data->ppx = ppx;
    data->ppy = ppy;
    data->si1 = si1;
    data->si2 = si2;

    return button;
}

GtkWidget * button_new_small (int w, int h)
{
    return button_new_base (BUTTON_TYPE_SMALL, w, h);
}

void button_on_press (GtkWidget * button, ButtonCB callback)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_if_fail (data);

    data->on_press = callback;
}

void button_on_release (GtkWidget * button, ButtonCB callback)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_if_fail (data);

    data->on_release = callback;
}

void button_on_rpress (GtkWidget * button, ButtonCB callback)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_if_fail (data);

    data->on_rpress = callback;
}

void button_on_rrelease (GtkWidget * button, ButtonCB callback)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_if_fail (data);

    data->on_rrelease = callback;
}

gboolean button_get_active (GtkWidget * button)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_val_if_fail (data && data->type == BUTTON_TYPE_TOGGLE, FALSE);

    return data->active;
}

void button_set_active (GtkWidget * button, gboolean active)
{
    ButtonData * data = (ButtonData *) g_object_get_data ((GObject *) button, "buttondata");
    g_return_if_fail (data && data->type == BUTTON_TYPE_TOGGLE);

    if (data->active == active)
        return;

    data->active = active;
    gtk_widget_queue_draw (button);
}
