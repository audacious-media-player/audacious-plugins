/*
 * event.c
 *
 * Copyright (C) 2010 Maximilian Bogner <max@mbogner.de>
 * Copyright (C) 2011 John Lindgren <john.lindgren@tds.net>
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

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "event.h"
#include "osd.h"

static char * last_title = NULL, * last_message = NULL; /* pooled */

static void clear (void)
{
    str_unref (last_title);
    last_title = NULL;
    str_unref (last_message);
    last_message = NULL;
}

static void reshow (void)
{
    if (! last_title || ! last_message)
        return;

    GdkPixbuf * pb = audgui_pixbuf_request_current ();
    if (pb)
        audgui_pixbuf_scale_within (& pb, 96);

    osd_show (last_title, last_message, "audio-x-generic", pb);

    if (pb)
        g_object_unref (pb);
}

static void update (void * unused, void * explicit)
{
    if (! aud_drct_get_playing () || ! aud_drct_get_ready ())
    {
        if (GPOINTER_TO_INT (explicit))
            osd_show (_("Stopped"), _("Audacious is not playing."), NULL, NULL);

        return;
    }

    int list = aud_playlist_get_playing ();
    int entry = aud_playlist_get_position (list);

    char * title, * artist, * album;
    aud_playlist_entry_describe (list, entry, & title, & artist, & album, FALSE);

    char * message;
    if (artist)
    {
        if (album)
            message = str_printf ("%s\n%s", artist, album);
        else
            message = str_ref (artist);
    }
    else if (album)
        message = str_ref (album);
    else
        message = str_get ("");

    str_unref (artist);
    str_unref (album);

    /* pointer comparison works for pooled strings */
    if (! GPOINTER_TO_INT (explicit) && title == last_title && message == last_message)
    {
        str_unref (title);
        str_unref (message);
        return;
    }

    str_unref (last_title);
    last_title = title;
    str_unref (last_message);
    last_message = message;

    reshow ();
}

void event_init (void)
{
    update (NULL, GINT_TO_POINTER (FALSE));
    hook_associate ("aosd toggle", (HookFunction) update, GINT_TO_POINTER (TRUE));
    hook_associate ("playback ready", (HookFunction) update, GINT_TO_POINTER (FALSE));
    hook_associate ("playlist update", (HookFunction) update, GINT_TO_POINTER (FALSE));
    hook_associate ("current art ready", (HookFunction) reshow, NULL);
    hook_associate ("playback begin", (HookFunction) clear, NULL);
    hook_associate ("playback stop", (HookFunction) clear, NULL);
}

void event_uninit (void)
{
    hook_dissociate ("aosd toggle", (HookFunction) update);
    hook_dissociate ("playback ready", (HookFunction) update);
    hook_dissociate ("playlist update", (HookFunction) update);
    hook_dissociate ("current art ready", (HookFunction) reshow);
    hook_dissociate ("playback begin", (HookFunction) clear);
    hook_dissociate ("playback stop", (HookFunction) clear);
    clear ();
}
