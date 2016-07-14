/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
*
* MIDI (SMF) parser based on aplaymidi.c from ALSA-utils
* aplaymidi.c is Copyright (c) 2004 Clemens Ladisch <clemens@ladisch.de>
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

#include "i_midi.h"

#include <stdlib.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>
#include <libaudcore/vfs.h>

#include "i_configure.h"

#define MAKE_ID(c1, c2, c3, c4) ((c1) | ((c2) << 8) | ((c3) << 16) | ((c4) << 24))

#define WARNANDBREAK(...) { AUDERR (__VA_ARGS__); break; }

#define ERRMSG_MIDITRACK() { AUDERR ("%s: invalid MIDI data (offset %#x)", \
    (const char *) file_name, file_offset); return false; }


/* skip a certain number of bytes */
void midifile_t::skip_bytes (int bytes)
{
    while (bytes > 0)
    {
        read_byte ();
        --bytes;
    }
}


/* reads a single byte */
int midifile_t::read_byte ()
{
    if (file_offset >= file_data.len ())
    {
        file_eof = true;
        return -1;
    }

    return (unsigned char) file_data[file_offset ++];
}


/* reads a little-endian 32-bit integer */
int midifile_t::read_32_le ()
{
    int value;
    value = read_byte ();
    value |= read_byte () << 8;
    value |= read_byte () << 16;
    value |= read_byte () << 24;
    return !file_eof ? value : -1;
}


/* reads a 4-character identifier */
int midifile_t::read_id ()
{
    return read_32_le ();
}


/* reads a fixed-size big-endian number */
int midifile_t::read_int (int bytes)
{
    int c, value = 0;

    do
    {
        c = read_byte ();

        if (c < 0) return -1;

        value = (value << 8) | c;
    }
    while (--bytes);

    return value;
}


/* reads a variable-length number */
int midifile_t::read_var ()
{
    int value, c;

    c = read_byte ();
    value = c & 0x7f;

    if (c & 0x80)
    {
        c = read_byte ();
        value = (value << 7) | (c & 0x7f);

        if (c & 0x80)
        {
            c = read_byte ();
            value = (value << 7) | (c & 0x7f);

            if (c & 0x80)
            {
                c = read_byte ();
                value = (value << 7) | c;

                if (c & 0x80)
                    return -1;
            }
        }
    }

    return value;
}


