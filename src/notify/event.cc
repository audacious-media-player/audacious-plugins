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

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "osd.h"

static String last_title, last_message;
static GdkPixbuf * last_pixbuf = nullptr;

static void clear_cache (void)
{
    last_title = String ();
    last_message = String ();

    if (last_pixbuf)
    {
        g_object_unref (last_pixbuf);
        last_pixbuf = nullptr;
    }

    osd_hide ();
}

static gboolean get_album_art (void)
{
    if (last_pixbuf)
        return false;

    last_pixbuf = audgui_pixbuf_request_current ();
    if (! last_pixbuf)
        return false;

    audgui_pixbuf_scale_within (& last_pixbuf, audgui_get_dpi ());
    return true;
}

static void show_stopped (void)
{
    osd_show (_("Stopped"), _("Audacious is not playing."), "audacious", nullptr);
}

static void show_playing (void)
{
    if (last_title && last_message)
        osd_show (last_title, last_message, "audio-x-generic", last_pixbuf);
}

static void playback_update (void)
{
    Tuple tuple = aud_drct_get_tuple ();
    String title = tuple.get_str (Tuple::Title);
    String artist = tuple.get_str (Tuple::Artist);
    String album = tuple.get_str (Tuple::Album);

    String message;
    if (artist)
    {
        if (album && aud_get_bool ("notify", "album"))
            message = String (str_printf ("%s\n%s", (const char *) artist, (const char *) album));
        else
            message = artist;
    }
    else if (album)
        message = album;
    else
        message = String ("");

    if (title == last_title && message == last_message)
        return;

    last_title = title;
    last_message = message;

    get_album_art ();
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
    if (aud_drct_get_ready ())
        playback_update ();
    else
        playback_stopped ();

    hook_associate ("playback begin", (HookFunction) clear_cache, nullptr);
    hook_associate ("playback ready", (HookFunction) playback_update, nullptr);
    hook_associate ("tuple change", (HookFunction) playback_update, nullptr);
    hook_associate ("playback pause", (HookFunction) playback_paused, nullptr);
    hook_associate ("playback unpause", (HookFunction) playback_paused, nullptr);
    hook_associate ("playback stop", (HookFunction) playback_stopped, nullptr);

    hook_associate ("aosd toggle", (HookFunction) force_show, nullptr);
}

void event_uninit (void)
{
    hook_dissociate ("playback begin", (HookFunction) clear_cache);
    hook_dissociate ("playback ready", (HookFunction) playback_update);
    hook_dissociate ("tuple change", (HookFunction) playback_update);
    hook_dissociate ("playback pause", (HookFunction) playback_paused);
    hook_dissociate ("playback unpause", (HookFunction) playback_paused);
    hook_dissociate ("playback stop", (HookFunction) playback_stopped);

    hook_dissociate ("aosd toggle", (HookFunction) force_show);

    clear_cache ();
}
