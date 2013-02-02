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

#include "osd.h"

static NotifyNotification * notification = NULL;

bool_t osd_init()
{
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

static void osd_closed_handler (void)
{
    if (notification)
    {
        g_object_unref (notification);
        notification = NULL;
    }
}

void osd_show (const char * title, const char * _message, const char * icon,
 GdkPixbuf * pixbuf)
{
    char * message = g_markup_escape_text (_message, -1);

    if (pixbuf)
        icon = NULL;

    if (notification)
        notify_notification_update (notification, title, message, icon);
    else
    {
        notification = notify_notification_new (title, message, icon);
        g_signal_connect (notification, "closed", (GCallback) osd_closed_handler, NULL);
    }

    if (pixbuf)
        notify_notification_set_icon_from_pixbuf (notification, pixbuf);

    notify_notification_show (notification, NULL);

    g_free (message);
}