/* reads one complete track from the file */
bool midifile_t::read_track (midifile_track_t & track, int track_end, int port_count)
{
    int tick = 0;
    unsigned char last_cmd = 0;
    unsigned char port = 0;

    track.start_tick = -1;
    track.end_tick = -1;

    /* the current file position is after the track ID and length */
    while (file_offset < track_end)
    {
        unsigned char cmd;
        midievent_t * event;
        int delta_ticks, len, c;

        delta_ticks = read_var ();

        if (delta_ticks < 0)
            break;

        tick += delta_ticks;

        c = read_byte ();

        if (c < 0)
            break;

        if (c & 0x80)
        {
            /* have command */
            cmd = c;

            if (cmd < 0xf0)
                last_cmd = cmd;
        }
        else
        {
            /* running status */
            file_offset--;
            cmd = last_cmd;

            if (!cmd)
                ERRMSG_MIDITRACK();
        }

        switch (cmd >> 4)
        {
            /* maps SMF events to ALSA sequencer events */
            static unsigned char cmd_type[16] =
            {
                0, 0, 0, 0, 0, 0, 0, 0,
                SND_SEQ_EVENT_NOTEOFF,     // 08h
                SND_SEQ_EVENT_NOTEON,      // 09h
                SND_SEQ_EVENT_KEYPRESS,    // 0ah
                SND_SEQ_EVENT_CONTROLLER,  // 0bh
                SND_SEQ_EVENT_PGMCHANGE,   // 0ch
                SND_SEQ_EVENT_CHANPRESS,   // 0dh
                SND_SEQ_EVENT_PITCHBEND    // 0eh
            };

        case 0x8: /* channel msg with 2 parameter bytes */
        case 0x9:
        case 0xa:
        {
            event = track.add_event ();
            event->type = cmd_type[cmd >> 4];
            event->port = port;
            event->tick = tick;
            event->d[0] = cmd & 0x0f;

            /* if this note is not in standard drum channel (10), apply transpose */
            if (event->d[0] != 9)
            {
                int transpose = aud_get_int ("amidiplug", "ap_opts_transpose_value");
                int data_tr = (read_byte () & 0x7f) + transpose;

                if (data_tr > 127) data_tr = 127;
                else if (data_tr < 0) data_tr = 0;

                event->d[1] = (unsigned char) data_tr;
            }
            else /* this note is in standard drum channel (10), apply drum shift */
            {
                int drumshift = aud_get_int ("amidiplug", "ap_opts_drumshift_value");
                int data_ds = (read_byte () & 0x7f) + drumshift; /* always > 0 */

                if (data_ds > 127) data_ds -= 127;

                event->d[1] = (unsigned char) data_ds;
            }

            event->d[2] = read_byte () & 0x7f;

            if (event->type == SND_SEQ_EVENT_NOTEON && track.start_tick < 0)
                track.start_tick = tick;
            if (track.start_tick >= 0 && tick >= track.start_tick)
                track.end_tick = tick;
        }
        break;

        case 0xb: /* channel msg with 2 parameter bytes */
        case 0xe:
        {
            event = track.add_event ();
            event->type = cmd_type[cmd >> 4];
            event->port = port;
            event->tick = tick;
            event->d[0] = cmd & 0x0f;
            event->d[1] = read_byte () & 0x7f;
            event->d[2] = read_byte () & 0x7f;

            if (track.start_tick >= 0 && tick >= track.start_tick)
                track.end_tick = tick;
        }
        break;

        case 0xc: /* channel msg with 1 parameter byte */
        case 0xd:
        {
            event = track.add_event ();
            event->type = cmd_type[cmd >> 4];
            event->port = port;
            event->tick = tick;
            event->d[0] = cmd & 0x0f;
            event->d[1] = read_byte () & 0x7f;

            if (track.start_tick >= 0 && tick >= track.start_tick)
                track.end_tick = tick;
        }
        break;

        case 0xf:
        {
            switch (cmd)
            {
            case 0xf0: /* sysex */
            case 0xf7: /* continued sysex, or escaped commands */
            {
                len = read_var ();

                if (len < 0)
                    ERRMSG_MIDITRACK();

                /* skip sysex */
                skip_bytes (len);
            }
            break;

            case 0xff: /* meta event */
            {
                c = read_byte ();
                len = read_var ();

                if (len < 0)
                    ERRMSG_MIDITRACK();

                switch (c)
                {
                case 0x21: /* port number */
                {
                    if (len < 1)
                        ERRMSG_MIDITRACK();

                    port = read_byte () % port_count;
                    skip_bytes (len - 1);
                }
                break;

                case 0x2f: /* end of track */
                {
                    if (! aud_get_bool ("amidiplug", "skip_leading"))
                        track.start_tick = 0;
                    if (! aud_get_bool ("amidiplug", "skip_trailing"))
                        track.end_tick = tick;

                    skip_bytes (track_end - file_offset);
                    return true;
                }

                case 0x51: /* tempo */
                {
                    if (len < 3)
                        ERRMSG_MIDITRACK();

                    if (smpte_timing)
                    {
                        /* SMPTE timing doesn't change */
                        skip_bytes (len);
                    }
                    else
                    {
                        event = track.add_event ();
                        event->type = SND_SEQ_EVENT_TEMPO;
                        event->port = port;
                        event->tick = tick;
                        event->tempo = read_byte () << 16;
                        event->tempo |= read_byte () << 8;
                        event->tempo |= read_byte ();
                        skip_bytes (len - 3);
                    }
                }
                break;

                case 0x01: /* text comments */
                {
                    if (len < 0)
                        ERRMSG_MIDITRACK();

                    event = track.add_event ();
                    event->type = SND_SEQ_EVENT_META_TEXT;
                    event->tick = tick;

                    StringBuf buf (len);
                    for (int i = 0; i < len; i ++)
                        buf[i] = read_byte ();

                    event->metat = String (str_to_utf8 (std::move (buf)));
                }
                break;

                case 0x05: /* lyrics */
                {
                    if (len < 0)
                        ERRMSG_MIDITRACK();

                    event = track.add_event ();
                    event->type = SND_SEQ_EVENT_META_LYRIC;
                    event->tick = tick;

                    StringBuf buf (len);
                    for (int i = 0; i < len; i ++)
                        buf[i] = read_byte ();

                    event->metat = String (str_to_utf8 (std::move (buf)));
                }
                break;

                default: /* ignore all other meta events */
                {
                    skip_bytes (len);
                }
                break;
                }
            }
            break;

            default: /* invalid Fx command */
                ERRMSG_MIDITRACK();
            }
        }
        break;

        default: /* cannot happen */
            ERRMSG_MIDITRACK();
        }
    }

    ERRMSG_MIDITRACK();
}


