/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "i_backend.h"
#include "i_common.h"
#include "i_configure.h"
#include "i_fileinfo.h"
#include "i_midi.h"
#include "i_utils.h"

enum
{
    AMIDIPLUG_STOP,
    AMIDIPLUG_PLAY,
    AMIDIPLUG_PAUSE,
    AMIDIPLUG_ERR
};

static void amidiplug_play_loop (InputPlayback * playback);

static bool_t amidiplug_play (InputPlayback * playback, const char *
                              filename_uri, VFSFile * file, int start_time, int stop_time, bool_t pause);
static void amidiplug_pause (InputPlayback * playback, bool_t paused);
static void amidiplug_mseek (InputPlayback * playback, int time);
static Tuple * amidiplug_get_song_tuple (const char * filename_uri, VFSFile * file);
static void amidiplug_skipto (int playing_tick);

static int amidiplug_playing_status = AMIDIPLUG_STOP;

static midifile_t midifile;

static const char * const amidiplug_vfs_extensions[] = {"mid", "midi", "rmi",
        "rmid", NULL
                                                       };

static pthread_mutex_t control_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t control_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t audio_control_mutex = PTHREAD_MUTEX_INITIALIZER;

/* also used in i_configure.c */
amidiplug_sequencer_backend_t * backend;

static int seek_time;

static pthread_t audio_thread;
static bool_t audio_stop_flag;
static int audio_seek_time;

static void amidiplug_cleanup (void)
{
    if (backend)
        i_backend_unload (backend);

    i_configure_cfg_ap_free ();
    i_configure_cfg_backend_free ();
}

static bool_t amidiplug_init (void)
{
    i_configure_cfg_ap_read ();
    i_configure_cfg_backend_read ();

    backend = i_backend_load ();

    if (! backend)
    {
        amidiplug_cleanup ();
        return FALSE;
    }

    return TRUE;
}

static int amidiplug_is_our_file_from_vfs (const char * filename_uri, VFSFile * fp)
{
    char magic_bytes[4];

    if (fp == NULL)
        return FALSE;

    if (vfs_fread (magic_bytes, 1, 4, fp) != 4)
        return FALSE;

    if (!strncmp (magic_bytes, "MThd", 4))
    {
        DEBUGMSG ("MIDI found, %s is a standard midi file\n", filename_uri);
        return TRUE;
    }

    if (!strncmp (magic_bytes, "RIFF", 4))
    {
        /* skip the four bytes after RIFF,
           then read the next four */
        if (vfs_fseek (fp, 4, SEEK_CUR) != 0)
            return FALSE;

        if (vfs_fread (magic_bytes, 1, 4, fp) != 4)
            return FALSE;

        if (!strncmp (magic_bytes, "RMID", 4))
        {
            DEBUGMSG ("MIDI found, %s is a riff midi file\n", filename_uri);
            return TRUE;
        }
    }

    return FALSE;
}

static void amidiplug_stop (InputPlayback * playback)
{
    DEBUGMSG ("STOP request at tick: %i\n", midifile.playing_tick);

    pthread_mutex_lock (& control_mutex);
    amidiplug_playing_status = AMIDIPLUG_STOP;
    pthread_cond_broadcast (& control_cond);
    pthread_mutex_unlock (& control_mutex);
}

static void amidiplug_pause (InputPlayback * playback, bool_t paused)
{
    pthread_mutex_lock (& control_mutex);
    amidiplug_playing_status = paused ? AMIDIPLUG_PAUSE : AMIDIPLUG_PLAY;
    pthread_cond_broadcast (& control_cond);
    pthread_mutex_unlock (& control_mutex);
}

static void amidiplug_mseek (InputPlayback * playback, int time)
{
    pthread_mutex_lock (& control_mutex);
    seek_time = time;
    pthread_cond_broadcast (& control_cond);
    pthread_mutex_unlock (& control_mutex);
}

static Tuple * amidiplug_get_song_tuple (const char * filename_uri, VFSFile *
        file)
{
    /* song title, get it from the filename */
    Tuple * tuple = tuple_new_from_filename (filename_uri);
    midifile_t mf;

    if (i_midi_parse_from_filename (filename_uri, &mf))
        tuple_set_int (tuple, FIELD_LENGTH, NULL, mf.length / 1000);

    i_midi_free (&mf);

    return tuple;
}


