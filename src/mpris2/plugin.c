#include <stdio.h>

#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "object-core.h"

static GDBusConnection * bus;
static GObject * object_core;

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

    if (! g_dbus_interface_skeleton_export ((GDBusInterfaceSkeleton *)
     object_core, bus, "/org/mpris/MediaPlayer2", & error))
    {
        g_dbus_connection_close_sync (bus, NULL, NULL);
        fprintf (stderr, "mpris2: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    return TRUE;
}

void mpris2_cleanup (void)
{
    g_dbus_connection_close_sync (bus, NULL, NULL);
    g_object_unref (object_core);
}

AUD_GENERAL_PLUGIN
(
    .name = "MPRIS2 Server",
    .init = mpris2_init,
    .cleanup = mpris2_cleanup
)
