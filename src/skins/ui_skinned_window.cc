/*
 * ui_skinned_window.c
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

#include "drawing.h"
#include "skins_cfg.h"
#include "ui_dock.h"
#include "ui_skinned_window.h"

typedef struct {
    void (* draw) (GtkWidget * window, cairo_t * cr);
    GtkWidget * normal, * shaded;
    GdkRegion * shape, * sshape;
    gboolean is_shaded, is_moving;
} WindowData;

DRAW_FUNC_BEGIN (window_draw, WindowData)
    if (data->draw)
        data->draw (wid, cr);
DRAW_FUNC_END

static void window_apply_shape (GtkWidget * window, WindowData * data)
{
    gdk_window_shape_combine_region (gtk_widget_get_window (window),
     data->is_shaded ? data->sshape : data->shape, 0, 0);
}

static void window_realize (GtkWidget * window, WindowData * data)
{
    gdk_window_set_back_pixmap (gtk_widget_get_window (window), nullptr, FALSE);
    window_apply_shape (window, data);
}

static gboolean window_button_press (GtkWidget * window, GdkEventButton * event, WindowData * data)
{
    /* pass double clicks through; they are handled elsewhere */
    if (event->button != 1 || event->type == GDK_2BUTTON_PRESS)
        return FALSE;

    if (data->is_moving)
        return TRUE;

    dock_move_start (window, event->x_root, event->y_root);
    data->is_moving = TRUE;
    return TRUE;
}

static gboolean window_button_release (GtkWidget * window,
 GdkEventButton * event, WindowData * data)
{
    if (event->button != 1)
        return FALSE;

    data->is_moving = FALSE;
    return TRUE;
}

static gboolean window_motion (GtkWidget * window, GdkEventMotion * event, WindowData * data)
{
    if (! data->is_moving)
        return TRUE;

    dock_move (event->x_root, event->y_root);
    return TRUE;
}

static void window_destroy (GtkWidget * window, WindowData * data)
{
    dock_remove_window (window);

    g_object_unref (data->normal);
    g_object_unref (data->shaded);

    if (data->shape)
        gdk_region_destroy (data->shape);
    if (data->sshape)
        gdk_region_destroy (data->sshape);

    g_free (data);
}

GtkWidget * window_new (int * x, int * y, int w, int h, gboolean main,
 gboolean shaded, void (* draw) (GtkWidget * window, cairo_t * cr))
{
    w *= config.scale;
    h *= config.scale;

    GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated ((GtkWindow *) window, FALSE);
    gtk_window_set_resizable ((GtkWindow *) window, FALSE);
    gtk_window_move ((GtkWindow *) window, * x, * y);
    gtk_widget_set_size_request (window, w, h);
    gtk_window_resize ((GtkWindow *) window, w, h);

    gtk_widget_set_app_paintable (window, TRUE);
    gtk_widget_add_events (window, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);

    /* We set None as the background pixmap in order to avoid flickering.
     * Setting a blank GtkStyle prevents GTK 2.x from overriding this. */
    GtkStyle * style = gtk_style_new ();
    gtk_widget_set_style (window, style);
    g_object_unref (style);

    WindowData * data = g_new0 (WindowData, 1);
    g_object_set_data ((GObject *) window, "windowdata", data);

    data->normal = gtk_fixed_new ();
    g_object_ref_sink (data->normal);

    data->shaded = gtk_fixed_new ();
    g_object_ref_sink (data->shaded);

    if (shaded)
        gtk_container_add ((GtkContainer *) window, data->shaded);
    else
        gtk_container_add ((GtkContainer *) window, data->normal);

    data->is_shaded = shaded;
    data->draw = draw;

    DRAW_CONNECT (window, window_draw, data);
    g_signal_connect (window, "realize", (GCallback) window_realize, data);
    g_signal_connect (window, "button-press-event", (GCallback) window_button_press, data);
    g_signal_connect (window, "button-release-event", (GCallback) window_button_release, data);
    g_signal_connect (window, "motion-notify-event", (GCallback) window_motion, data);
    g_signal_connect (window, "destroy", (GCallback) window_destroy, data);

    dock_add_window (window, x, y, w, h, main);
    return window;
}

void window_set_size (GtkWidget * window, int w, int h)
{
    w *= config.scale;
    h *= config.scale;

    gtk_widget_set_size_request (window, w, h);
    gtk_window_resize ((GtkWindow *) window, w, h);
    dock_set_size (window, w, h);
}

void window_set_shapes (GtkWidget * window, GdkRegion * shape, GdkRegion * sshape)
{
    WindowData * data = (WindowData *) g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    if (data->shape)
        gdk_region_destroy (data->shape);
    if (data->sshape)
        gdk_region_destroy (data->sshape);

    data->shape = shape;
    data->sshape = sshape;
    
    if (gtk_widget_get_realized (window))
        window_apply_shape (window, data);
}

void window_set_shaded (GtkWidget * window, gboolean shaded)
{
    WindowData * data = (WindowData *) g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    if (data->is_shaded == shaded)
        return;

    if (shaded)
    {
        gtk_container_remove ((GtkContainer *) window, data->normal);
        gtk_container_add ((GtkContainer *) window, data->shaded);
    }
    else
    {
        gtk_container_remove ((GtkContainer *) window, data->shaded);
        gtk_container_add ((GtkContainer *) window, data->normal);
    }

    data->is_shaded = shaded;
    
    if (gtk_widget_get_realized (window))
        window_apply_shape (window, data);
}

void window_put_widget (GtkWidget * window, gboolean shaded, GtkWidget * widget,
 int x, int y)
{
    x *= config.scale;
    y *= config.scale;

    WindowData * data = (WindowData *) g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    GtkWidget * fixed = shaded ? data->shaded : data->normal;
    gtk_fixed_put ((GtkFixed *) fixed, widget, x, y);
}

void window_move_widget (GtkWidget * window, gboolean shaded, GtkWidget *
 widget, int x, int y)
{
    x *= config.scale;
    y *= config.scale;

    WindowData * data = (WindowData *) g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    GtkWidget * fixed = shaded ? data->shaded : data->normal;
    gtk_fixed_move ((GtkFixed *) fixed, widget, x, y);
}

void window_show_all (GtkWidget * window)
{
    WindowData * data = (WindowData *) g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    gtk_widget_show_all (data->normal);
    gtk_widget_show_all (data->shaded);
}
