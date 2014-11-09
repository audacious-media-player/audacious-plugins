/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Main source file

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2009 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include "xs_config.h"
#include "xs_sidplay2.h"

static void xs_get_song_tuple_info(Tuple &pResult, const xs_tuneinfo_t &info, int subTune);

/*
 * Initialization functions
 */
bool xs_init(void)
{
    /* Initialize and get configuration */
    xs_init_configuration();

    /* Try to initialize emulator engine */
    return xs_sidplayfp_init();
}


/*
 * Shut down XMMS-SID
 */
void xs_close(void)
{
    xs_sidplayfp_close();
}


/*
 * Check whether this is a SID file
 */
bool xs_is_our_file(const char *filename, VFSFile &file)
{
    char buf[4];
    if (file.fread (buf, 1, 4) != 4)
        return false;

    return xs_sidplayfp_probe(buf, 4);
}


/*
 * Start playing the given file
 */
bool xs_play_file(const char *filename, VFSFile &file)
{
    /* Load file */
    Index<char> buf = file.read_all ();
    if (!xs_sidplayfp_probe(buf.begin(), buf.len()))
        return false;

    /* Get tune information */
    xs_tuneinfo_t info;
    if (!xs_sidplayfp_getinfo(info, filename, buf.begin(), buf.len()))
        return false;

    /* Initialize the tune */
    if (!xs_sidplayfp_load(buf.begin(), buf.len()))
        return false;

    /* Set general status information */
    int subTune = -1;
    uri_parse(filename, nullptr, nullptr, nullptr, &subTune);

    if (subTune < 1 || subTune > info.nsubTunes)
        subTune = info.startTune;

    /* Check minimum playtime */
    int tmpLength = info.subTunes[subTune - 1].tuneLength;
    if (xs_cfg.playMinTimeEnable && (tmpLength >= 0)) {
        if (tmpLength < xs_cfg.playMinTime)
            tmpLength = xs_cfg.playMinTime;
    }

    /* Initialize song */
    if (!xs_sidplayfp_initsong(subTune)) {
        AUDERR("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n",
            (const char *) info.sidFilename, subTune);
        return false;
    }

    /* Open the audio output */
    if (!aud_input_open_audio(FMT_S16_NE, xs_cfg.audioFrequency, xs_cfg.audioChannels))
    {
        AUDERR("Couldn't open audio output (fmt=%x, freq=%i, nchan=%i)!\n",
            FMT_S16_NE,
            xs_cfg.audioFrequency,
            xs_cfg.audioChannels);

        return false;
    }

    /* Set song information for current subtune */
    xs_sidplayfp_updateinfo(info, subTune);

    Tuple tmpTuple;
    tmpTuple.set_filename(info.sidFilename);
    xs_get_song_tuple_info(tmpTuple, info, subTune);
    aud_input_set_tuple (std::move (tmpTuple));

    /* Allocate audio buffer */
    int audioBufSize = xs_cfg.audioFrequency * xs_cfg.audioChannels * 2;
    if (audioBufSize < 512)
        audioBufSize = 512;

    char *audioBuffer = new char[audioBufSize];
    int64_t bytes_played = 0;

    while (! aud_input_check_stop ())
    {
        if (aud_input_check_seek () >= 0)
            AUDWARN ("Seeking is not implemented, ignoring.\n");

        int bufRemaining = xs_sidplayfp_fillbuffer(audioBuffer, audioBufSize);

        aud_input_write_audio (audioBuffer, bufRemaining);
        bytes_played += bufRemaining;

        /* Check if we have played enough */
        int time_played = aud::rescale<int64_t> (bytes_played,
         xs_cfg.audioFrequency * xs_cfg.audioChannels * 2, 1000);

        if (xs_cfg.playMaxTimeEnable) {
            if (xs_cfg.playMaxTimeUnknown) {
                if (tmpLength < 0 &&
                    time_played >= xs_cfg.playMaxTime * 1000)
                    break;
            } else {
                if (time_played >= xs_cfg.playMaxTime * 1000)
                    break;
            }
        }

        if (tmpLength >= 0) {
            if (time_played >= tmpLength * 1000)
                break;
        }
    }

    delete[] audioBuffer;

    return true;
}


/*
 * Return song information Tuple
 */
