
//plugin includes
#include "scrobbler.h"//needed

//audacious includes
#include <config.h>                //needed
#include <audacious/plugin.h>      //needed
#include <audacious/drct.h> //needed
#include <audacious/playlist.h>    //needed
#include <libaudcore/hook.h>       //needed

//external includes


//shared variables
GMutex *communication_mutex;
GCond  *communication_signal;
GMutex *log_access_mutex;
gchar *session_key;
gchar *request_token;


//static (private) variables
static Tuple * playing_track       = NULL;
static  gint64 timestamp           = 0;
static  gint64 play_started_at     = 0;
static  gint64 pause_started_at    = 0;
static  gint64 time_until_scrobble = 0;
static   guint queue_function_ID   = 0;


static void cleanup_current_track(void) {

    timestamp = 0;
    play_started_at = 0;
    pause_started_at = 0;
    time_until_scrobble = 0;
    if (queue_function_ID != 0) {
        gboolean success = g_source_remove(queue_function_ID);
        queue_function_ID = 0;
        if (!success) {
            AUDDBG("BUG: No success on g_source_remove!\n");
        }
    }
    if (playing_track != NULL) {
        tuple_unref(playing_track);
        playing_track = NULL;
    }
}

gboolean queue_track_to_scrobble (gpointer data) {
    AUDDBG("The playing track is going to be ENQUEUED!\n.");

    char *queuepath = g_strconcat(aud_get_path(AUD_PATH_USER_DIR),"/scrobbler.log", NULL);

    char *artist = tuple_get_str(playing_track, FIELD_ARTIST, NULL);
    char *title  = tuple_get_str(playing_track, FIELD_TITLE, NULL);
    char *album  = tuple_get_str(playing_track, FIELD_ALBUM, NULL);
    int number = tuple_get_int(playing_track, FIELD_TRACK_NUMBER, NULL);
    int length = tuple_get_int(playing_track, FIELD_LENGTH, NULL) / 1000;

    //artist, title and timestamp are required for a successful scrobble
    if (artist != NULL && title != NULL) {
        g_mutex_lock(log_access_mutex);
        FILE *f = fopen(queuepath, "a");

        if (f == NULL) {
            perror("fopen");
        } else {
            //This isn't exactly the scrobbler.log format because the header
            //is missing, but we're sticking to it anyway...
            if (fprintf(f, "%s\t%s\t%s\t%i\t%i\t%s\t%li\n",
                        artist, (album == NULL ? "" : album), title, number, length, "L", timestamp
                       ) < 0) {
                perror("fprintf");
            } else {
                g_mutex_lock(communication_mutex);
                g_cond_signal(communication_signal);
                g_mutex_unlock(communication_mutex);
            }
            fclose(f);
        }
        g_mutex_unlock(log_access_mutex);
    }
    g_free(queuepath);
    str_unref(artist);
    str_unref(title);
    str_unref(album);

    cleanup_current_track();
    return FALSE;
}


static void stopped (void *hook_data, void *user_data) {
    printf("Playback stopped!\n");
    // Called when pressing STOP and when the playlist ends.

    cleanup_current_track();
}

static void ended (void *hook_data, void *user_data) {
    printf("Playback ended!\n");
    //Called when when a track finishes playing.

    //TODO: probably we can also enqueue here to workaround tracks with wrong lengths
    cleanup_current_track();
}

