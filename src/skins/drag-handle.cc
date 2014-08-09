/*
 * drag-handle.c
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

#include "drag-handle.h"
#include "skins_cfg.h"

typedef struct {
    gboolean held;
    int x_origin, y_origin;
    void (* press) (void);
    void (* drag) (int x_offset, int y_offset);
} DHandleData;

static gboolean handle_button_press (GtkWidget * handle, GdkEventButton * event)
{
    DHandleData * data = (DHandleData *) g_object_get_data ((GObject *) handle, "dhandledata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    data->held = TRUE;
    data->x_origin = event->x_root;
    data->y_origin = event->y_root;

    if (data->press)
        data->press ();

    return TRUE;
}

static gboolean handle_button_release (GtkWidget * handle, GdkEventButton *
 event)
{
    DHandleData * data = (DHandleData *) g_object_get_data ((GObject *) handle, "dhandledata");
    g_return_val_if_fail (data, FALSE);

    if (event->button != 1)
        return FALSE;

    data->held = FALSE;
    return TRUE;
}

static gboolean handle_motion (GtkWidget * handle, GdkEventMotion * event)
{
    DHandleData * data = (DHandleData *) g_object_get_data ((GObject *) handle, "dhandledata");
    g_return_val_if_fail (data, FALSE);

    if (! data->held)
        return TRUE;

    if (data->drag)
        data->drag ((event->x_root - data->x_origin) / config.scale,
         (event->y_root - data->y_origin) / config.scale);

    return TRUE;
}

static void handle_destroy (GtkWidget * handle)
{
    g_free (g_object_get_data ((GObject *) handle, "dhandledata"));
}

GtkWidget * drag_handle_new (int w, int h, void (* press) (void), void
 (* drag) (int x, int y))
{
    GtkWidget * handle = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) handle, FALSE);
    gtk_widget_set_size_request (handle, w * config.scale, h * config.scale);
    gtk_widget_add_events (handle, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect (handle, "button-press-event", (GCallback)
     handle_button_press, nullptr);
    g_signal_connect (handle, "button-release-event", (GCallback)
     handle_button_release, nullptr);
    g_signal_connect (handle, "motion-notify-event", (GCallback) handle_motion,
     nullptr);
    g_signal_connect (handle, "destroy", (GCallback) handle_destroy, nullptr);

    DHandleData * data = g_new0 (DHandleData, 1);
    data->press = press;
    data->drag = drag;
    g_object_set_data ((GObject *) handle, "dhandledata", data);

    return handle;
}
