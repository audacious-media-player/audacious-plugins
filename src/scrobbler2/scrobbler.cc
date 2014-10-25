/*
 * Scrobbler Plugin v2.0 for Audacious by Pitxyoki
 *
 * Copyright 2012-2013 Luís Picciochi Oliveira <Pitxyoki@Gmail.com>
 *
 * This plugin is part of the Audacious Media Player.
 * It is licensed under the GNU General Public License, version 3.
 */

#include <glib/gstdio.h>

//audacious includes
#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>


//plugin includes
#include "scrobbler.h"


//shared variables
gboolean scrobbler_running        = TRUE;
gboolean migrate_config_requested = FALSE;
gboolean now_playing_requested    = FALSE;
Tuple now_playing_track;

pthread_mutex_t communication_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t communication_signal = PTHREAD_COND_INITIALIZER;
pthread_mutex_t log_access_mutex = PTHREAD_MUTEX_INITIALIZER;
String session_key;
String request_token;


//static (private) variables
static Tuple playing_track;
//all times are in microseconds
static  int64_t timestamp           = 0;
static  int64_t play_started_at     = 0;
static  int64_t pause_started_at    = 0;
static  int64_t time_until_scrobble = 0;
static   unsigned queue_function_ID   = 0;

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
    playing_track = Tuple ();
}

StringBuf clean_string (const char *string) {
    StringBuf temp = str_copy (string ? string : "");
    str_replace_char (temp, '\t', ' ');
    return temp;
}

static gboolean queue_track_to_scrobble (void * data) {
    AUDDBG("The playing track is going to be ENQUEUED!\n.");

    char *queuepath = g_strconcat(aud_get_path(AudPath::UserDir),"/scrobbler.log", nullptr);

    StringBuf artist = clean_string (playing_track.get_str (Tuple::Artist));
    StringBuf title  = clean_string (playing_track.get_str (Tuple::Title));
    StringBuf album  = clean_string (playing_track.get_str (Tuple::Album));

    int track  = playing_track.get_int (Tuple::Track);
    int length = playing_track.get_int (Tuple::Length);

    //artist, title and length are required for a successful scrobble
    if (artist[0] && title[0] && length > 0) {
        StringBuf track_str = (track > 0) ? int_to_str (track) : StringBuf (0);

        pthread_mutex_lock(&log_access_mutex);
        FILE *f = g_fopen(queuepath, "a");

        if (f == nullptr) {
            perror("fopen");
        } else {
            //This isn't exactly the scrobbler.log format because the header
            //is missing, but we're sticking to it anyway...
            //See http://www.audioscrobbler.net/wiki/Portable_Player_Logging
            if (fprintf(f, "%s\t%s\t%s\t%s\t%i\tL\t%" G_GINT64_FORMAT "\n",
             (const char *)artist, (const char *)album, (const char *)title,
             (const char *)track_str, length / 1000, timestamp) < 0) {
                perror("fprintf");
            } else {
                pthread_mutex_lock(&communication_mutex);
                pthread_cond_signal(&communication_signal);
                pthread_mutex_unlock(&communication_mutex);
            }
            fclose(f);
        }
        pthread_mutex_unlock(&log_access_mutex);
    }
    g_free(queuepath);
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
    if (playing_track && (g_get_monotonic_time() > (play_started_at + 30*G_USEC_PER_SEC)) ) {
      //This is an odd situation when the track's real length doesn't correspond to the length reported by the player.
      //If we are at the end of the track, it is longer than 30 seconds and it wasn't scrobbled, we scrobble it by then.

      if (queue_function_ID != 0) {
        gboolean success = g_source_remove(queue_function_ID);
        queue_function_ID = 0;
        if (!success) {
          AUDDBG("BUG or race condition: Could not remove source.\n");
        } else {
          queue_track_to_scrobble(nullptr);
        }
      }
    }

    cleanup_current_track();
}

static void ready (void *hook_data, void *user_data) {
    cleanup_current_track();

    int playlist = aud_playlist_get_playing ();
    int position = aud_playlist_get_position (playlist);
    Tuple current_track = aud_playlist_entry_get_tuple (playlist, position);

    int duration_seconds = current_track.get_int (Tuple::Length) / 1000;
    if (duration_seconds <= 30)
        return;

    pthread_mutex_lock(&communication_mutex);
    now_playing_track = current_track.ref ();
    now_playing_requested = TRUE;
    pthread_cond_signal(&communication_signal);
    pthread_mutex_unlock(&communication_mutex);

    time_until_scrobble = (((int64_t)duration_seconds)*G_USEC_PER_SEC) / 2;
    if (time_until_scrobble > 4*60*G_USEC_PER_SEC) {
        time_until_scrobble = 4*60*G_USEC_PER_SEC;
    }
    timestamp = g_get_real_time() / G_USEC_PER_SEC;
    play_started_at = g_get_monotonic_time();
    playing_track = std::move (current_track);

    queue_function_ID = g_timeout_add_seconds(time_until_scrobble / G_USEC_PER_SEC, (GSourceFunc) queue_track_to_scrobble, nullptr);

}

static void paused (void *hook_data, void *user_data) {
    if (! playing_track) {
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

    if (! playing_track
        || pause_started_at == 0) { //TODO: audacious was started with a paused track.
        return;
    }
    time_until_scrobble = time_until_scrobble - (pause_started_at - play_started_at);

    queue_function_ID = g_timeout_add_seconds(time_until_scrobble / G_USEC_PER_SEC, (GSourceFunc) queue_track_to_scrobble, nullptr);

    pause_started_at = 0;
    play_started_at = g_get_monotonic_time();
}



static bool scrobbler_init (void) {
    // Initialize libXML and check potential ABI mismatches between
    // the version it was compiled for and the actual libXML in use
    LIBXML_TEST_VERSION

    if (scrobbler_communication_init() == FALSE) {
        aud_ui_show_error(_("The Scrobbler plugin could not be started.\n"
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

      String migrated = aud_get_str("scrobbler", "migrated");
      if (strcmp(migrated, "true") != 0) {
        //We haven't been migrated yet

        String oldpass = aud_get_str("audioscrobbler","password");
        String olduser = aud_get_str("audioscrobbler","username");
        if (oldpass[0] && olduser[0]) {
          //And the old scrobbler was configured

          scrobbling_enabled = FALSE;
          migrate_config_requested = TRUE;
        }
      }
    }

    pthread_create(&communicator, nullptr, scrobbling_thread, nullptr);

    hook_associate("playback stop", (HookFunction) stopped, nullptr);
    hook_associate("playback end", (HookFunction) ended, nullptr);
    hook_associate("playback ready", (HookFunction) ready, nullptr);
    hook_associate("playback pause", (HookFunction) paused, nullptr);
    hook_associate("playback unpause", (HookFunction) unpaused, nullptr);
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

    pthread_join(communicator, nullptr);

    request_token = String();
    session_key = String();
    username = String();
    scrobbler_running = TRUE;
}

static const char scrobbler_about[] =
 N_("Audacious Scrobbler Plugin 2.0 by Pitxyoki,\n\n"
    "Copyright © 2012-2013 Luís M. Picciochi Oliveira <Pitxyoki@Gmail.com>\n\n"
    "Thanks to John Lindgren for giving me a hand at the beginning of this project.\n\n");

#define AUD_PLUGIN_NAME        N_("Scrobbler 2.0")
#define AUD_PLUGIN_ABOUT       scrobbler_about
#define AUD_PLUGIN_INIT        scrobbler_init
#define AUD_PLUGIN_CLEANUP     scrobbler_cleanup
#define AUD_PLUGIN_PREFS       &configuration

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>
