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
#include <audacious/input.h>
#include <audacious/plugin.h>

#include "xs_sidplay2.h"


/*
 * Global variables
 */
xs_status_t xs_status;
pthread_mutex_t xs_status_mutex = PTHREAD_MUTEX_INITIALIZER;

static void xs_get_song_tuple_info(Tuple *pResult, xs_tuneinfo_t *pInfo, int subTune);

/*
 * Initialization functions
 */
bool_t xs_init(void)
{
    bool_t success;

    /* Initialize and get configuration */
    xs_init_configuration();

    pthread_mutex_lock(&xs_status_mutex);
    pthread_mutex_lock(&xs_cfg_mutex);

    /* Initialize status and sanitize configuration */
    memset(&xs_status, 0, sizeof(xs_status));

    if (xs_cfg.audioFrequency < 8000)
        xs_cfg.audioFrequency = 8000;

    xs_status.audioFrequency = xs_cfg.audioFrequency;
    xs_status.audioChannels = xs_cfg.audioChannels;

    /* Try to initialize emulator engine */
    success = xs_sidplayfp_init(&xs_status);

    /* Get settings back, in case the chosen emulator backend changed them */
    xs_cfg.audioFrequency = xs_status.audioFrequency;
    xs_cfg.audioChannels = xs_status.audioChannels;

    pthread_mutex_unlock(&xs_status_mutex);
    pthread_mutex_unlock(&xs_cfg_mutex);

    if (! success)
        return FALSE;

    /* Initialize song-length database */
    xs_songlen_close();
    if (xs_cfg.songlenDBEnable && (xs_songlen_init() != 0)) {
        xs_error("Error initializing song-length database!\n");
    }

    /* Initialize STIL database */
    xs_stil_close();
    if (xs_cfg.stilDBEnable && (xs_stil_init() != 0)) {
        xs_error("Error initializing STIL database!\n");
    }

    return TRUE;
}


/*
 * Shut down XMMS-SID
 */
void xs_close(void)
{
    xs_tuneinfo_free(xs_status.tuneInfo);
    xs_status.tuneInfo = NULL;

    xs_sidplayfp_delete (& xs_status);
    xs_sidplayfp_close (& xs_status);

    xs_songlen_close();
    xs_stil_close();
}


/*
 * Start playing the given file
 */
