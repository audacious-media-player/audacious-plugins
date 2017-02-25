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
#include "osd.h"

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>

#ifdef USE_GTK
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#endif
#ifdef USE_QT
#include <libaudqt/libaudqt.h>
#endif

static String last_title, last_message;
static GdkPixbuf * last_pixbuf = nullptr;

#ifdef USE_QT
static QImage qimage;
#endif

static void clear_cache ()
{
    last_title = String ();
    last_message = String ();

    if (last_pixbuf)
    {
        g_object_unref (last_pixbuf);
        last_pixbuf = nullptr;
    }

#ifdef USE_QT
    qimage = QImage ();
#endif

    osd_hide ();
}

static void get_album_art ()
{
    if (last_pixbuf)
        return;

#ifdef USE_GTK
    if (aud_get_mainloop_type () == MainloopType::GLib)
    {
        AudguiPixbuf pb = audgui_pixbuf_request_current ();
        if (pb)
            audgui_pixbuf_scale_within (pb, audgui_get_dpi ());

        last_pixbuf = pb.release ();
    }
#endif
#ifdef USE_QT
    if (aud_get_mainloop_type () == MainloopType::Qt)
    {
        QImage image = audqt::art_request_current (96, 96, false).toImage ();
        if (! image.isNull ())
            qimage = image.convertToFormat (QImage::Format_RGBA8888);

        // convert QImage to GdkPixbuf.
        // note that the GdkPixbuf shares the same internal image data.
        if (! qimage.isNull ())
            last_pixbuf = gdk_pixbuf_new_from_data (qimage.bits (),
             GDK_COLORSPACE_RGB, true, 8, qimage.width (), qimage.height (),
             qimage.bytesPerLine (), nullptr, nullptr);
    }
#endif
}

static void show_stopped ()
{
    osd_show (_("Stopped"), _("Audacious is not playing."), "audacious", nullptr);
}

static void show_playing ()
{
    if (last_title && last_message)
        osd_show (last_title, last_message, "audio-x-generic", last_pixbuf);
}

static void playback_update ()
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

static void playback_paused ()
{
    if (aud_get_bool ("notify", "resident"))
        show_playing ();
}

static void playback_stopped ()
{
    clear_cache ();

    if (aud_get_bool ("notify", "resident"))
        show_stopped ();
}

static void force_show ()
{
    if (aud_drct_get_playing ())
        show_playing ();
    else
        show_stopped ();
}

void event_init ()
{
#ifdef USE_GTK
    if (aud_get_mainloop_type () == MainloopType::GLib)
        audgui_init ();
#endif
#ifdef USE_QT
    if (aud_get_mainloop_type () == MainloopType::Qt)
        audqt::init ();
#endif

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

void event_uninit ()
{
    hook_dissociate ("playback begin", (HookFunction) clear_cache);
    hook_dissociate ("playback ready", (HookFunction) playback_update);
    hook_dissociate ("tuple change", (HookFunction) playback_update);
    hook_dissociate ("playback pause", (HookFunction) playback_paused);
    hook_dissociate ("playback unpause", (HookFunction) playback_paused);
    hook_dissociate ("playback stop", (HookFunction) playback_stopped);

    hook_dissociate ("aosd toggle", (HookFunction) force_show);

    clear_cache ();

#ifdef USE_GTK
    if (aud_get_mainloop_type () == MainloopType::GLib)
        audgui_cleanup ();
#endif
#ifdef USE_QT
    if (aud_get_mainloop_type () == MainloopType::Qt)
        audqt::cleanup ();
#endif
}
