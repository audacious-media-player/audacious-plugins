/*
 * MPRIS 2 Server for Audacious
 * Copyright 2011-2012 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <math.h>
#include <stdint.h>

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/probe.h>
#include <libaudcore/runtime.h>

#include "object-core.h"
#include "object-player.h"

class MPRIS2Plugin : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("MPRIS 2 Server"),
        PACKAGE
    };

    constexpr MPRIS2Plugin () : GeneralPlugin (info, true) {}

    bool init ();
    void cleanup ();
};

EXPORT MPRIS2Plugin aud_plugin_instance;

static GObject * object_core, * object_player;
static String last_title, last_artist, last_album, last_file;
static int last_length;
static AudArtPtr image;

static gboolean quit_cb (MprisMediaPlayer2 * object, GDBusMethodInvocation * call,
 void * unused)
{
    aud_quit ();
    mpris_media_player2_complete_quit (object, call);
    return true;
}

static gboolean raise_cb (MprisMediaPlayer2 * object, GDBusMethodInvocation *
 call, void * unused)
{
    aud_ui_show (true);
    mpris_media_player2_complete_raise (object, call);
    return true;
}

static void update_metadata (void * data, GObject * object)
{
    String title, artist, album, file;
    int length = 0;

    if (aud_drct_get_ready ())
    {
        Tuple tuple = aud_drct_get_tuple ();

        title = tuple.get_str (Tuple::Title);
        artist = tuple.get_str (Tuple::Artist);
        album = tuple.get_str (Tuple::Album);
        length = tuple.get_int (Tuple::Length);

        file = aud_drct_get_filename ();
    }

    if (title == last_title && artist == last_artist && album == last_album
     && file == last_file && length == last_length)
        return;

    if (file != last_file)
        image = file ? aud_art_request (file, AUD_ART_FILE) : AudArtPtr ();

    last_title = title;
    last_artist = artist;
    last_album = album;
    last_file = file;
    last_length = length;

    GVariant * elems[7];
    int nelems = 0;

    if (title)
    {
        GVariant * key = g_variant_new_string ("xesam:title");
        GVariant * str = g_variant_new_string (title);
        GVariant * var = g_variant_new_variant (str);
        elems[nelems ++] = g_variant_new_dict_entry (key, var);
    }

    if (artist)
    {
        GVariant * key = g_variant_new_string ("xesam:artist");
        GVariant * str = g_variant_new_string (artist);
        GVariant * array = g_variant_new_array (G_VARIANT_TYPE_STRING, & str, 1);
        GVariant * var = g_variant_new_variant (array);
        elems[nelems ++] = g_variant_new_dict_entry (key, var);
    }

    if (album)
    {
        GVariant * key = g_variant_new_string ("xesam:album");
        GVariant * str = g_variant_new_string (album);
        GVariant * var = g_variant_new_variant (str);
        elems[nelems ++] = g_variant_new_dict_entry (key, var);
    }

    if (file)
    {
        GVariant * key = g_variant_new_string ("xesam:url");
        GVariant * str = g_variant_new_string (file);
        GVariant * var = g_variant_new_variant (str);
        elems[nelems ++] = g_variant_new_dict_entry (key, var);
    }

    if (length > 0)
    {
        GVariant * key = g_variant_new_string ("mpris:length");
        GVariant * val = g_variant_new_int64 ((int64_t) length * 1000);
        GVariant * var = g_variant_new_variant (val);
        elems[nelems ++] = g_variant_new_dict_entry (key, var);
    }

    auto image_file = image.file ();
    if (image_file)
    {
        GVariant * key = g_variant_new_string ("mpris:artUrl");
        GVariant * str = g_variant_new_string (image_file);
        GVariant * var = g_variant_new_variant (str);
        elems[nelems ++] = g_variant_new_dict_entry (key, var);
    }

    GVariant * key = g_variant_new_string ("mpris:trackid");
    GVariant * str = g_variant_new_string ("/org/mpris/MediaPlayer2/CurrentTrack");
    GVariant * var = g_variant_new_variant (str);
    elems[nelems ++] = g_variant_new_dict_entry (key, var);

    GVariant * array = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), elems, nelems);
    g_object_set (object, "metadata", array, nullptr);
}

static void volume_changed (GObject * object)
{
    double vol;
    g_object_get (object, "volume", & vol, nullptr);
    aud_drct_set_volume_main (round (vol * 100));
}

static void update (void * object)
{
    int64_t pos = 0;
    int vol = 0;

    if (aud_drct_get_playing () && aud_drct_get_ready ())
        pos = (int64_t) aud_drct_get_time () * 1000;

    vol = aud_drct_get_volume_main ();

    g_signal_handlers_block_by_func (object, (void *) volume_changed, nullptr);
    g_object_set ((GObject *) object, "position", pos, "volume", (double) vol / 100, nullptr);
    g_signal_handlers_unblock_by_func (object, (void *) volume_changed, nullptr);
}

static void update_playback_status (void * data, GObject * object)
{
    const char * status;

    if (aud_drct_get_playing ())
        status = aud_drct_get_paused () ? "Paused" : "Playing";
    else
        status = "Stopped";

    g_object_set (object, "playback-status", status, nullptr);
    update (object);
}

static void emit_seek (void * data, GObject * object)
{
    g_signal_emit_by_name (object, "seeked", (int64_t) aud_drct_get_time () * 1000);
}

static gboolean next_cb (MprisMediaPlayer2Player * object, GDBusMethodInvocation *
 call, void * unused)
{
    aud_drct_pl_next ();
    mpris_media_player2_player_complete_next (object, call);
    return true;
}

static gboolean pause_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, void * unused)
{
    if (aud_drct_get_playing () && ! aud_drct_get_paused ())
        aud_drct_pause ();

    mpris_media_player2_player_complete_pause (object, call);
    return true;
}

static gboolean play_cb (MprisMediaPlayer2Player * object, GDBusMethodInvocation *
 call, void * unused)
{
    aud_drct_play ();
    mpris_media_player2_player_complete_play (object, call);
    return true;
}

static gboolean play_pause_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, void * unused)
{
    aud_drct_play_pause ();
    mpris_media_player2_player_complete_play_pause (object, call);
    return true;
}

static gboolean previous_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, void * unused)
{
    aud_drct_pl_prev ();
    mpris_media_player2_player_complete_previous (object, call);
    return true;
}

static gboolean seek_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, int64_t offset, void * unused)
{
    aud_drct_seek (aud_drct_get_time () + offset / 1000);
    mpris_media_player2_player_complete_seek (object, call);
    return true;
}

static gboolean set_position_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, const char * track, int64_t pos, void * unused)
{
    if (aud_drct_get_playing ())
        aud_drct_seek (pos / 1000);

    mpris_media_player2_player_complete_set_position (object, call);
    return true;
}

static gboolean stop_cb (MprisMediaPlayer2Player * object, GDBusMethodInvocation *
 call, void * unused)
{
    if (aud_drct_get_playing ())
        aud_drct_stop ();

    mpris_media_player2_player_complete_stop (object, call);
    return true;
}

void MPRIS2Plugin::cleanup ()
{
    hook_dissociate ("playback begin", (HookFunction) update_playback_status);
    hook_dissociate ("playback pause", (HookFunction) update_playback_status);
    hook_dissociate ("playback stop", (HookFunction) update_playback_status);
    hook_dissociate ("playback unpause", (HookFunction) update_playback_status);

    hook_dissociate ("playback ready", (HookFunction) update_metadata);
    hook_dissociate ("playback stop", (HookFunction) update_metadata);
    hook_dissociate ("tuple change", (HookFunction) update_metadata);

    hook_dissociate ("playback ready", (HookFunction) emit_seek);
    hook_dissociate ("playback seek", (HookFunction) emit_seek);

    timer_remove (TimerRate::Hz4, update, object_player);

    g_object_unref (object_core);
    g_object_unref (object_player);

    last_title = String ();
    last_artist = String ();
    last_album = String ();
    last_file = String ();
    last_length = 0;

    image.clear ();
}

bool MPRIS2Plugin::init ()
{
#if ! GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    GError * error = nullptr;
    GDBusConnection * bus = g_bus_get_sync (G_BUS_TYPE_SESSION, nullptr, & error);

    if (! bus)
    {
        AUDERR ("%s\n", error->message);
        g_error_free (error);
        return false;
    }

    g_bus_own_name_on_connection (bus, "org.mpris.MediaPlayer2.audacious",
     (GBusNameOwnerFlags) 0, nullptr, nullptr, nullptr, nullptr);

    object_core = (GObject *) mpris_media_player2_skeleton_new ();

    g_object_set (object_core,
     "can-quit", (gboolean) true,
     "can-raise", (gboolean) true,
     "desktop-entry", "audacious",
     "identity", "Audacious",
     nullptr);

    g_signal_connect (object_core, "handle-quit", (GCallback) quit_cb, nullptr);
    g_signal_connect (object_core, "handle-raise", (GCallback) raise_cb, nullptr);

    object_player = (GObject *) mpris_media_player2_player_skeleton_new ();

    g_object_set (object_player,
     "can-control", (gboolean) true,
     "can-go-next", (gboolean) true,
     "can-go-previous", (gboolean) true,
     "can-pause", (gboolean) true,
     "can-play", (gboolean) true,
     "can-seek", (gboolean) true,
     nullptr);

    update_playback_status (nullptr, object_player);

    if (aud_drct_get_playing () && aud_drct_get_ready ())
        emit_seek (nullptr, object_player);

    hook_associate ("playback begin", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback pause", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback stop", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback unpause", (HookFunction) update_playback_status, object_player);

    hook_associate ("playback ready", (HookFunction) update_metadata, object_player);
    hook_associate ("playback stop", (HookFunction) update_metadata, object_player);
    hook_associate ("tuple change", (HookFunction) update_metadata, object_player);

    hook_associate ("playback ready", (HookFunction) emit_seek, object_player);
    hook_associate ("playback seek", (HookFunction) emit_seek, object_player);

    timer_add (TimerRate::Hz4, update, object_player);

    g_signal_connect (object_player, "handle-next", (GCallback) next_cb, nullptr);
    g_signal_connect (object_player, "handle-pause", (GCallback) pause_cb, nullptr);
    g_signal_connect (object_player, "handle-play", (GCallback) play_cb, nullptr);
    g_signal_connect (object_player, "handle-play-pause", (GCallback) play_pause_cb, nullptr);
    g_signal_connect (object_player, "handle-previous", (GCallback) previous_cb, nullptr);
    g_signal_connect (object_player, "handle-seek", (GCallback) seek_cb, nullptr);
    g_signal_connect (object_player, "handle-set-position", (GCallback) set_position_cb, nullptr);
    g_signal_connect (object_player, "handle-stop", (GCallback) stop_cb, nullptr);

    g_signal_connect (object_player, "notify::volume", (GCallback) volume_changed, nullptr);

    if (! g_dbus_interface_skeleton_export ((GDBusInterfaceSkeleton *)
     object_core, bus, "/org/mpris/MediaPlayer2", & error) ||
     ! g_dbus_interface_skeleton_export ((GDBusInterfaceSkeleton *)
     object_player, bus, "/org/mpris/MediaPlayer2", & error))
    {
        cleanup ();
        AUDERR ("%s\n", error->message);
        g_error_free (error);
        return false;
    }

    return true;
}
