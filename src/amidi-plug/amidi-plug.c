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

#include <glib.h>

#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/input.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "i_backend.h"
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

static midifile_t midifile;

static const char * const amidiplug_vfs_extensions[] = {"mid", "midi", "rmi", "rmid", NULL};

static void amidiplug_cleanup (void)
{
    backend_cleanup ();
}

static bool_t amidiplug_init (void)
{
    static const char * const defaults[] =
    {
        "ap_opts_transpose_value", "0",
        "ap_opts_drumshift_value", "0",
        "fsyn_synth_samplerate", "44100",
        "fsyn_synth_gain", "-1",
        "fsyn_synth_polyphony", "-1",
        "fsyn_synth_reverb", "-1",
        "fsyn_synth_chorus", "-1",
        NULL
    };

    aud_config_set_defaults ("amidiplug", defaults);

    backend_init ();

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
        AUDDBG ("MIDI found, %s is a standard midi file\n", filename_uri);
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
            AUDDBG ("MIDI found, %s is a riff midi file\n", filename_uri);
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
        tuple_set_int (tuple, FIELD_LENGTH, mf.length / 1000);

    i_midi_free (&mf);

    return tuple;
}


static int s_samplerate, s_channels;
static int s_bufsize;
static void * s_buf;

static bool_t audio_init (void)
{
    int bitdepth;

    backend_audio_info (& s_channels, & bitdepth, & s_samplerate);

    if (bitdepth != 16 || ! aud_input_open_audio (FMT_S16_NE, s_samplerate, s_channels))
        return FALSE;

    s_bufsize = 2 * s_channels * (s_samplerate / 4);
    s_buf = g_malloc (s_bufsize);

    return TRUE;
}

static void audio_generate (double seconds)
{
    int total = 2 * s_channels * (int) round (seconds * s_samplerate);

    while (total)
    {
        int chunk = (total < s_bufsize) ? total : s_bufsize;

        backend_generate_audio (s_buf, chunk);
        aud_input_write_audio (s_buf, chunk);

        total -= chunk;
    }
}

static void audio_cleanup (void)
{
    g_free (s_buf);
}

static bool_t amidiplug_play (const char * filename_uri, VFSFile * file)
{
    if (g_atomic_int_compare_and_exchange (& backend_settings_changed, TRUE, FALSE))
    {
        AUDDBG ("Settings changed, reinitializing backend\n");
        backend_cleanup ();
        backend_init ();
    }

    if (! audio_init ())
        return FALSE;

    AUDDBG ("PLAY requested, midifile init\n");
    /* midifile init */
    i_midi_init (&midifile);

    AUDDBG ("PLAY requested, opening file: %s\n", filename_uri);
    midifile.file_pointer = file;
    midifile.file_name = g_strdup (filename_uri);

    switch (i_midi_file_read_id (&midifile))
    {
    case MAKE_ID ('R', 'I', 'F', 'F') :
        AUDDBG ("PLAY requested, RIFF chunk found, processing...\n");

        /* read riff chunk */
        if (!i_midi_file_parse_riff (&midifile))
        {
            fprintf (stderr, "%s: invalid file format (riff parser)\n", filename_uri);
            goto ERR;
        }

        /* if that was read correctly, go ahead and read smf data */

    case MAKE_ID ('M', 'T', 'h', 'd') :
        AUDDBG ("PLAY requested, MThd chunk found, processing...\n");

        if (!i_midi_file_parse_smf (&midifile, 1))
        {
            fprintf (stderr, "%s: invalid file format (smf parser)\n", filename_uri);
            goto ERR;
        }

        if (midifile.time_division < 1)
        {
            fprintf (stderr, "%s: invalid time division (%i)\n", filename_uri, midifile.time_division);
            goto ERR;
        }

        AUDDBG ("PLAY requested, setting ppq and tempo...\n");

        /* fill midifile.ppq and midifile.tempo using time_division */
        if (!i_midi_setget_tempo (&midifile))
        {
            fprintf (stderr, "%s: invalid values while setting ppq and tempo\n", filename_uri);
            goto ERR;
        }

        /* fill midifile.length, keeping in count tempo-changes */
        i_midi_setget_length (&midifile);
        AUDDBG ("PLAY requested, song length calculated: %i msec\n", (int) (midifile.length / 1000));

        /* done with file */
        midifile.file_pointer = NULL;

        /* play play play! */
        AUDDBG ("PLAY requested, starting play thread\n");
        amidiplug_play_loop ();
        break;

    default:
        fprintf (stderr, "%s is not a Standard MIDI File\n", filename_uri);
        goto ERR;
    }

    audio_cleanup ();
    return TRUE;

ERR:
    audio_cleanup ();
    return FALSE;
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

    backend_prepare ();

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
            seq_event_noteon (event);
            break;

        case SND_SEQ_EVENT_NOTEOFF:
            seq_event_noteoff (event);
            break;

        case SND_SEQ_EVENT_KEYPRESS:
            seq_event_keypress (event);
            break;

        case SND_SEQ_EVENT_CONTROLLER:
            seq_event_controller (event);
            break;

        case SND_SEQ_EVENT_PGMCHANGE:
            seq_event_pgmchange (event);
            break;

        case SND_SEQ_EVENT_CHANPRESS:
            seq_event_chanpress (event);
            break;

        case SND_SEQ_EVENT_PITCHBEND:
            seq_event_pitchbend (event);
            break;

        case SND_SEQ_EVENT_SYSEX:
            seq_event_sysex (event);
            break;

        case SND_SEQ_EVENT_TEMPO:
            seq_event_tempo (event);
            AUDDBG ("PLAY thread, processing tempo event with value %i on tick %i\n",
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
            AUDDBG ("PLAY thread, encountered invalid event type %i\n", event->type);
            break;
        }
    }

    if (! stopped)
        generate_to_tick (midifile.max_tick);

    backend_reset ();

    i_midi_free (& midifile);
}


/* amidigplug_skipto: re-do all events that influence the playing of our
   midi file; re-do them using a time-tick of 0, so they are processed
   istantaneously and proceed this way until the playing_tick is reached */
static void amidiplug_skipto (int playing_tick)
{
    backend_reset ();

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
            AUDDBG ("SKIPTO request, reached the last event but not the requested tick (!)\n");
            break; /* end of song reached */
        }

        /* reached the requested tick, job done */
        if (event->tick >= playing_tick)
        {
            AUDDBG ("SKIPTO request, reached the requested tick, exiting from skipto loop\n");
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
            seq_event_controller (event);
            break;

        case SND_SEQ_EVENT_PGMCHANGE:
            seq_event_pgmchange (event);
            break;

        case SND_SEQ_EVENT_CHANPRESS:
            seq_event_chanpress (event);
            break;

        case SND_SEQ_EVENT_PITCHBEND:
            seq_event_pitchbend (event);
            break;

        case SND_SEQ_EVENT_SYSEX:
            seq_event_sysex (event);
            break;

        case SND_SEQ_EVENT_TEMPO:
            seq_event_tempo (event);
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
    .prefs = & amidiplug_prefs,
    .play = amidiplug_play,
    .cleanup = amidiplug_cleanup,
    .probe_for_tuple = amidiplug_get_song_tuple,
    .file_info_box = i_fileinfo_gui,
    .is_our_file_from_vfs = amidiplug_is_our_file_from_vfs,
    .extensions = amidiplug_vfs_extensions,
)