/* read a MIDI file in Standard MIDI Format */
/* return values: 0 = error, 1 = ok */
bool midifile_t::parse_smf (int port_count)
{
    int header_len;

    /* the curren position is immediately after the "MThd" id */
    header_len = read_int (4);

    if (header_len < 6)
    {
        AUDERR ("%s: invalid file format\n", (const char *) file_name);
        return false;
    }

    format = read_int (2);

    if (format != 0 && format != 1)
    {
        AUDERR ("%s: type %d format is not supported\n", (const char *) file_name, format);
        return false;
    }

    int num_tracks = read_int (2);

    if (num_tracks < 1 || num_tracks > 1000)
    {
        AUDERR ("%s: invalid number of tracks (%d)\n", (const char *) file_name, num_tracks);
        num_tracks = 0;
        return false;
    }

    tracks.insert (0, num_tracks);

    time_division = read_int (2);

    if (time_division < 0)
    {
        AUDERR ("%s: invalid file format\n", (const char *) file_name);
        return false;
    }

    smpte_timing = !! (time_division & 0x8000);

    /* read tracks */
    for (midifile_track_t & track : tracks)
    {
        int len;

        /* search for MTrk chunk */
        for (;;)
        {
            int id = read_id ();
            len = read_int (4);

            if (file_eof)
            {
                AUDERR ("%s: unexpected end of file\n", (const char *) file_name);
                return false;
            }

            if (len < 0 || len >= 0x10000000)
            {
                AUDERR ("%s: invalid chunk length %d\n", (const char *) file_name, len);
                return false;
            }

            if (id == MAKE_ID ('M', 'T', 'r', 'k'))
                break;

            skip_bytes (len);
        }

        if (! read_track (track, file_offset + len, port_count))
            return false;
    }

    /* calculate the max_tick for the entire file */
    start_tick = -1;
    max_tick = 0;

    for (midifile_track_t & track : tracks)
    {
        if (track.start_tick >= 0 && (start_tick < 0 || track.start_tick < start_tick))
            start_tick = track.start_tick;
        if (track.end_tick > max_tick)
            max_tick = track.end_tick;
    }

    if (start_tick < 0)
        start_tick = 0;

    /* ok, success */
    return true;
}


/* read a MIDI file enclosed in RIFF format */
/* return values: 0 = error, 1 = ok */
bool midifile_t::parse_riff ()
{
    /* skip file length (4 bytes) */
    skip_bytes (4);

    /* check file type ("RMID" = RIFF MIDI) */
    if (read_id () != MAKE_ID ('R', 'M', 'I', 'D'))
        return false;

    /* search for "data" chunk */
    for (;;)
    {
        int id = read_id ();
        int len = read_32_le ();

        if (file_eof)
            return false;

        if (id == MAKE_ID ('d', 'a', 't', 'a'))
            break;

        if (len < 0)
            return false;

        skip_bytes ((len + 1) & ~1);
    }

    /* the "data" chunk must contain data in SMF format */
    if (read_id () != MAKE_ID ('M', 'T', 'h', 'd'))
        return false;

    /* ok, success */
    return true;
}


