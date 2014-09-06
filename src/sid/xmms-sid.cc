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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>

#include "xs_config.h"
#include "xs_sidplay2.h"
#include "xs_slsup.h"

static void xs_get_song_tuple_info(Tuple &pResult, xs_tuneinfo_t *pInfo, int subTune);

class FileBuffer
{
public:
    void *data;
    int64_t size;

    FileBuffer(VFSFile *file) :
        data(nullptr),
        size(0)
    {
        vfs_file_read_all(file, &data, &size);
    }

    ~FileBuffer()
        { free(data); }
};

/*
 * Initialization functions
 */
bool xs_init(void)
{
    /* Initialize and get configuration */
    xs_init_configuration();

    if (xs_cfg.audioFrequency < 8000)
        xs_cfg.audioFrequency = 8000;

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
 * Start playing the given file
 */
bool xs_play_file(const char *filename, VFSFile *file)
{
    xs_tuneinfo_t *info;
    int audioBufSize, bufRemaining, tmpLength, subTune = -1;
    char *audioBuffer = nullptr;
    Tuple tmpTuple;

    uri_parse (filename, nullptr, nullptr, nullptr, & subTune);

    /* Load file */
    FileBuffer buf(file);
    if (!xs_sidplayfp_probe(buf.data, buf.size))
        return false;

    /* Get tune information */
    if (!(info = xs_sidplayfp_getinfo(filename, buf.data, buf.size)))
        return false;

    /* Initialize the tune */
    if (!xs_sidplayfp_load(buf.data, buf.size))
    {
        xs_tuneinfo_free(info);
        return false;
    }

    bool error = false;

    /* Set general status information */
    if (subTune < 1 || subTune > info->nsubTunes)
        subTune = info->startTune;

    int channels = xs_cfg.audioChannels;

    /* Allocate audio buffer */
    audioBufSize = xs_cfg.audioFrequency * channels * FMT_SIZEOF (FMT_S16_NE);
    if (audioBufSize < 512) audioBufSize = 512;

    audioBuffer = new char[audioBufSize];

    /* Check minimum playtime */
    tmpLength = info->subTunes[subTune - 1].tuneLength;
    if (xs_cfg.playMinTimeEnable && (tmpLength >= 0)) {
        if (tmpLength < xs_cfg.playMinTime)
            tmpLength = xs_cfg.playMinTime;
    }

    /* Initialize song */
    if (!xs_sidplayfp_initsong(subTune)) {
        xs_error("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n",
            (const char *) info->sidFilename, subTune);
        goto xs_err_exit;
    }

    /* Open the audio output */
    if (!aud_input_open_audio(FMT_S16_NE, xs_cfg.audioFrequency, channels))
    {
        xs_error("Couldn't open audio output (fmt=%x, freq=%i, nchan=%i)!\n",
            FMT_S16_NE,
            xs_cfg.audioFrequency,
            channels);

        goto xs_err_exit;
    }

    /* Set song information for current subtune */
    xs_sidplayfp_updateinfo(info, subTune);
    tmpTuple.set_filename (info->sidFilename);
    xs_get_song_tuple_info(tmpTuple, info, subTune);

    aud_input_set_tuple (std::move (tmpTuple));

    while (! aud_input_check_stop ())
    {
        bufRemaining = xs_sidplayfp_fillbuffer(audioBuffer, audioBufSize);

        aud_input_write_audio (audioBuffer, bufRemaining);

        /* Check if we have played enough */
        if (xs_cfg.playMaxTimeEnable) {
            if (xs_cfg.playMaxTimeUnknown) {
                if (tmpLength < 0 &&
                    aud_input_written_time() >= xs_cfg.playMaxTime * 1000)
                    break;
            } else {
                if (aud_input_written_time() >= xs_cfg.playMaxTime * 1000)
                    break;
            }
        }

        if (tmpLength >= 0) {
            if (aud_input_written_time() >= tmpLength * 1000)
                break;
        }
    }

DONE:
    delete[] audioBuffer;

    /* Free tune information */
    xs_tuneinfo_free(info);

    /* Exit the playing thread */
    return ! error;

xs_err_exit:
    error = true;
    goto DONE;
}


/*
 * Return song information Tuple
 */
static void xs_get_song_tuple_info(Tuple &tuple, xs_tuneinfo_t *info, int subTune)
{
    tuple.set_str (FIELD_TITLE, info->sidName);
    tuple.set_str (FIELD_ARTIST, info->sidComposer);
    tuple.set_str (FIELD_COPYRIGHT, info->sidCopyright);
    tuple.set_str (FIELD_CODEC, info->sidFormat);

#if 0
    switch (info->sidModel) {
        case XS_SIDMODEL_6581: tmpStr = "6581"; break;
        case XS_SIDMODEL_8580: tmpStr = "8580"; break;
        case XS_SIDMODEL_ANY: tmpStr = "ANY"; break;
        default: tmpStr = "?"; break;
    }
    tuple_set_str(tuple, -1, "sid-model", tmpStr);
#endif

    /* Get sub-tune information, if available */
    if (subTune < 0 || info->startTune > info->nsubTunes)
        subTune = info->startTune;

    if (subTune > 0 && subTune <= info->nsubTunes) {
        int tmpInt = info->subTunes[subTune - 1].tuneLength;
        tuple.set_int (FIELD_LENGTH, (tmpInt < 0) ? -1 : tmpInt * 1000);

#if 0
        tmpInt = info->subTunes[subTune - 1].tuneSpeed;
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

    tuple.set_int (FIELD_SUBSONG_NUM, info->nsubTunes);
    tuple.set_int (FIELD_SUBSONG_ID, subTune);
    tuple.set_int (FIELD_TRACK_NUMBER, subTune);
}


static void xs_fill_subtunes(Tuple &tuple, xs_tuneinfo_t *info)
{
    Index<int> subtunes;

    for (int count = 0; count < info->nsubTunes; count++) {
        if (count + 1 == info->startTune || !xs_cfg.subAutoMinOnly ||
            info->subTunes[count].tuneLength < 0 ||
            info->subTunes[count].tuneLength >= xs_cfg.subAutoMinTime)
            subtunes.append (count + 1);
    }

    tuple.set_subtunes (subtunes.len (), subtunes.begin ());
}

Tuple xs_probe_for_tuple(const char *filename, VFSFile *fd)
{
    Tuple tuple;
    xs_tuneinfo_t *info;
    int tune = -1;

    FileBuffer buf(fd);
    if (!xs_sidplayfp_probe(buf.data, buf.size))
        return tuple;

    /* Get information from URL */
    tuple.set_filename (filename);
    tune = tuple.get_int (FIELD_SUBSONG_ID);

    /* Get tune information from emulation engine */
    if (!(info = xs_sidplayfp_getinfo(filename, buf.data, buf.size)))
        return tuple;

    xs_get_song_tuple_info(tuple, info, tune);

    if (xs_cfg.subAutoEnable && info->nsubTunes > 1 && tune < 0)
        xs_fill_subtunes(tuple, info);

    xs_tuneinfo_free(info);

    return tuple;
}

/*
 * Plugin header
 */
static const char *xs_sid_fmts[] = { "sid", "psid", nullptr };

#define AUD_PLUGIN_NAME        "SID Player"
#define AUD_PLUGIN_INIT        xs_init
#define AUD_PLUGIN_CLEANUP     xs_close
#define AUD_INPUT_IS_OUR_FILE  nullptr
#define AUD_INPUT_PLAY         xs_play_file
#define AUD_INPUT_READ_TUPLE   xs_probe_for_tuple

#define AUD_INPUT_EXTS         xs_sid_fmts
#define AUD_INPUT_SUBTUNES     true

/* medium priority (slow to initialize) */
#define AUD_INPUT_PRIORITY     5

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
