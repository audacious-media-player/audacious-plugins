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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/input.h>
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
    AMIDIPLUG_ERR
};

static void amidiplug_play_loop (void);

static bool_t amidiplug_play (const char * filename_uri, VFSFile * file);
static Tuple * amidiplug_get_song_tuple (const char * filename_uri, VFSFile * file);
static void amidiplug_skipto (int playing_tick);

static int amidiplug_playing_status = AMIDIPLUG_STOP;

static midifile_t midifile;

static const char * const amidiplug_vfs_extensions[] = {"mid", "midi", "rmi",
        "rmid", NULL
                                                       };

/* also used in i_configure.c */
amidiplug_sequencer_backend_t * backend;

static void amidiplug_cleanup (void)
{
    if (backend)
        i_backend_unload (backend);

    i_configure_cfg_ap_free ();
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


static int s_samplerate, s_channels;
static int s_bufsize;
static void * s_buf;

static bool_t audio_init (void)
{
    int bitdepth;

    backend->audio_info_get (& s_channels, & bitdepth, & s_samplerate);

    if (bitdepth != 16 || ! aud_input_open_audio (FMT_S16_NE, s_samplerate, s_channels))
        return FALSE;

    s_bufsize = 2 * s_channels * (s_samplerate / 4);
    s_buf = malloc (s_bufsize);

    return TRUE;
}

static void audio_generate (double seconds)
{
    int total = 2 * s_channels * (int) round (seconds * s_samplerate);

    while (total)
    {
        int chunk = (total < s_bufsize) ? total : s_bufsize;

        backend->generate_audio (s_buf, chunk);
        aud_input_write_audio (s_buf, chunk);

        total -= chunk;
    }
}

static void audio_cleanup (void)
{
    free (s_buf);
}

static bool_t amidiplug_play (const char * filename_uri, VFSFile * file)
{
    if (! audio_init ())
        return FALSE;

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

        if (!i_midi_file_parse_smf (&midifile, 1))
            WARNANDBREAKANDPLAYERR ("%s: invalid file format (smf parser)\n", filename_uri);

        if (midifile.time_division < 1)
            WARNANDBREAKANDPLAYERR ("%s: invalid time division (%i)\n", filename_uri, midifile.time_division);

        DEBUGMSG ("PLAY requested, setting ppq and tempo...\n");

        /* fill midifile.ppq and midifile.tempo using time_division */
        if (!i_midi_setget_tempo (&midifile))
            WARNANDBREAKANDPLAYERR ("%s: invalid values while setting ppq and tempo\n", filename_uri);

        /* fill midifile.length, keeping in count tempo-changes */
        i_midi_setget_length (&midifile);
        DEBUGMSG ("PLAY requested, song length calculated: %i msec\n", (int) (midifile.length / 1000));

        /* done with file */
        midifile.file_pointer = NULL;

        /* play play play! */
        DEBUGMSG ("PLAY requested, starting play thread\n");
        amidiplug_playing_status = AMIDIPLUG_PLAY;
        amidiplug_play_loop ();
        break;
    }

    default:
    {
        amidiplug_playing_status = AMIDIPLUG_ERR;
        fprintf (stderr, "%s is not a Standard MIDI File\n", filename_uri);
        break;
    }
    }

    audio_cleanup ();

    return TRUE;
}

static void generate_to_tick (int tick)
{
    double ticksecs = (double) midifile.current_tempo / midifile.ppq / 1000000;
    audio_generate (ticksecs * (tick - midifile.playing_tick));
    midifile.playing_tick = tick;
}

static void amidiplug_play_loop ()
{
    bool_t stopped = FALSE;

    backend->prepare ();

    /* initialize current position in each track */
    for (int j = 0; j < midifile.num_tracks; ++j)
        midifile.tracks[j].current_event = midifile.tracks[j].first_event;

    while (! (stopped = aud_input_check_stop ()))
    {
        int seektime = aud_input_check_seek ();
        if (seektime >= 0)
            amidiplug_skipto ((int64_t) seektime * 1000 / midifile.avg_microsec_per_tick);

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

        if (!event)
            break; /* end of song reached */

        /* advance pointer to next event */
        event_track->current_event = event->next;

        if (event->tick > midifile.playing_tick)
            generate_to_tick (event->tick);

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
    }

    if (! stopped)
        generate_to_tick (midifile.max_tick);

    backend->reset ();

    i_midi_free (& midifile);
}


/* amidigplug_skipto: re-do all events that influence the playing of our
   midi file; re-do them using a time-tick of 0, so they are processed
   istantaneously and proceed this way until the playing_tick is reached */
static void amidiplug_skipto (int playing_tick)
{
    backend->reset ();

    /* this check is always made, for safety*/
    if (playing_tick >= midifile.max_tick)
        playing_tick = midifile.max_tick - 1;

    /* initialize current position in each track */
    for (int i = 0; i < midifile.num_tracks; ++i)
        midifile.tracks[i].current_event = midifile.tracks[i].first_event;

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
    .cleanup = amidiplug_cleanup,
    .probe_for_tuple = amidiplug_get_song_tuple,
    .file_info_box = i_fileinfo_gui,
    .is_our_file_from_vfs = amidiplug_is_our_file_from_vfs,
    .extensions = amidiplug_vfs_extensions,
)
