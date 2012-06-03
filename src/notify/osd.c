/*
 * osd.c
 *
 * Copyright (C) 2010 Maximilian Bogner <max@mbogner.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <audacious/debug.h>
#include <libnotify/notify.h>

#include "osd.h"

NotifyNotification *notification = NULL;

gboolean osd_init() {
    notification = NULL;
    return notify_init ("Audacious");
}

void osd_uninit (void)
{
    if (notification)
    {
        g_object_unref (notification);
        notification = NULL;
    }

    notify_uninit();
}

void osd_closed_handler(NotifyNotification *notification2, gpointer data) {
    if(notification != NULL) {
        g_object_unref(notification);
        notification = NULL;
        AUDDBG("notification unrefed!\n");
    }
}

void osd_show (const gchar * title, const gchar * _message, const gchar * icon,
 GdkPixbuf * pb)
{
    gchar * message = g_markup_escape_text (_message, -1);
    GError *error = NULL;

    if(notification == NULL) {
        notification = notify_notification_new(title, message, pb == NULL ? icon : NULL);
        g_signal_connect(notification, "closed", G_CALLBACK(osd_closed_handler), NULL);
        AUDDBG("new osd created! (notification=%p)\n", (void *) notification);
    } else {
        if(notify_notification_update(notification, title, message, pb == NULL ? icon : NULL)) {
            AUDDBG("old osd updated! (notification=%p)\n", (void *) notification);
        } else {
            AUDDBG("could not update old osd! (notification=%p)\n", (void *) notification);
        }
    }

    if(pb != NULL)
        notify_notification_set_icon_from_pixbuf(notification, pb);

    if(!notify_notification_show(notification, &error))
        AUDDBG("%s!\n", error->message);

    g_free (message);
}
