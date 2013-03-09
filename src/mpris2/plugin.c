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
#include <stdio.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>

#include "object-core.h"
#include "object-player.h"

static GDBusConnection * bus;
static GObject * object_core, * object_player;
static char * last_title, * last_artist, * last_album, * last_file;
static int last_length;
static const char * image_file;
static bool_t recheck_image;
static GVariantType * metadata_type;
static int update_timer;

static bool_t quit_cb (MprisMediaPlayer2 * object, GDBusMethodInvocation * call,
 void * unused)
{
    aud_drct_quit ();
    mpris_media_player2_complete_quit (object, call);
    return TRUE;
}

static bool_t raise_cb (MprisMediaPlayer2 * object, GDBusMethodInvocation *
 call, void * unused)
{
    aud_interface_show (TRUE);
    mpris_media_player2_complete_raise (object, call);
    return TRUE;
}

static void update_metadata (void * data, GObject * object)
{
    char * title = NULL, * artist = NULL, * album = NULL, * file = NULL;
    int length = 0;

    int playlist = aud_playlist_get_playing ();
    int entry = (playlist >= 0) ? aud_playlist_get_position (playlist) : -1;

    if (entry >= 0)
    {
        aud_playlist_entry_describe (playlist, entry, & title, & artist, & album, TRUE);
        file = aud_playlist_entry_get_filename (playlist, entry);
        length = aud_playlist_entry_get_length (playlist, entry, TRUE);
    }

    /* pointer comparison works for pooled strings */
    if (title == last_title && artist == last_artist && album == last_album &&
     file == last_file && length == last_length && ! recheck_image)
    {
        str_unref (title);
        str_unref (artist);
        str_unref (album);
        str_unref (file);
        return;
    }

    if (file != last_file || recheck_image)
    {
        if (image_file)
            aud_art_unref (last_file);
        image_file = file ? aud_art_request_file (file) : NULL;
        recheck_image = FALSE;
    }

    str_unref (last_title);
    str_unref (last_artist);
    str_unref (last_album);
    str_unref (last_file);
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

    if (! metadata_type)
        metadata_type = g_variant_type_new ("{sv}");

    GVariant * array = g_variant_new_array (metadata_type, elems, nelems);
    g_object_set (object, "metadata", array, NULL);
}

static void update_image (void * data, GObject * object)
{
    recheck_image = TRUE;
    update_metadata (data, object);
}

static void volume_changed (GObject * object)
{
    double vol;
    g_object_get (object, "volume", & vol, NULL);
    aud_drct_set_volume_main (round (vol * 100));
}

static bool_t update (GObject * object)
{
    int64_t pos = 0;
    int vol = 0;

    if (aud_drct_get_playing () && aud_drct_get_ready ())
        pos = (int64_t) aud_drct_get_time () * 1000;

    aud_drct_get_volume_main (& vol);

    g_signal_handlers_block_by_func (object, (void *) volume_changed, NULL);
    g_object_set (object, "position", pos, "volume", (double) vol / 100, NULL);
    g_signal_handlers_unblock_by_func (object, (void *) volume_changed, NULL);
    return TRUE;
}

static void update_playback_status (void * data, GObject * object)
{
    const char * status;

    if (aud_drct_get_playing ())
        status = aud_drct_get_paused () ? "Paused" : "Playing";
    else
        status = "Stopped";

    g_object_set (object, "playback-status", status, NULL);
    update (object);
}

static void emit_seek (void * data, GObject * object)
{
    g_signal_emit_by_name (object, "seeked", (int64_t) aud_drct_get_time () * 1000);
}

static bool_t next_cb (MprisMediaPlayer2Player * object, GDBusMethodInvocation *
 call, void * unused)
{
    aud_drct_pl_next ();
    mpris_media_player2_player_complete_next (object, call);
    return TRUE;
}

static bool_t pause_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, void * unused)
{
    if (aud_drct_get_playing () && ! aud_drct_get_paused ())
        aud_drct_pause ();

    mpris_media_player2_player_complete_pause (object, call);
    return TRUE;
}

static bool_t play_cb (MprisMediaPlayer2Player * object, GDBusMethodInvocation *
 call, void * unused)
{
    aud_drct_play ();
    mpris_media_player2_player_complete_play (object, call);
    return TRUE;
}

static bool_t play_pause_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, void * unused)
{
    aud_drct_play_pause ();
    mpris_media_player2_player_complete_play_pause (object, call);
    return TRUE;
}

static bool_t previous_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, void * unused)
{
    aud_drct_pl_prev ();
    mpris_media_player2_player_complete_previous (object, call);
    return TRUE;
}

static bool_t seek_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, int64_t offset, void * unused)
{
    aud_drct_seek (aud_drct_get_time () + offset / 1000);
    mpris_media_player2_player_complete_seek (object, call);
    return TRUE;
}

static bool_t set_position_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, const char * track, int64_t pos, void * unused)
{
    if (aud_drct_get_playing ())
        aud_drct_seek (pos / 1000);

    mpris_media_player2_player_complete_set_position (object, call);
    return TRUE;
}

