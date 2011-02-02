/************************************************************************
 * libnotify-aosd_event.c                                               *
 *                                                                      *
 * Copyright (C) 2010 Maximilian Bogner <max@mbogner.de>                *
 * Copyright (C) 2011 John Lindgren <john.lindgren@tds.net>             *
 *                                                                      *
 * This program is free software; you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation; either version 3 of the License,       *
 * or (at your option) any later version.                               *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program; if not, see <http://www.gnu.org/licenses/>. *
 ************************************************************************/

#include <glib.h>

#include <audacious/drct.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "libnotify-aosd_common.h"

static gchar * last_title = NULL, * last_message = NULL;

static void clear (void)
{
    g_free (last_title);
    last_title = NULL;
    g_free (last_message);
    last_message = NULL;
}

static void update (void)
{
    if (! aud_drct_get_playing () || ! aud_drct_get_ready ())
        return;

    gint list = aud_playlist_get_playing ();
    gint entry = aud_playlist_get_position (list);

    const gchar * title, * artist, * album;
    aud_playlist_entry_describe (list, entry, & title, & artist, & album, FALSE);

    gchar * message;
    if (artist)
    {
        if (album)
            message = g_strdup_printf ("%s\n%s", artist, album);
        else
            message = g_strdup (artist);
    }
    else if (album)
        message = g_strdup (album);
    else
        message = g_strdup ("");

    if (last_title && last_message && ! strcmp (title, last_title) && ! strcmp
     (message, last_message))
    {
        g_free (message);
        return;
    }

    GdkPixbuf * pb = audgui_pixbuf_for_current ();
    if (pb)
        audgui_pixbuf_scale_within (& pb, 128);

    osd_show (title, message, "audio-x-generic", pb);

    if (pb)
        g_object_unref (pb);

    clear ();
    last_title = g_strdup (title);
    last_message = message;
}

void event_init (void)
{
    hook_associate ("playback ready", (HookFunction) update, NULL);
    hook_associate ("playlist update", (HookFunction) update, NULL);
    hook_associate ("playback stop", (HookFunction) clear, NULL);
}

void event_uninit (void)
{
    hook_dissociate ("playback ready", (HookFunction) update);
    hook_dissociate ("playlist update", (HookFunction) update);
    hook_dissociate ("playback stop", (HookFunction) clear);
}