/* queue set tempo */
bool midifile_t::setget_tempo ()
{
    /* interpret and set tempo */
    bool smpte_timing = !! (time_division & 0x8000);

    if (!smpte_timing)
    {
        /* time_division is ticks per quarter */
        current_tempo = 500000;
        ppq = time_division;
    }
    else
    {
        /* upper byte is negative frames per second */
        int fps = 0x80 - ((time_division >> 8) & 0x7f);
        /* lower byte is ticks per frame */
        int tpf = time_division & 0xff;

        /* now pretend that we have quarter-note based timing */
        switch (fps)
        {
        case 24:
            current_tempo = 500000;
            ppq = 12 * tpf;
            break;

        case 25:
            current_tempo = 400000;
            ppq = 10 * tpf;
            break;

        case 29: /* 30 drop-frame */
            current_tempo = 100000000;
            ppq = 2997 * tpf;
            break;

        case 30:
            current_tempo = 500000;
            ppq = 15 * tpf;
            break;

        default:
            AUDERR ("Invalid number of SMPTE frames per second (%d)\n", fps);
            return false;
        }
    }

    AUDDBG ("MIDI tempo set -> time division: %i\n", time_division);
    AUDDBG ("MIDI tempo set -> tempo: %i\n", current_tempo);
    AUDDBG ("MIDI tempo set -> ppq: %i\n", ppq);
    return true;
}


/* this will set the midi length in microseconds
   COMMENT: this will also reset current position in each track! */
void midifile_t::setget_length ()
{
    int64_t length_microsec = 0;
    int last_tick = start_tick;
    /* get the first microsec_per_tick ratio */
    int microsec_per_tick = (int) (current_tempo / ppq);

    /* initialize current position in each track */
    for (midifile_track_t & track : tracks)
        track.current_event = track.events.head ();

    /* search for tempo events in each track; in fact, since the program
       currently supports type 0 and type 1 MIDI files, we should find
       tempo events only in one track */
    AUDDBG ("LENGTH calc: starting calc loop\n");

    for (;;)
    {
        midievent_t * event = nullptr;
        midifile_track_t * event_track = nullptr;
        int min_tick = max_tick + 1;

        /* search next event */
        for (midifile_track_t & track : tracks)
        {
            midievent_t * e2 = track.current_event;

            if (e2 && e2->tick < min_tick)
            {
                min_tick = e2->tick;
                event = e2;
                event_track = & track;
            }
        }

        if (!event)
        {
            /* calculate the remaining length */
            length_microsec += (microsec_per_tick * (max_tick - last_tick));
            break; /* end of song reached */
        }

        /* advance pointer to next event */
        event_track->current_event = event_track->events.next (event);

        /* check if this is a tempo event */
        if (event->type == SND_SEQ_EVENT_TEMPO)
        {
            int tick = aud::max (event->tick, start_tick);
            AUDDBG ("LENGTH calc: tempo event (%i) on tick %i\n", event->tempo, tick);

            /* increment length_microsec with the amount of microsec before tempo change */
            length_microsec += (microsec_per_tick * (tick - last_tick));
            /* now update last_tick and the microsec_per_tick ratio */
            last_tick = tick;
            microsec_per_tick = (int) (event->tempo / ppq);
        }
    }

    /* IMPORTANT
       this couple of important values is set by midifile_t::set_length */
    length = length_microsec;

    if (max_tick > start_tick)
        avg_microsec_per_tick = (int) (length_microsec / (max_tick - start_tick));
    else
        avg_microsec_per_tick = 0;

    return;
}


/* this will get the weighted average bpm of the midi file;
   if the file has a variable bpm, 'bpm' is set to -1;
   COMMENT: this will also reset current position in each track! */
