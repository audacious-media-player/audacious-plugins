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

#include <libaudcore/runtime.h>
#include <libaudcore/i18n.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>

#include "i_backend.h"
#include "i_configure.h"
#include "i_fileinfo.h"
#include "i_midi.h"

enum
{
    AMIDIPLUG_STOP,
    AMIDIPLUG_PLAY,
    AMIDIPLUG_ERR
};

static void amidiplug_play_loop (midifile_t & midifile);

static bool amidiplug_play (const char * filename_uri, VFSFile & file);
static Tuple amidiplug_get_song_tuple (const char * filename_uri, VFSFile & file);
static int amidiplug_skipto (midifile_t & midifile, int seektime);

static const char * const amidiplug_vfs_extensions[] = {"mid", "midi", "rmi", "rmid", nullptr};

static void amidiplug_cleanup (void)
{
    backend_cleanup ();
}

static bool amidiplug_init (void)
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
        "skip_leading", "FALSE",
        "skip_trailing", "FALSE",
        nullptr
    };

    aud_config_set_defaults ("amidiplug", defaults);

    backend_init ();

    return true;
}

static bool amidiplug_is_our_file_from_vfs (const char * filename_uri, VFSFile & fp)
{
    char magic_bytes[4];

    if (fp.fread (magic_bytes, 1, 4) != 4)
        return false;

    if (!strncmp (magic_bytes, "MThd", 4))
    {
        AUDDBG ("MIDI found, %s is a standard midi file\n", filename_uri);
        return true;
    }

    if (!strncmp (magic_bytes, "RIFF", 4))
    {
        /* skip the four bytes after RIFF,
           then read the next four */
        if (fp.fseek (4, VFS_SEEK_CUR) != 0)
            return false;

        if (fp.fread (magic_bytes, 1, 4) != 4)
            return false;

        if (!strncmp (magic_bytes, "RMID", 4))
        {
            AUDDBG ("MIDI found, %s is a riff midi file\n", filename_uri);
            return true;
        }
    }

    return false;
}

static Tuple amidiplug_get_song_tuple (const char * filename_uri, VFSFile & file)
{
    /* song title, get it from the filename */
    Tuple tuple;
    tuple.set_filename (filename_uri);

    midifile_t mf;

    if (mf.parse_from_filename (filename_uri))
        tuple.set_int (FIELD_LENGTH, mf.length / 1000);

    return tuple;
}


static int s_samplerate, s_channels;
static int s_bufsize;
static int16_t * s_buf;

static bool audio_init (void)
{
    int bitdepth;

    backend_audio_info (& s_channels, & bitdepth, & s_samplerate);

    if (bitdepth != 16 || ! aud_input_open_audio (FMT_S16_NE, s_samplerate, s_channels))
        return false;

    s_bufsize = 2 * s_channels * (s_samplerate / 4);
    s_buf = new int16_t[s_bufsize / 2];

    return true;
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
    delete[] s_buf;
}

static bool amidiplug_play (const char * filename_uri, VFSFile & file)
{
    if (__sync_bool_compare_and_swap (& backend_settings_changed, true, false))
    {
        AUDDBG ("Settings changed, reinitializing backend\n");
        backend_cleanup ();
        backend_init ();
    }

    if (! audio_init ())
        return false;

    AUDDBG ("PLAY requested, midifile init\n");
    /* midifile init */
    midifile_t midifile;

    AUDDBG ("PLAY requested, opening file: %s\n", filename_uri);
    midifile.file_data = file.read_all ();
    midifile.file_name = String (filename_uri);

    switch (midifile.read_id ())
    {
    case MAKE_ID ('R', 'I', 'F', 'F') :
        AUDDBG ("PLAY requested, RIFF chunk found, processing...\n");

        /* read riff chunk */
        if (! midifile.parse_riff ())
        {
            AUDERR ("%s: invalid file format (riff parser)\n", filename_uri);
            goto ERR;
        }

        /* if that was read correctly, go ahead and read smf data */

    case MAKE_ID ('M', 'T', 'h', 'd') :
        AUDDBG ("PLAY requested, MThd chunk found, processing...\n");

        if (! midifile.parse_smf (1))
        {
            AUDERR ("%s: invalid file format (smf parser)\n", filename_uri);
            goto ERR;
        }

        if (midifile.time_division < 1)
        {
            AUDERR ("%s: invalid time division (%i)\n", filename_uri, midifile.time_division);
            goto ERR;
        }

        AUDDBG ("PLAY requested, setting ppq and tempo...\n");

        /* fill midifile.ppq and midifile.tempo using time_division */
        if (! midifile.setget_tempo ())
        {
            AUDERR ("%s: invalid values while setting ppq and tempo\n", filename_uri);
            goto ERR;
        }

        /* fill midifile.length, keeping in count tempo-changes */
        midifile.setget_length ();
        AUDDBG ("PLAY requested, song length calculated: %i msec\n", (int) (midifile.length / 1000));

        /* done with file */
        midifile.file_data.clear ();

        /* play play play! */
        AUDDBG ("PLAY requested, starting play thread\n");
        amidiplug_play_loop (midifile);
        break;

    default:
        AUDERR ("%s is not a Standard MIDI File\n", filename_uri);
        goto ERR;
    }

    audio_cleanup ();
    return true;

ERR:
    audio_cleanup ();
    return false;
}