static void xs_get_song_tuple_info(Tuple &tuple, const xs_tuneinfo_t &info, int subTune)
{
    tuple.set_str (Tuple::Title, info.sidName);
    tuple.set_str (Tuple::Artist, info.sidComposer);
    tuple.set_str (Tuple::Copyright, info.sidCopyright);
    tuple.set_str (Tuple::Codec, info.sidFormat);

#if 0
    switch (info.sidModel) {
        case XS_SIDMODEL_6581: tmpStr = "6581"; break;
        case XS_SIDMODEL_8580: tmpStr = "8580"; break;
        case XS_SIDMODEL_ANY: tmpStr = "ANY"; break;
        default: tmpStr = "?"; break;
    }
    tuple_set_str(tuple, -1, "sid-model", tmpStr);
#endif

    /* Get sub-tune information, if available */
    if (subTune < 0 || info.startTune > info.nsubTunes)
        subTune = info.startTune;

    if (subTune > 0 && subTune <= info.nsubTunes) {
        int tmpInt = info.subTunes[subTune - 1].tuneLength;
        tuple.set_int (Tuple::Length, (tmpInt < 0) ? -1 : tmpInt * 1000);

#if 0
        tmpInt = info.subTunes[subTune - 1].tuneSpeed;
        if (tmpInt > 0) {
            switch (tmpInt) {
            case XS_CLOCK_PAL: tmpStr = "PAL"; break;
            case XS_CLOCK_NTSC: tmpStr = "NTSC"; break;
            case XS_CLOCK_ANY: tmpStr = "ANY"; break;
            case XS_CLOCK_VBI: tmpStr = "VBI"; break;
            case XS_CLOCK_CIA: tmpStr = "CIA"; break;
            default:
                snprintf(tmpStr2, sizeof(tmpStr2), "%dHz", tmpInt);
                tmpStr = tmpStr2;
                break;
            }
        } else
            tmpStr = "?";

        tuple_set_str(tuple, -1, "sid-speed", tmpStr);
#endif
    } else
        subTune = 1;

    tuple.set_int (Tuple::NumSubtunes, info.nsubTunes);
    tuple.set_int (Tuple::Subtune, subTune);
    tuple.set_int (Tuple::Track, subTune);
}


static void xs_fill_subtunes(Tuple &tuple, const xs_tuneinfo_t &info)
{
    Index<int> subtunes;

    for (int count = 0; count < info.nsubTunes; count++) {
        if (count + 1 == info.startTune || !xs_cfg.subAutoMinOnly ||
            info.subTunes[count].tuneLength < 0 ||
            info.subTunes[count].tuneLength >= xs_cfg.subAutoMinTime)
            subtunes.append (count + 1);
    }

    tuple.set_subtunes (subtunes.len (), subtunes.begin ());
}

Tuple xs_probe_for_tuple(const char *filename, VFSFile &fd)
{
    Tuple tuple;
    xs_tuneinfo_t info;
    int tune = -1;

    Index<char> buf = fd.read_all ();
    if (!xs_sidplayfp_probe(buf.begin(), buf.len()))
        return tuple;

    /* Get information from URL */
    tuple.set_filename (filename);
    tune = tuple.get_int (Tuple::Subtune);

    /* Get tune information from emulation engine */
    if (!xs_sidplayfp_getinfo(info, filename, buf.begin(), buf.len()))
        return tuple;

    xs_get_song_tuple_info(tuple, info, tune);

    if (xs_cfg.subAutoEnable && info.nsubTunes > 1 && tune < 0)
        xs_fill_subtunes(tuple, info);

    return tuple;
}

/*
 * Plugin header
 */
static const char *xs_sid_fmts[] = { "sid", "psid", nullptr };

#define AUD_PLUGIN_NAME        "SID Player"
#define AUD_PLUGIN_PREFS       &sid_prefs
#define AUD_PLUGIN_INIT        xs_init
#define AUD_PLUGIN_CLEANUP     xs_close
#define AUD_INPUT_IS_OUR_FILE  xs_is_our_file
#define AUD_INPUT_PLAY         xs_play_file
#define AUD_INPUT_READ_TUPLE   xs_probe_for_tuple

#define AUD_INPUT_EXTS         xs_sid_fmts
#define AUD_INPUT_SUBTUNES     true

/* medium priority (slow to initialize) */
#define AUD_INPUT_PRIORITY     5

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