static bool_t amidiplug_play (InputPlayback * playback, const char *
                              filename_uri, VFSFile * file, int start_time, int stop_time, bool_t pause)
{
    int port_count = 0;
    int au_samplerate = -1, au_bitdepth = -1, au_channels = -1;

    /* get information about audio from backend, if available */
    backend->audio_info_get (&au_channels, &au_bitdepth, &au_samplerate);
    DEBUGMSG ("PLAY requested, audio details: channels -> %i, bitdepth -> %i, samplerate -> %i\n",
              au_channels, au_bitdepth, au_samplerate);

    playback->output->open_audio (FMT_S16_NE, au_samplerate, au_channels);

    DEBUGMSG ("PLAY requested, midifile init\n");
    /* midifile init */
    i_midi_init (&midifile);

    DEBUGMSG ("PLAY requested, opening file: %s\n", filename_uri);
    midifile.file_pointer = file;
    midifile.file_name = strdup (filename_uri);

    switch (i_midi_file_read_id (&midifile))
    {
    case MAKE_ID ('R', 'I', 'F', 'F') :
    {
        DEBUGMSG ("PLAY requested, RIFF chunk found, processing...\n");

        /* read riff chunk */
        if (!i_midi_file_parse_riff (&midifile))
            WARNANDBREAKANDPLAYERR ("%s: invalid file format (riff parser)\n", filename_uri);

        /* if that was read correctly, go ahead and read smf data */
    }

    case MAKE_ID ('M', 'T', 'h', 'd') :
    {
        DEBUGMSG ("PLAY requested, MThd chunk found, processing...\n");

        if (!i_midi_file_parse_smf (&midifile, port_count))
            WARNANDBREAKANDPLAYERR ("%s: invalid file format (smf parser)\n", filename_uri);

        if (midifile.time_division < 1)
            WARNANDBREAKANDPLAYERR ("%s: invalid time division (%i)\n", filename_uri, midifile.time_division);

        DEBUGMSG ("PLAY requested, setting ppq and tempo...\n");

        /* fill midifile.ppq and midifile.tempo using time_division */
        if (!i_midi_setget_tempo (&midifile))
            WARNANDBREAKANDPLAYERR ("%s: invalid values while setting ppq and tempo\n", filename_uri);

        DEBUGMSG ("PLAY requested, sequencer start\n");

        /* sequencer start */
        if (!backend->seq_start (filename_uri))
            WARNANDBREAKANDPLAYERR ("%s: problem with seq_start, play aborted\n", filename_uri);

        DEBUGMSG ("PLAY requested, sequencer on\n");

        /* sequencer on */
        if (!backend->seq_on())
            WARNANDBREAKANDPLAYERR ("%s: problem with seq_on, play aborted\n", filename_uri);

        DEBUGMSG ("PLAY requested, setting sequencer queue tempo...\n");

        /* set sequencer queue tempo using ppq and tempo (call only after i_midi_setget_tempo) */
        if (!backend->seq_queue_tempo (midifile.current_tempo, midifile.ppq))
        {
            backend->seq_off(); /* kill the sequencer */
            WARNANDBREAKANDPLAYERR ("%s: ALSA queue problem, play aborted\n", filename_uri);
        }

        /* fill midifile.length, keeping in count tempo-changes */
        i_midi_setget_length (&midifile);
        DEBUGMSG ("PLAY requested, song length calculated: %i msec\n", (int) (midifile.length / 1000));

        playback->set_params (playback, au_bitdepth * au_samplerate * au_channels
                              / 8, au_samplerate, au_channels);

        /* done with file */
        midifile.file_pointer = NULL;

        /* play play play! */
        DEBUGMSG ("PLAY requested, starting play thread\n");
        amidiplug_playing_status = pause ? AMIDIPLUG_PAUSE : AMIDIPLUG_PLAY;
        seek_time = (start_time > 0) ? start_time : -1;
        playback->set_pb_ready (playback);
        amidiplug_play_loop (playback);
        break;
    }

    default:
    {
        amidiplug_playing_status = AMIDIPLUG_ERR;
        fprintf (stderr, "%s is not a Standard MIDI File\n", filename_uri);
        break;
    }
    }

    return TRUE;
}

static void * audio_loop (void * arg)
{
    InputPlayback * playback = arg;
    void * buffer = NULL;
    int buffer_size = 0;

    while (1)
    {
        pthread_mutex_lock (& audio_control_mutex);

        if (audio_stop_flag)
        {
            pthread_mutex_unlock (& audio_control_mutex);
            break;
        }

        if (audio_seek_time != -1)
        {
            playback->output->flush (audio_seek_time);
            audio_seek_time = -1;
        }

        pthread_mutex_unlock (& audio_control_mutex);

        if (backend->seq_output (& buffer, & buffer_size))
            playback->output->write_audio (buffer, buffer_size);
    }

    free (buffer);
    return NULL;
}

