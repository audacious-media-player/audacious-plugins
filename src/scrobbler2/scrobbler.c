/*
 * Scrobbler Plugin v2.0 for Audacious by Pitxyoki
 *
 * Copyright 2012-2013 Luís Picciochi Oliveira <Pitxyoki@Gmail.com>
 *
 * This plugin is part of the Audacious Media Player.
 * It is licensed under the GNU General Public License, version 3.
 */

//audacious includes
#include <audacious/plugin.h>
#include <audacious/drct.h>
#include <audacious/playlist.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>


//plugin includes
#include "scrobbler.h"


//shared variables
bool_t scrobbler_running        = TRUE;
bool_t migrate_config_requested = FALSE;
bool_t now_playing_requested    = FALSE;
Tuple *now_playing_track        = NULL;

pthread_mutex_t communication_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t communication_signal = PTHREAD_COND_INITIALIZER;
pthread_mutex_t log_access_mutex = PTHREAD_MUTEX_INITIALIZER;
gchar *session_key = NULL; /* pooled */
gchar *request_token = NULL; /* pooled */


//static (private) variables
static Tuple * playing_track       = NULL;
//all times are in microseconds
static  gint64 timestamp           = 0;
static  gint64 play_started_at     = 0;
static  gint64 pause_started_at    = 0;
static  gint64 time_until_scrobble = 0;
static   guint queue_function_ID   = 0;

static pthread_t communicator;

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

char *clean_string(char *string) {
    if (!string)
        return str_get("");

    SCOPY(temp, string);
    str_replace_char(temp, '\t', ' ');
    str_unref(string);
    return str_get(temp);
}

static gboolean queue_track_to_scrobble (gpointer data) {
    AUDDBG("The playing track is going to be ENQUEUED!\n.");

    char *queuepath = g_strconcat(aud_get_path(AUD_PATH_USER_DIR),"/scrobbler.log", NULL);

    char *artist = clean_string(tuple_get_str(playing_track, FIELD_ARTIST));
    char *title  = clean_string(tuple_get_str(playing_track, FIELD_TITLE));
    char *album  = clean_string(tuple_get_str(playing_track, FIELD_ALBUM));

    int track  = tuple_get_int(playing_track, FIELD_TRACK_NUMBER);
    int length = tuple_get_int(playing_track, FIELD_LENGTH);

    //artist, title and length are required for a successful scrobble
    if (artist[0] && title[0] && length > 0) {
        char *track_str = (track > 0) ? int_to_str(track) : str_get("");

        pthread_mutex_lock(&log_access_mutex);
        FILE *f = fopen(queuepath, "a");

        if (f == NULL) {
            perror("fopen");
        } else {
            //This isn't exactly the scrobbler.log format because the header
            //is missing, but we're sticking to it anyway...
            //See http://www.audioscrobbler.net/wiki/Portable_Player_Logging
            if (fprintf(f, "%s\t%s\t%s\t%s\t%i\tL\t%"G_GINT64_FORMAT"\n",
             artist, album, title, track_str, length / 1000, timestamp) < 0) {
                perror("fprintf");
            } else {
                pthread_mutex_lock(&communication_mutex);
                pthread_cond_signal(&communication_signal);
                pthread_mutex_unlock(&communication_mutex);
            }
            fclose(f);
        }
        pthread_mutex_unlock(&log_access_mutex);

        str_unref(track_str);
    }
    g_free(queuepath);
    str_unref(artist);
    str_unref(title);
    str_unref(album);

    cleanup_current_track();
    return FALSE;
}


static void stopped (void *hook_data, void *user_data) {
    // Called when pressing STOP and when the playlist ends.
    cleanup_current_track();
}

static void ended (void *hook_data, void *user_data) {
    //Called when when a track finishes playing.

    //TODO: hic sunt race conditions
    if (playing_track != NULL && (g_get_monotonic_time() > (play_started_at + 30*G_USEC_PER_SEC)) ) {
      //This is an odd situation when the track's real length doesn't correspond to the length reported by the player.
      //If we are at the end of the track, it is longer than 30 seconds and it wasn't scrobbled, we scrobble it by then.

      if (queue_function_ID != 0) {
        gboolean success = g_source_remove(queue_function_ID);
        queue_function_ID = 0;
        if (!success) {
          AUDDBG("BUG or race condition: Could not remove source.\n");
        } else {
          queue_track_to_scrobble(NULL);
        }
      }
    }

    cleanup_current_track();
}

