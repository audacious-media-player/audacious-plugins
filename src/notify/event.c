/*
 * event.c
 *
 * Copyright (C) 2010 Maximilian Bogner <max@mbogner.de>
 * Copyright (C) 2011-2013 John Lindgren and Jean-Alexandre Anglès d'Auriac
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

#include "event.h"

#include <audacious/drct.h>
#include <libaudcore/i18n.h>
#include <audacious/playlist.h>
#include <libaudcore/runtime.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "osd.h"

static char * last_title = NULL, * last_message = NULL; /* pooled */
static GdkPixbuf * last_pixbuf = NULL;

static void clear_cache (void)
{
    str_unref (last_title);
    last_title = NULL;
    str_unref (last_message);
    last_message = NULL;

    if (last_pixbuf)
    {
        g_object_unref (last_pixbuf);
        last_pixbuf = NULL;
    }
}

static bool_t get_album_art (void)
{
    if (last_pixbuf)
        return FALSE;

    last_pixbuf = audgui_pixbuf_request_current ();
    if (! last_pixbuf)
        return FALSE;

    audgui_pixbuf_scale_within (& last_pixbuf, 96);
    return TRUE;
}

static void show_stopped (void)
{
    osd_show (_("Stopped"), _("Audacious is not playing."), "audacious", NULL);
}

static void show_playing (void)
{
    if (last_title && last_message)
        osd_show (last_title, last_message, "audio-x-generic", last_pixbuf);
}

static void playback_update (void)
{
    if (! aud_drct_get_playing () || ! aud_drct_get_ready ())
        return;

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

    if (str_equal (title, last_title) && str_equal (message, last_message))
    {
        str_unref (title);
        str_unref (message);
        return;
    }

    str_unref (last_title);
    last_title = title;
    str_unref (last_message);
    last_message = message;

    get_album_art ();
    show_playing ();
}

static void art_ready (void)
{
    if (aud_drct_get_playing () && get_album_art ())
        show_playing ();
}

static void playback_paused (void)
{
    if (aud_get_bool ("notify", "resident"))
        show_playing ();
}

static void playback_stopped (void)
{
    clear_cache ();

    if (aud_get_bool ("notify", "resident"))
        show_stopped ();
}

static void force_show (void)
{
    if (aud_drct_get_playing ())
        show_playing ();
    else
        show_stopped ();
}

void event_init (void)
{
    if (aud_drct_get_playing ())
        playback_update ();
    else
        playback_stopped ();

    hook_associate ("playback begin", (HookFunction) clear_cache, NULL);
    hook_associate ("playback ready", (HookFunction) playback_update, NULL);
    hook_associate ("playlist update", (HookFunction) playback_update, NULL);
    hook_associate ("current art ready", (HookFunction) art_ready, NULL);
    hook_associate ("playback pause", (HookFunction) playback_paused, NULL);
    hook_associate ("playback unpause", (HookFunction) playback_paused, NULL);
    hook_associate ("playback stop", (HookFunction) playback_stopped, NULL);

    hook_associate ("aosd toggle", (HookFunction) force_show, NULL);
}

void event_uninit (void)
{
    hook_dissociate ("playback begin", (HookFunction) clear_cache);
    hook_dissociate ("playback ready", (HookFunction) playback_update);
    hook_dissociate ("playlist update", (HookFunction) playback_update);
    hook_dissociate ("current art ready", (HookFunction) art_ready);
    hook_dissociate ("playback pause", (HookFunction) playback_paused);
    hook_dissociate ("playback unpause", (HookFunction) playback_paused);
    hook_dissociate ("playback stop", (HookFunction) playback_stopped);

    hook_dissociate ("aosd toggle", (HookFunction) force_show);

    clear_cache ();
    osd_hide ();
}