static void audio_start (InputPlayback * playback)
{
    audio_stop_flag = FALSE;
    audio_seek_time = -1;
    pthread_create (& audio_thread, NULL, audio_loop, (void *) playback);
}

static void audio_seek (InputPlayback * playback, int time)
{
    pthread_mutex_lock (& audio_control_mutex);
    audio_seek_time = time;
    playback->output->abort_write ();
    pthread_mutex_unlock (& audio_control_mutex);
}

static void audio_pause (InputPlayback * playback, bool_t pause)
{
    playback->output->pause (pause);
}

static void audio_stop (InputPlayback * playback)
{
    pthread_mutex_lock (& audio_control_mutex);
    audio_stop_flag = TRUE;
    playback->output->abort_write ();
    pthread_mutex_unlock (& audio_control_mutex);
    pthread_join (audio_thread, NULL);
}

static void do_seek (int time)
{
    backend->seq_event_allnoteoff (midifile.playing_tick);
    backend->seq_queue_stop ();
    amidiplug_skipto (time * (int64_t) 1000 / midifile.avg_microsec_per_tick);
}

static void do_pause (bool_t pause)
{
    if (pause)
    {
        backend->seq_event_allnoteoff (midifile.playing_tick);
        backend->seq_queue_stop ();
    }
    else
    {
        midifile.skip_offset = midifile.playing_tick;
        backend->seq_queue_start ();
    }
}

static void amidiplug_play_loop (InputPlayback * playback)
{
    int j = 0;
    bool_t rewind = TRUE, paused = FALSE, stopped = FALSE;

    if (rewind)
    {
        /* initialize current position in each track */
        for (j = 0; j < midifile.num_tracks; ++j)
            midifile.tracks[j].current_event = midifile.tracks[j].first_event;
    }

    audio_start (playback);

    /* queue start */
    backend->seq_queue_start();
    /* common settings for all our events */
    backend->seq_event_init();

    DEBUGMSG ("PLAY thread, start the play loop\n");

    for (;;)
    {
        midievent_t * event = NULL;
        midifile_track_t * event_track = NULL;
        int i, min_tick = midifile.max_tick + 1;

        pthread_mutex_lock (& control_mutex);

        if (amidiplug_playing_status == AMIDIPLUG_STOP)
        {
            pthread_mutex_unlock (& control_mutex);
            stopped = TRUE;
            break;
        }

        if (seek_time != -1)
        {
            audio_seek (playback, seek_time);

            do_seek (seek_time);
            seek_time = -1;
        }

        if (amidiplug_playing_status == AMIDIPLUG_PAUSE)
        {
            if (! paused)
            {
                do_pause (TRUE);

                audio_pause (playback, TRUE);

                paused = TRUE;
            }

            pthread_cond_wait (& control_cond, & control_mutex);
            pthread_mutex_unlock (& control_mutex);
            continue;
        }

        if (paused)
        {
            do_pause (FALSE);

            audio_pause (playback, FALSE);

            paused = FALSE;
        }

        pthread_mutex_unlock (& control_mutex);

        /* search next event */
        for (i = 0; i < midifile.num_tracks; ++i)
        {
            midifile_track_t * track = &midifile.tracks[i];
            midievent_t * e2 = track->current_event;

            if ((e2) && (e2->tick < min_tick))
            {
                min_tick = e2->tick;
                event = e2;
                event_track = track;
            }
        }

        if (!event)
            break; /* end of song reached */

        /* advance pointer to next event */
        event_track->current_event = event->next;
        /* consider the midifile.skip_offset */
        event->tick_real = event->tick - midifile.skip_offset;


        switch (event->type)
        {
        case SND_SEQ_EVENT_NOTEON:
            backend->seq_event_noteon (event);
            break;

        case SND_SEQ_EVENT_NOTEOFF:
            backend->seq_event_noteoff (event);
            break;

        case SND_SEQ_EVENT_KEYPRESS:
            backend->seq_event_keypress (event);
            break;

        case SND_SEQ_EVENT_CONTROLLER:
            backend->seq_event_controller (event);
            break;

        case SND_SEQ_EVENT_PGMCHANGE:
            backend->seq_event_pgmchange (event);
            break;

        case SND_SEQ_EVENT_CHANPRESS:
            backend->seq_event_chanpress (event);
            break;

        case SND_SEQ_EVENT_PITCHBEND:
            backend->seq_event_pitchbend (event);
            break;

        case SND_SEQ_EVENT_SYSEX:
            backend->seq_event_sysex (event);
            break;

        case SND_SEQ_EVENT_TEMPO:
            backend->seq_event_tempo (event);
            DEBUGMSG ("PLAY thread, processing tempo event with value %i on tick %i\n",
                      event->data.tempo, event->tick);
            midifile.current_tempo = event->data.tempo;
            break;

        case SND_SEQ_EVENT_META_TEXT:
            /* do nothing */
            break;

        case SND_SEQ_EVENT_META_LYRIC:
            /* do nothing */
            break;

        default:
            DEBUGMSG ("PLAY thread, encountered invalid event type %i\n", event->type);
            break;
        }

        midifile.playing_tick = event->tick;
    }

    backend->seq_output_shut (stopped ? midifile.playing_tick : midifile.max_tick,
                              midifile.skip_offset);

    audio_stop (playback);

    backend->seq_off ();
    backend->seq_stop ();
    i_midi_free (& midifile);
}