void midifile_t::get_bpm (int * bpm, int * wavg_bpm)
{
    int last_tick = start_tick;
    unsigned weighted_avg_tempo = 0;
    bool is_monotempo = true;
    int last_tempo = current_tempo;

    /* initialize current position in each track */
    for (midifile_track_t & track : tracks)
        track.current_event = track.events.head ();

    /* search for tempo events in each track; in fact, since the program
       currently supports type 0 and type 1 MIDI files, we should find
       tempo events only in one track */
    AUDDBG ("BPM calc: starting calc loop\n");

    for (;;)
    {
        midievent_t * event = nullptr;
        midifile_track_t * event_track = nullptr;
        int min_tick = max_tick + 1;

        /* search next event */
        for (midifile_track_t & track : tracks)
        {
            midievent_t * e2 = track.current_event;

            if (e2 && e2->tick < min_tick)
            {
                min_tick = e2->tick;
                event = e2;
                event_track = & track;
            }
        }

        if (!event)
        {
            /* calculate the remaining length */
            if (max_tick > start_tick)
                weighted_avg_tempo += (unsigned) (last_tempo *
                 ((float) (max_tick - last_tick) / (float) (max_tick - start_tick)));

            break; /* end of song reached */
        }

        /* advance pointer to next event */
        event_track->current_event = event_track->events.next (event);

        /* check if this is a tempo event */
        if (event->type == SND_SEQ_EVENT_TEMPO)
        {
            int tick = aud::max (event->tick, start_tick);
            AUDDBG ("BPM calc: tempo event (%i) on tick %i\n", event->tempo, tick);

            /* check if this is a tempo change (real change, tempo should be
               different) in the midi file (and it shouldn't be at tick 0); */
            if (is_monotempo && tick > start_tick && event->tempo != last_tempo)
                is_monotempo = false;

            /* add the previous tempo change multiplied for its weight (the tick interval for the tempo )  */
            if (max_tick > start_tick)
                weighted_avg_tempo += (unsigned) (last_tempo *
                 ((float) (tick - last_tick) / (float) (max_tick - start_tick)));

            /* now update last_tick and the microsec_per_tick ratio */
            last_tick = tick;
            last_tempo = event->tempo;
        }
    }

    AUDDBG ("BPM calc: weighted average tempo: %i\n", weighted_avg_tempo);

    if (weighted_avg_tempo > 0)
        *wavg_bpm = (int) (60000000 / weighted_avg_tempo);
    else
        *wavg_bpm = 0;

    AUDDBG ("BPM calc: weighted average bpm: %i\n", *wavg_bpm);

    if (is_monotempo)
        *bpm = *wavg_bpm; /* the song has fixed bpm */
    else
        *bpm = -1; /* the song has variable bpm */
}


/* helper function that parses a midi file; returns 1 on success, 0 otherwise */
bool midifile_t::parse_from_file (const char * filename, VFSFile & file)
{
    bool success = false;

    file_name = String (filename);
    file_data = file.read_all ();

    switch (read_id ())
    {
    case MAKE_ID ('R', 'I', 'F', 'F') :
        AUDDBG ("PARSE_FROM_FILENAME requested, RIFF chunk found, processing...\n");

        /* read riff chunk */
        if (!parse_riff ())
            WARNANDBREAK ("%s: invalid file format (riff parser)\n", filename);

        /* if that was read correctly, go ahead and read smf data */

    case MAKE_ID ('M', 'T', 'h', 'd') :
        AUDDBG ("PARSE_FROM_FILENAME requested, MThd chunk found, processing...\n");

        /* we don't care about port count here, pass 1 */
        if (!parse_smf (1))
            WARNANDBREAK ("%s: invalid file format (smf parser)\n", filename);

        if (time_division < 1)
            WARNANDBREAK ("%s: invalid time division (%i)\n", filename, time_division);

        /* fill ppq and tempo using time_division */
        if (!setget_tempo ())
            WARNANDBREAK ("%s: invalid values while setting ppq and tempo\n", filename);

        /* fill length, keeping in count tempo-changes */
        setget_length ();

        /* ok, mf has been filled with information; successfully return */
        success = true;
        break;

    default:
        AUDERR ("%s is not a Standard MIDI File\n", filename);
        break;
    }

    file_name = String ();
    file_data.clear ();

    return success;
}