static void generate_ticks (midifile_t & midifile, int num_ticks)
{
    double ticksecs = (double) midifile.current_tempo / midifile.ppq / 1000000;
    audio_generate (ticksecs * num_ticks);
}

static void amidiplug_play_loop (midifile_t & midifile)
{
    int tick = midifile.start_tick;
    bool stopped = false;

    backend_prepare ();

    /* initialize current position in each track */
    for (midifile_track_t & track : midifile.tracks)
        track.current_event = track.events.head ();

    while (! (stopped = aud_input_check_stop ()))
    {
        int seektime = aud_input_check_seek ();
        if (seektime >= 0)
            tick = amidiplug_skipto (midifile, seektime);

        midievent_t * event = nullptr;
        midifile_track_t * event_track = nullptr;
        int min_tick = midifile.max_tick + 1;

        /* search next event */
        for (midifile_track_t & track : midifile.tracks)
        {
            midievent_t * e2 = track.current_event;

            if ((e2) && (e2->tick < min_tick))
            {
                min_tick = e2->tick;
                event = e2;
                event_track = & track;
            }
        }

        if (!event)
            break; /* end of song reached */

        /* advance pointer to next event */
        event_track->current_event = event_track->events.next (event);

        if (event->tick > tick)
        {
            generate_ticks (midifile, event->tick - tick);
            tick = event->tick;
        }

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
                      event->tempo, event->tick);
            midifile.current_tempo = event->tempo;
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
        generate_ticks (midifile, midifile.max_tick - tick);

    backend_reset ();
}


/* amidigplug_skipto: re-do all events that influence the playing of our
   midi file; re-do them using a time-tick of 0, so they are processed
   istantaneously and proceed this way until the playing_tick is reached */
static int amidiplug_skipto (midifile_t & midifile, int seektime)
{
    backend_reset ();

    int tick = midifile.start_tick;
    if (midifile.avg_microsec_per_tick > 0)
        tick += (int64_t) seektime * 1000 / midifile.avg_microsec_per_tick;

    /* initialize current position in each track */
    for (midifile_track_t & track : midifile.tracks)
        track.current_event = track.events.head ();

    for (;;)
    {
        midievent_t * event = nullptr;
        midifile_track_t * event_track = nullptr;
        int min_tick = midifile.max_tick + 1;

        /* search next event */
        for (midifile_track_t & track : midifile.tracks)
        {
            midievent_t * e2 = track.current_event;

            if ((e2) && (e2->tick < min_tick))
            {
                min_tick = e2->tick;
                event = e2;
                event_track = & track;
            }
        }

        /* unlikely here... unless very strange MIDI files are played :) */
        if (!event)
        {
            AUDDBG ("SKIPTO request, reached the last event but not the requested tick (!)\n");
            break; /* end of song reached */
        }

        /* reached the requested tick, job done */
        if (event->tick >= tick)
        {
            AUDDBG ("SKIPTO request, reached the requested tick, exiting from skipto loop\n");
            break;
        }

        /* advance pointer to next event */
        event_track->current_event = event_track->events.next (event);

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
            midifile.current_tempo = event->tempo;
            break;
        }
    }

    return tick;
}

static const char amidiplug_about[] =
 N_("AMIDI-Plug\n"
    "modular MIDI music player\n"
    "http://www.develia.org/projects.php?p=amidiplug\n\n"
    "written by Giacomo Lozito\n"
    "<james@develia.org>\n\n"
    "special thanks to...\n\n"
    "Clemens Ladisch and Jaroslav Kysela\n"
    "for their cool programs aplaymidi and amixer; those\n"
    "were really useful, along with alsa-lib docs, in order\n"
    "to learn more about the ALSA API\n\n"
    "Alfredo Spadafina\n"
    "for the nice midi keyboard logo\n\n"
    "Tony Vroon\n"
    "for the good help with alpha testing");

#define AUD_PLUGIN_NAME        N_("AMIDI-Plug (MIDI Player)")
#define AUD_PLUGIN_INIT        amidiplug_init
#define AUD_PLUGIN_ABOUT       amidiplug_about
#define AUD_PLUGIN_PREFS       & amidiplug_prefs
#define AUD_INPUT_PLAY         amidiplug_play
#define AUD_PLUGIN_CLEANUP     amidiplug_cleanup
#define AUD_INPUT_READ_TUPLE   amidiplug_get_song_tuple
#define AUD_INPUT_INFOWIN      i_fileinfo_gui
#define AUD_INPUT_IS_OUR_FILE  amidiplug_is_our_file_from_vfs
#define AUD_INPUT_EXTS         amidiplug_vfs_extensions

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
