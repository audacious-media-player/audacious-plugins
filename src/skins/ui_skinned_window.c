/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2007 William Pitcock <nenolod -at- sacredspiral.co.uk>
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
#include "ui_dock.h"
#include "ui_skinned_window.h"

typedef struct {
    void (* draw) (GtkWidget * window, cairo_t * cr);
    GtkWidget * normal, * shaded;
    gboolean is_shaded, is_moving;
} WindowData;

DRAW_FUNC_BEGIN (window_draw)
    WindowData * data = g_object_get_data ((GObject *) wid, "windowdata");
    g_return_val_if_fail (data, FALSE);

    if (data->draw)
        data->draw (wid, cr);
DRAW_FUNC_END

static gboolean window_button_press (GtkWidget * window, GdkEventButton * event)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
    g_return_val_if_fail (data, FALSE);

    /* pass double clicks through; they are handled elsewhere */
    if (event->button != 1 || event->type == GDK_2BUTTON_PRESS)
        return FALSE;

    if (data->is_moving)
        return TRUE;

    dock_move_start (window, event->x, event->y);
    data->is_moving = TRUE;
    return TRUE;
}

static gboolean window_button_release (GtkWidget * window, GdkEventButton *
 event)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    if (! data->is_moving)
        return TRUE;

    dock_move_end ();
    data->is_moving = FALSE;
    return TRUE;
}

static gboolean window_motion (GtkWidget * window, GdkEventMotion * event)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
    g_return_val_if_fail (data, FALSE);

    if (! data->is_moving)
        return TRUE;

    dock_move (event->x, event->y);
    return TRUE;
}

static void window_realize (GtkWidget * window)
{
    GdkWindow * gdk_window = gtk_widget_get_window (window);
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_window_set_background_pattern (gdk_window, NULL);
#else
    gdk_window_set_back_pixmap (gdk_window, NULL);
#endif
}

static void window_destroy (GtkWidget * window)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    dock_remove_window (window);

    if (data->is_shaded)
        gtk_container_remove ((GtkContainer *) window, data->shaded);
    else
        gtk_container_remove ((GtkContainer *) window, data->normal);

    g_object_unref (data->normal);
    g_object_unref (data->shaded);
    g_free (data);
}

static void window_set_size_real (GtkWidget * window, gint w, gint h)
{
    GdkGeometry hints = {.min_width = w, .min_height = h, .max_width = w,
     .max_height = h};
    gtk_window_set_geometry_hints ((GtkWindow *) window, NULL, & hints,
     GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);

    gtk_window_resize ((GtkWindow *) window, w, h);
}

GtkWidget * window_new (gint * x, gint * y, gint w, gint h, gboolean main,
 gboolean shaded, void (* draw) (GtkWidget * window, cairo_t * cr))
{
    GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated ((GtkWindow *) window, FALSE);
    gtk_window_move ((GtkWindow *) window, * x, * y);
    window_set_size_real (window, w, h);

    gtk_widget_add_events (window, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect (window, "realize", (GCallback) window_realize, NULL);
    g_signal_connect (window, DRAW_SIGNAL, (GCallback) window_draw, NULL);
    g_signal_connect (window, "button-press-event", (GCallback) window_button_press, NULL);
    g_signal_connect (window, "button-release-event", (GCallback) window_button_release, NULL);
    g_signal_connect (window, "motion-notify-event", (GCallback) window_motion, NULL);
    g_signal_connect (window, "destroy", (GCallback) window_destroy, NULL);

    WindowData * data = g_malloc0 (sizeof (WindowData));
    g_object_set_data ((GObject *) window, "windowdata", data);

    data->normal = gtk_fixed_new ();
    g_object_ref (data->normal);

    data->shaded = gtk_fixed_new ();
    g_object_ref (data->shaded);

    if (shaded)
        gtk_container_add ((GtkContainer *) window, data->shaded);
    else
        gtk_container_add ((GtkContainer *) window, data->normal);

    data->is_shaded = shaded;
    data->draw = draw;

    dock_add_window (window, x, y, w, h, main);
    return window;
}

void window_set_size (GtkWidget * window, gint w, gint h)
{
    window_set_size_real (window, w, h);
    dock_set_size (window, w, h);
}

void window_set_shaded (GtkWidget * window, gboolean shaded)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
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
}

void window_put_widget (GtkWidget * window, gboolean shaded, GtkWidget * widget,
 gint x, gint y)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    GtkWidget * fixed = shaded ? data->shaded : data->normal;
    gtk_fixed_put ((GtkFixed *) fixed, widget, x, y);
}

void window_move_widget (GtkWidget * window, gboolean shaded, GtkWidget *
 widget, gint x, gint y)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    GtkWidget * fixed = shaded ? data->shaded : data->normal;
    gtk_fixed_move ((GtkFixed *) fixed, widget, x, y);
}

void window_show_all (GtkWidget * window)
{
    WindowData * data = g_object_get_data ((GObject *) window, "windowdata");
    g_return_if_fail (data);

    gtk_widget_show_all (data->normal);
    gtk_widget_show_all (data->shaded);
}