static void ready (void *hook_data, void *user_data) {
    printf("Playback ready!\n");

    Tuple *current_track = aud_playlist_entry_get_tuple(aud_playlist_get_playing(), aud_playlist_get_position(aud_playlist_get_playing()), FALSE);

    int duration_seconds = tuple_get_int(current_track, FIELD_LENGTH, NULL) / 1000;
    if (duration_seconds <= 30) {
        tuple_unref(current_track);
        return;
    }

    time_until_scrobble = (duration_seconds*G_USEC_PER_SEC) / 2;
    if (time_until_scrobble > 4*60*G_USEC_PER_SEC) {
        time_until_scrobble = 4*60*G_USEC_PER_SEC;
    }
    timestamp = g_get_real_time() / G_USEC_PER_SEC;
    play_started_at = g_get_monotonic_time();
    playing_track = current_track;

    queue_function_ID = g_timeout_add_seconds(time_until_scrobble / G_USEC_PER_SEC, (GSourceFunc) queue_track_to_scrobble, NULL);

}

static void paused (void *hook_data, void *user_data) {
    printf("Playback paused!\n");
    if (playing_track == NULL) {
        return;
    }

    gboolean success = g_source_remove(queue_function_ID);
    if (!success) {
        AUDDBG("BUG: Could not remove source.\n");
        return;
    }

    pause_started_at = g_get_monotonic_time();

}

static void unpaused (void *hook_data, void *user_data) {
    printf("playback unpaused! %li, %li, %li\n", time_until_scrobble, pause_started_at, play_started_at);

    if (playing_track == NULL
        || pause_started_at == 0) { //TODO: audacious was started with a paused track.
        return;
    }
    time_until_scrobble = time_until_scrobble - (pause_started_at - play_started_at);

    queue_function_ID = g_timeout_add_seconds(time_until_scrobble / G_USEC_PER_SEC, (GSourceFunc) queue_track_to_scrobble, NULL);

    pause_started_at = 0;
    play_started_at = g_get_monotonic_time();
}

static bool_t scrobbler_init (void) {
    // Initialize libXML and check potential ABI mismatches between
    // the version it was compiled for and the actual libXML in use
    LIBXML_TEST_VERSION

    if (scrobbler_communication_init() == FALSE) {
        aud_interface_show_error("The Scrobbler plugin could not be started.\n"
                "There might be a problem with your installation.");
        return FALSE;
    }

    session_key = aud_get_string("scrobbler", "session_key");
    request_token = NULL;

    //TODO: replace with g_mutex_init(communication_mutex) from glib 2.32
    log_access_mutex = g_mutex_new();
    communication_mutex = g_mutex_new();
    permission_check_mutex = g_mutex_new();

    //TODO: replace with g_cond_init
    communication_signal = g_cond_new();
    permission_check_signal = g_cond_new();


    //TODO: g_thread_create should be updated to g_thread_new() from glib 2.32
    g_thread_create(scrobbling_thread, NULL, FALSE, NULL);

    hook_associate("playback stop", (HookFunction) stopped, NULL);
    hook_associate("playback end", (HookFunction) ended, NULL);
    hook_associate("playback ready", (HookFunction) ready, NULL);
    hook_associate("playback pause", (HookFunction) paused, NULL);
    hook_associate("playback unpause", (HookFunction) unpaused, NULL);
    return TRUE;
}

static void scrobbler_cleanup (void) {
    //TODO: Do we want anything else here?

    hook_dissociate("playback stop", (HookFunction) stopped);
    hook_dissociate("playback end", (HookFunction) ended);
    hook_dissociate("playback ready", (HookFunction) ready);
    hook_dissociate("playback pause", (HookFunction) paused);
    hook_dissociate("playback unpause", (HookFunction) unpaused);
}

static const char scrobbler_about[] =
 "Audacious Scrobbling Plugin 2.0 by Pitxyoki,\n\n"
 "Copyright © 2012 Luís Picciochi <Pitxyoki@Gmail.com>.\n\n"
 "Thanks to John Lindgren for giving me a hand at the beginning of this project.\n\n";


AUD_GENERAL_PLUGIN (
    .name = N_("Scrobbler 2.0"),
    .domain = PACKAGE,
    .about_text = scrobbler_about,
    .init = scrobbler_init,
    .cleanup = scrobbler_cleanup,
    .prefs = &configuration //see config_window.c
)