bool_t xs_play_file(const char *filename, VFSFile *file)
{
    xs_tuneinfo_t *tmpTune;
    int audioBufSize, bufRemaining, tmpLength, subTune = -1;
    char *audioBuffer = NULL, *oversampleBuffer = NULL;
    Tuple *tmpTuple;

    uri_parse (filename, NULL, NULL, NULL, & subTune);

    /* Get tune information */
    pthread_mutex_lock(&xs_status_mutex);

    if (! (xs_status.tuneInfo = xs_sidplayfp_getinfo (filename)))
    {
        pthread_mutex_unlock(&xs_status_mutex);
        return FALSE;
    }

    /* Initialize the tune */
    if (! xs_sidplayfp_load (& xs_status, filename))
    {
        pthread_mutex_unlock(&xs_status_mutex);
        xs_tuneinfo_free(xs_status.tuneInfo);
        xs_status.tuneInfo = NULL;
        return FALSE;
    }

    bool_t error = FALSE;

    /* Set general status information */
    tmpTune = xs_status.tuneInfo;

    if (subTune < 1 || subTune > xs_status.tuneInfo->nsubTunes)
        xs_status.currSong = xs_status.tuneInfo->startTune;
    else
        xs_status.currSong = subTune;

    int channels = xs_status.audioChannels;

    /* Allocate audio buffer */
    audioBufSize = xs_status.audioFrequency * channels * FMT_SIZEOF (FMT_S16_NE);
    if (audioBufSize < 512) audioBufSize = 512;

    audioBuffer = (char *) malloc(audioBufSize);
    if (audioBuffer == NULL) {
        xs_error("Couldn't allocate memory for audio data buffer!\n");
        pthread_mutex_unlock(&xs_status_mutex);
        goto xs_err_exit;
    }

    /* Check minimum playtime */
    tmpLength = tmpTune->subTunes[xs_status.currSong - 1].tuneLength;
    if (xs_cfg.playMinTimeEnable && (tmpLength >= 0)) {
        if (tmpLength < xs_cfg.playMinTime)
            tmpLength = xs_cfg.playMinTime;
    }

    /* Initialize song */
    if (!xs_sidplayfp_initsong(&xs_status)) {
        xs_error("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n",
            tmpTune->sidFilename, xs_status.currSong);
        pthread_mutex_unlock(&xs_status_mutex);
        goto xs_err_exit;
    }

    /* Open the audio output */
    if (!aud_input_open_audio(FMT_S16_NE, xs_status.audioFrequency, channels))
    {
        xs_error("Couldn't open audio output (fmt=%x, freq=%i, nchan=%i)!\n",
            FMT_S16_NE,
            xs_status.audioFrequency,
            channels);

        pthread_mutex_unlock(&xs_status_mutex);
        goto xs_err_exit;
    }

    /* Set song information for current subtune */
    xs_sidplayfp_updateinfo(&xs_status);
    tmpTuple = tuple_new_from_filename(tmpTune->sidFilename);
    xs_get_song_tuple_info(tmpTuple, tmpTune, xs_status.currSong);

    pthread_mutex_unlock(&xs_status_mutex);

    aud_input_set_tuple(tmpTuple);

    while (! aud_input_check_stop ())
    {
        bufRemaining = xs_sidplayfp_fillbuffer(&xs_status, audioBuffer, audioBufSize);

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
    free(audioBuffer);
    free(oversampleBuffer);

    /* Set playing status to false (stopped), thus when
     * XMMS next calls xs_get_time(), it can return appropriate
     * value "not playing" status and XMMS knows to move to
     * next entry in the playlist .. or whatever it wishes.
     */
    pthread_mutex_lock(&xs_status_mutex);

    /* Free tune information */
    xs_sidplayfp_delete(&xs_status);
    xs_tuneinfo_free(xs_status.tuneInfo);
    xs_status.tuneInfo = NULL;
    pthread_mutex_unlock(&xs_status_mutex);

    /* Exit the playing thread */
    return ! error;

xs_err_exit:
    error = TRUE;
    goto DONE;
}


/*
 * Return song information Tuple
 */
static void xs_get_song_tuple_info(Tuple *tuple, xs_tuneinfo_t *info, int subTune)
{
    tuple_set_str(tuple, FIELD_TITLE, info->sidName);
    tuple_set_str(tuple, FIELD_ARTIST, info->sidComposer);
    tuple_set_str(tuple, FIELD_COPYRIGHT, info->sidCopyright);
    tuple_set_str(tuple, FIELD_CODEC, info->sidFormat);

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
        tuple_set_int(tuple, FIELD_LENGTH, (tmpInt < 0) ? -1 : tmpInt * 1000);

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

    tuple_set_int(tuple, FIELD_SUBSONG_NUM, info->nsubTunes);
    tuple_set_int(tuple, FIELD_SUBSONG_ID, subTune);
    tuple_set_int(tuple, FIELD_TRACK_NUMBER, subTune);
}


static void xs_fill_subtunes(Tuple *tuple, xs_tuneinfo_t *info)
{
    int count, found;
    int subtunes[info->nsubTunes];

    for (found = count = 0; count < info->nsubTunes; count++) {
        if (count + 1 == info->startTune || !xs_cfg.subAutoMinOnly ||
            info->subTunes[count].tuneLength < 0 ||
            info->subTunes[count].tuneLength >= xs_cfg.subAutoMinTime)
            subtunes[found ++] = count + 1;
    }

    tuple_set_subtunes (tuple, found, subtunes);
}

Tuple * xs_probe_for_tuple(const char *filename, VFSFile *fd)
{
    Tuple *tuple;
    xs_tuneinfo_t *info;
    int tune = -1;

    pthread_mutex_lock(&xs_status_mutex);
    if (!xs_sidplayfp_probe(fd)) {
        pthread_mutex_unlock(&xs_status_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&xs_status_mutex);

    /* Get information from URL */
    tuple = tuple_new_from_filename (filename);
    tune = tuple_get_int (tuple, FIELD_SUBSONG_ID);

    /* Get tune information from emulation engine */
    pthread_mutex_lock(&xs_status_mutex);
    info = xs_sidplayfp_getinfo (filename);
    pthread_mutex_unlock(&xs_status_mutex);

    if (info == NULL)
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
static const char *xs_sid_fmts[] = { "sid", "psid", NULL };

AUD_INPUT_PLUGIN
(
    .name = "SID Player",               /* Plugin description */
    .domain = PACKAGE,
    .init = xs_init,                    /* Initialization */
    .cleanup = xs_close,                /* Cleanup */
    .play = xs_play_file,               /* Play given file */
    .probe_for_tuple = xs_probe_for_tuple,

    .extensions = xs_sid_fmts,          /* File ext assist */
    .have_subtune = TRUE,

    /* medium priority (slow to initialize) */
    .priority = 5,
)