/* amidigplug_skipto: re-do all events that influence the playing of our
   midi file; re-do them using a time-tick of 0, so they are processed
   istantaneously and proceed this way until the playing_tick is reached;
   also obtain the correct skip_offset from the playing_tick */
static void amidiplug_skipto (int playing_tick)
{
    int i;

    /* this check is always made, for safety*/
    if (playing_tick >= midifile.max_tick)
        playing_tick = midifile.max_tick - 1;

    /* initialize current position in each track */
    for (i = 0; i < midifile.num_tracks; ++i)
        midifile.tracks[i].current_event = midifile.tracks[i].first_event;

    /* common settings for all our events */
    backend->seq_event_init();
    backend->seq_queue_start();

    DEBUGMSG ("SKIPTO request, starting skipto loop\n");

    for (;;)
    {
        midievent_t * event = NULL;
        midifile_track_t * event_track = NULL;
        int i, min_tick = midifile.max_tick + 1;

        /* search next event */
        for (i = 0; i < midifile.num_tracks; ++i)
        {
            midifile_track_t * track = &midifile.tracks[i];
            midievent_t * e2 = track->current_event;

            if ((e2) && (e2->tick < min_tick))
            {
                min_tick = e2->tick;
                event = e2;
                event_track = track;
            }
        }

        /* unlikely here... unless very strange MIDI files are played :) */
        if (!event)
        {
            DEBUGMSG ("SKIPTO request, reached the last event but not the requested tick (!)\n");
            break; /* end of song reached */
        }

        /* reached the requested tick, job done */
        if (event->tick >= playing_tick)
        {
            DEBUGMSG ("SKIPTO request, reached the requested tick, exiting from skipto loop\n");
            break;
        }

        /* advance pointer to next event */
        event_track->current_event = event->next;
        /* set the time tick to 0 */
        event->tick_real = 0;

        switch (event->type)
        {
            /* do nothing for these
            case SND_SEQ_EVENT_NOTEON:
            case SND_SEQ_EVENT_NOTEOFF:
            case SND_SEQ_EVENT_KEYPRESS:
            {
              break;
            } */
        case SND_SEQ_EVENT_CONTROLLER:
            backend->seq_event_controller (event);
            break;

        case SND_SEQ_EVENT_PGMCHANGE:
            backend->seq_event_pgmchange (event);
            break;

        case SND_SEQ_EVENT_CHANPRESS:
            backend->seq_event_chanpress (event);
            break;

        case SND_SEQ_EVENT_PITCHBEND:
            backend->seq_event_pitchbend (event);
            break;

        case SND_SEQ_EVENT_SYSEX:
            backend->seq_event_sysex (event);
            break;

        case SND_SEQ_EVENT_TEMPO:
            backend->seq_event_tempo (event);
            midifile.current_tempo = event->data.tempo;
            break;
        }
    }

    midifile.skip_offset = playing_tick;
    midifile.playing_tick = playing_tick;
}


AUD_INPUT_PLUGIN
(
    .name = N_("AMIDI-Plug (MIDI Player)"),
    .domain = PACKAGE,
    .init = amidiplug_init,
    .about = i_about_gui,
    .configure = i_configure_gui,
    .play = amidiplug_play,
    .stop = amidiplug_stop,
    .pause = amidiplug_pause,
    .mseek = amidiplug_mseek,
    .cleanup = amidiplug_cleanup,
    .probe_for_tuple = amidiplug_get_song_tuple,
    .file_info_box = i_fileinfo_gui,
    .is_our_file_from_vfs = amidiplug_is_our_file_from_vfs,
    .extensions = amidiplug_vfs_extensions,
)