static void ready (void *hook_data, void *user_data) {
    cleanup_current_track();

    Tuple *current_track = aud_playlist_entry_get_tuple(aud_playlist_get_playing(), aud_playlist_get_position(aud_playlist_get_playing()), FALSE);

    int duration_seconds = tuple_get_int(current_track, FIELD_LENGTH) / 1000;
    if (duration_seconds <= 30) {
        tuple_unref(current_track);
        return;
    }

    pthread_mutex_lock(&communication_mutex);
    now_playing_track = tuple_ref(current_track);
    now_playing_requested = TRUE;
    pthread_cond_signal(&communication_signal);
    pthread_mutex_unlock(&communication_mutex);

    time_until_scrobble = (((gint64)duration_seconds)*G_USEC_PER_SEC) / 2;
    if (time_until_scrobble > 4*60*G_USEC_PER_SEC) {
        time_until_scrobble = 4*60*G_USEC_PER_SEC;
    }
    timestamp = g_get_real_time() / G_USEC_PER_SEC;
    play_started_at = g_get_monotonic_time();
    playing_track = current_track;

    queue_function_ID = g_timeout_add_seconds(time_until_scrobble / G_USEC_PER_SEC, (GSourceFunc) queue_track_to_scrobble, NULL);

}

static void paused (void *hook_data, void *user_data) {
    if (playing_track == NULL) {
        //This happens when audacious is started in paused mode
        return;
    }

    gboolean success = g_source_remove(queue_function_ID);
    queue_function_ID = 0;
    if (!success) {
        AUDDBG("BUG: Could not remove source.\n");
        return;
    }

    pause_started_at = g_get_monotonic_time();
}

static void unpaused (void *hook_data, void *user_data) {

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
        aud_interface_show_error(_("The Scrobbler plugin could not be started.\n"
                                   "There might be a problem with your installation."));
        return FALSE;
    }

    session_key = aud_get_str("scrobbler", "session_key");
    if (!session_key[0])
        scrobbling_enabled = FALSE;

    //TODO: Remove this after we are "sure" that noone is using the old scrobbler (from audacious < 3.4)
    //By Debian's standard, this will probably be by 2020 or so
    //Migration from the old scrobbler config
    if (!session_key[0]) {
      //We haven't been configured yet

      char *migrated = aud_get_str("scrobbler", "migrated");
      if (strcmp(migrated, "true") != 0) {
        //We haven't been migrated yet

        char *oldpass = aud_get_str("audioscrobbler","password");
        if (oldpass[0]) {

          char *olduser = aud_get_str("audioscrobbler","username");
          if (olduser[0]) {
            //And the old scrobbler was configured

            scrobbling_enabled = FALSE;
            migrate_config_requested = TRUE;
          }
          str_unref(olduser);
        }
        str_unref(oldpass);
      }
      str_unref(migrated);
    }



    pthread_create(&communicator, NULL, scrobbling_thread, NULL);

    hook_associate("playback stop", (HookFunction) stopped, NULL);
    hook_associate("playback end", (HookFunction) ended, NULL);
    hook_associate("playback ready", (HookFunction) ready, NULL);
    hook_associate("playback pause", (HookFunction) paused, NULL);
    hook_associate("playback unpause", (HookFunction) unpaused, NULL);
    return TRUE;
}

static void scrobbler_cleanup (void) {

    hook_dissociate("playback stop", (HookFunction) stopped);
    hook_dissociate("playback end", (HookFunction) ended);
    hook_dissociate("playback ready", (HookFunction) ready);
    hook_dissociate("playback pause", (HookFunction) paused);
    hook_dissociate("playback unpause", (HookFunction) unpaused);

    cleanup_current_track();

    scrobbling_enabled = FALSE;
    scrobbler_running  = FALSE;
    pthread_mutex_lock(&communication_mutex);
    pthread_cond_signal(&communication_signal);
    pthread_mutex_unlock(&communication_mutex);

    pthread_join(communicator, NULL);

    str_unref(request_token);
    str_unref(session_key);
    str_unref(username);
    request_token = NULL;
    session_key   = NULL;
    username      = NULL;
    scrobbler_running = TRUE;
}

static const char scrobbler_about[] =
 N_("Audacious Scrobbler Plugin 2.0 by Pitxyoki,\n\n"
    "Copyright © 2012-2013 Luís M. Picciochi Oliveira <Pitxyoki@Gmail.com>\n\n"
    "Thanks to John Lindgren for giving me a hand at the beginning of this project.\n\n");


AUD_GENERAL_PLUGIN (
    .name = N_("Scrobbler 2.0"),
    .domain = PACKAGE,
    .about_text = scrobbler_about,
    .init = scrobbler_init,
    .cleanup = scrobbler_cleanup,
    .prefs = &configuration //see config_window.c
)
