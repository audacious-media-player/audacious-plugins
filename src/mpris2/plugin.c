#include <stdio.h>

#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>

#include "object-core.h"
#include "object-player.h"

static GDBusConnection * bus;
static GObject * object_core, * object_player;

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

static void update_playback_status (void * data, GObject * object)
{
    const char * status;

    if (aud_drct_get_playing ())
        status = aud_drct_get_paused () ? "Paused" : "Playing";
    else
        status = "Stopped";

    g_object_set (object, "playback-status", status, NULL);
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
    if (! aud_drct_get_playing () || aud_drct_get_paused ())
        aud_drct_play ();

    mpris_media_player2_player_complete_play (object, call);
    return TRUE;
}

static bool_t play_pause_cb (MprisMediaPlayer2Player * object,
 GDBusMethodInvocation * call, void * unused)
{
    if (aud_drct_get_playing () && ! aud_drct_get_paused ())
        aud_drct_pause ();
    else
        aud_drct_play ();

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

    g_dbus_connection_close_sync (bus, NULL, NULL);
    g_object_unref (object_core);
    g_object_unref (object_player);
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
     "has-track-list", FALSE,
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
     "can-seek", FALSE,
     NULL);

    update_playback_status (NULL, object_player);

    hook_associate ("playback begin", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback pause", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback stop", (HookFunction) update_playback_status, object_player);
    hook_associate ("playback unpause", (HookFunction) update_playback_status, object_player);

    g_signal_connect (object_player, "handle-next", (GCallback) next_cb, NULL);
    g_signal_connect (object_player, "handle-pause", (GCallback) pause_cb, NULL);
    g_signal_connect (object_player, "handle-play", (GCallback) play_cb, NULL);
    g_signal_connect (object_player, "handle-play-pause", (GCallback) play_pause_cb, NULL);
    g_signal_connect (object_player, "handle-previous", (GCallback) previous_cb, NULL);
    g_signal_connect (object_player, "handle-stop", (GCallback) stop_cb, NULL);

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
    .name = "MPRIS2 Server",
    .init = mpris2_init,
    .cleanup = mpris2_cleanup
)