static bool_t stop_cb (MprisMediaPlayer2Player * object, GDBusMethodInvocation *
 call, void * unused)
{
    if (aud_drct_get_playing ())
        aud_drct_stop ();

    mpris_media_player2_player_complete_stop (object, call);
    return TRUE;
}

void mpris2_cleanup (void)
{
    hook_dissociate ("playback begin", (HookFunction) update_playback_status);
    hook_dissociate ("playback pause", (HookFunction) update_playback_status);
    hook_dissociate ("playback stop", (HookFunction) update_playback_status);
    hook_dissociate ("playback unpause", (HookFunction) update_playback_status);

    hook_dissociate ("playlist set playing", (HookFunction) update_metadata);
    hook_dissociate ("playlist position", (HookFunction) update_metadata);
    hook_dissociate ("playlist update", (HookFunction) update_metadata);

    hook_dissociate ("current art ready", (HookFunction) update_image);

    hook_dissociate ("playback ready", (HookFunction) emit_seek);
    hook_dissociate ("playback seek", (HookFunction) emit_seek);

    if (update_timer)
    {
        g_source_remove (update_timer);
        update_timer = 0;
    }

    g_dbus_connection_close_sync (bus, NULL, NULL);
    g_object_unref (object_core);
    g_object_unref (object_player);

    if (image_file)
    {
        aud_art_unref (last_file);
        image_file = NULL;
    }

    str_unref (last_title);
    str_unref (last_artist);
    str_unref (last_album);
    str_unref (last_file);
    last_title = last_artist = last_album = last_file = NULL;
    last_length = 0;

    if (metadata_type)
    {
        g_variant_type_free (metadata_type);
        metadata_type = NULL;
    }
}

bool_t mpris2_init (void)
{
    GError * error = NULL;
    bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, & error);

    if (! bus)
    {
        fprintf (stderr, "mpris2: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    g_bus_own_name_on_connection (bus, "org.mpris.MediaPlayer2.audacious", 0,
     NULL, NULL, NULL, NULL);

    object_core = (GObject *) mpris_media_player2_skeleton_new ();

    g_object_set (object_core,
     "can-quit", TRUE,
     "can-raise", TRUE,
     "desktop-entry", "audacious",
     "identity", "Audacious",
     NULL);

    g_signal_connect (object_core, "handle-quit", (GCallback) quit_cb, NULL);
    g_signal_connect (object_core, "handle-raise", (GCallback) raise_cb, NULL);

    object_player = (GObject *) mpris_media_player2_player_skeleton_new ();

    g_object_set (object_player,
     "can-control", TRUE,
     "can-go-next", TRUE,
     "can-go-previous", TRUE,
     "can-pause", TRUE,
     "can-play", TRUE,
     "can-seek", TRUE,
     NULL);

    update_timer = g_timeout_add (250, (GSourceFunc) update, object_player);
    update_playback_status (NULL, object_player);

    if (aud_drct_get_playing () && aud_drct_get_ready ())
        emit_seek (NULL, object_player);

    hook_associate ("playback begin", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback pause", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback stop", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback unpause", (HookFunction) update_playback_status, object_player);

    hook_associate ("playlist set playing", (HookFunction) update_metadata, object_player);
    hook_associate ("playlist position", (HookFunction) update_metadata, object_player);
    hook_associate ("playlist update", (HookFunction) update_metadata, object_player);

    hook_associate ("current art ready", (HookFunction) update_image, object_player);

    hook_associate ("playback ready", (HookFunction) emit_seek, object_player);
    hook_associate ("playback seek", (HookFunction) emit_seek, object_player);

    g_signal_connect (object_player, "handle-next", (GCallback) next_cb, NULL);
    g_signal_connect (object_player, "handle-pause", (GCallback) pause_cb, NULL);
    g_signal_connect (object_player, "handle-play", (GCallback) play_cb, NULL);
    g_signal_connect (object_player, "handle-play-pause", (GCallback) play_pause_cb, NULL);
    g_signal_connect (object_player, "handle-previous", (GCallback) previous_cb, NULL);
    g_signal_connect (object_player, "handle-seek", (GCallback) seek_cb, NULL);
    g_signal_connect (object_player, "handle-set-position", (GCallback) set_position_cb, NULL);
    g_signal_connect (object_player, "handle-stop", (GCallback) stop_cb, NULL);

    g_signal_connect (object_player, "notify::volume", (GCallback) volume_changed, NULL);

    if (! g_dbus_interface_skeleton_export ((GDBusInterfaceSkeleton *)
     object_core, bus, "/org/mpris/MediaPlayer2", & error) ||
     ! g_dbus_interface_skeleton_export ((GDBusInterfaceSkeleton *)
     object_player, bus, "/org/mpris/MediaPlayer2", & error))
    {
        mpris2_cleanup ();
        fprintf (stderr, "mpris2: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    return TRUE;
}

AUD_GENERAL_PLUGIN
(
    .name = N_("MPRIS 2 Server"),
    .domain = PACKAGE,
    .enabled_by_default = TRUE,
    .init = mpris2_init,
    .cleanup = mpris2_cleanup
)
