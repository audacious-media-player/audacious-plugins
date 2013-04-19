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
#include "xmms-sid.h"

#include <stdlib.h>
#include <stdarg.h>

#include <audacious/debug.h>
#include <libaudcore/audstrings.h>

#include "xs_config.h"
#include "xs_length.h"
#include "xs_sidplay2.h"
#include "xs_stil.h"
#include "xs_player.h"
#include "xs_slsup.h"


/*
 * Global variables
 */
xs_status_t xs_status;
XS_MUTEX(xs_status);

static void xs_get_song_tuple_info(Tuple *pResult, xs_tuneinfo_t *pInfo, gint subTune);

/*
 * Error messages
 */
void xs_verror(const gchar *fmt, va_list ap)
{
    g_logv("AUD-SID", G_LOG_LEVEL_WARNING, fmt, ap);
}

void xs_error(const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xs_verror(fmt, ap);
    va_end(ap);
}


/*
 * Initialization functions
 */
gboolean xs_init(void)
{
    gboolean success;

    /* Initialize and get configuration */
    xs_init_configuration();

    XS_MUTEX_LOCK(xs_status);
    XS_MUTEX_LOCK(xs_cfg);

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

    XS_MUTEX_UNLOCK(xs_status);
    XS_MUTEX_UNLOCK(xs_cfg);

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
gboolean xs_play_file(InputPlayback *pb, const gchar *filename, VFSFile *file, gint start_time, gint stop_time, gboolean pause)
{
    xs_tuneinfo_t *tmpTune;
    gint audioBufSize, bufRemaining, tmpLength, subTune = -1;
    gchar *audioBuffer = NULL, *oversampleBuffer = NULL;
    Tuple *tmpTuple;

    assert(pb);

    uri_parse (filename, NULL, NULL, NULL, & subTune);

    /* Get tune information */
    XS_MUTEX_LOCK(xs_status);

    if (! (xs_status.tuneInfo = xs_sidplayfp_getinfo (filename)))
    {
        XS_MUTEX_UNLOCK(xs_status);
        return FALSE;
    }

    /* Initialize the tune */
    if (! xs_sidplayfp_load (& xs_status, filename))
    {
        XS_MUTEX_UNLOCK(xs_status);
        xs_tuneinfo_free(xs_status.tuneInfo);
        xs_status.tuneInfo = NULL;
        return FALSE;
    }

    gboolean error = FALSE;

    /* Set general status information */
    tmpTune = xs_status.tuneInfo;

    if (subTune < 1 || subTune > xs_status.tuneInfo->nsubTunes)
        xs_status.currSong = xs_status.tuneInfo->startTune;
    else
        xs_status.currSong = subTune;

    gint channels = xs_status.audioChannels;

    /* Allocate audio buffer */
    audioBufSize = xs_status.audioFrequency * channels * FMT_SIZEOF (FMT_S16_NE);
    if (audioBufSize < 512) audioBufSize = 512;

    audioBuffer = (gchar *) g_malloc(audioBufSize);
    if (audioBuffer == NULL) {
        xs_error("Couldn't allocate memory for audio data buffer!\n");
        XS_MUTEX_UNLOCK(xs_status);
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
        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    /* Open the audio output */
    if (!pb->output->open_audio(FMT_S16_NE, xs_status.audioFrequency,
     channels))
    {
        xs_error("Couldn't open audio output (fmt=%x, freq=%i, nchan=%i)!\n",
            FMT_S16_NE,
            xs_status.audioFrequency,
            channels);

        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    /* Set song information for current subtune */
    xs_sidplayfp_updateinfo(&xs_status);
    tmpTuple = tuple_new_from_filename(tmpTune->sidFilename);
    xs_get_song_tuple_info(tmpTuple, tmpTune, xs_status.currSong);

    xs_status.stop_flag = FALSE;
    XS_MUTEX_UNLOCK(xs_status);

    pb->set_tuple(pb, tmpTuple);
    pb->set_params (pb, -1, xs_status.audioFrequency, channels);
    pb->set_pb_ready(pb);

    while (1)
    {
        XS_MUTEX_LOCK (xs_status);

        if (xs_status.stop_flag)
        {
            XS_MUTEX_UNLOCK (xs_status);
            break;
        }

        XS_MUTEX_UNLOCK (xs_status);

        bufRemaining = xs_sidplayfp_fillbuffer(&xs_status, audioBuffer, audioBufSize);

        pb->output->write_audio (audioBuffer, bufRemaining);

        /* Check if we have played enough */
        if (xs_cfg.playMaxTimeEnable) {
            if (xs_cfg.playMaxTimeUnknown) {
                if (tmpLength < 0 &&
                    pb->output->written_time() >= xs_cfg.playMaxTime * 1000)
                    break;
            } else {
                if (pb->output->written_time() >= xs_cfg.playMaxTime * 1000)
                    break;
            }
        }

        if (tmpLength >= 0) {
            if (pb->output->written_time() >= tmpLength * 1000)
                break;
        }
    }

DONE:
    g_free(audioBuffer);
    g_free(oversampleBuffer);

    /* Set playing status to false (stopped), thus when
     * XMMS next calls xs_get_time(), it can return appropriate
     * value "not playing" status and XMMS knows to move to
     * next entry in the playlist .. or whatever it wishes.
     */
    XS_MUTEX_LOCK(xs_status);
    xs_status.stop_flag = TRUE;

    /* Free tune information */
    xs_sidplayfp_delete(&xs_status);
    xs_tuneinfo_free(xs_status.tuneInfo);
    xs_status.tuneInfo = NULL;
    XS_MUTEX_UNLOCK(xs_status);

    /* Exit the playing thread */
    return ! error;

xs_err_exit:
    error = TRUE;
    goto DONE;
}


/*
 * Stop playing
 * Here we set the playing status to stop and wait for playing
 * thread to shut down. In any "correctly" done plugin, this is
 * also the function where you close the output-plugin, but since
 * XMMS-SID has special behaviour (audio opened/closed in the
 * playing thread), we don't do that here.
 *
 * Finally tune and other memory allocations are free'd.
 */
void xs_stop(InputPlayback *pb)
{
    /* Lock xs_status and stop playing thread */
    XS_MUTEX_LOCK(xs_status);

    if (! xs_status.stop_flag)
    {
        xs_status.stop_flag = TRUE;
        pb->output->abort_write ();
    }

    XS_MUTEX_UNLOCK (xs_status);
}


/*
 * Pause/unpause the playing
 */
void xs_pause (InputPlayback * pb, gboolean pauseState)
{
    XS_MUTEX_LOCK(xs_status);
    pb->output->pause(pauseState);
    XS_MUTEX_UNLOCK(xs_status);
}


/*
 * Return song information Tuple
 */
static void xs_get_song_tuple_info(Tuple *tuple, xs_tuneinfo_t *info, gint subTune)
{
    gchar *tmpStr;

    tmpStr = str_to_utf8(info->sidName);
    tuple_set_str(tuple, FIELD_TITLE, NULL, tmpStr);
    g_free(tmpStr);
    tmpStr = str_to_utf8(info->sidComposer);
    tuple_set_str(tuple, FIELD_ARTIST, NULL, tmpStr);
    g_free(tmpStr);
    tmpStr = str_to_utf8(info->sidCopyright);
    tuple_set_str(tuple, FIELD_COPYRIGHT, NULL, tmpStr);
    g_free(tmpStr);
    tuple_set_str(tuple, FIELD_CODEC, NULL, info->sidFormat);

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
        gint tmpInt = info->subTunes[subTune - 1].tuneLength;
        tuple_set_int(tuple, FIELD_LENGTH, NULL, (tmpInt < 0) ? -1 : tmpInt * 1000);

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
                g_snprintf(tmpStr2, sizeof(tmpStr2), "%dHz", tmpInt);
                tmpStr = tmpStr2;
                break;
            }
        } else
            tmpStr = "?";

        tuple_set_str(tuple, -1, "sid-speed", tmpStr);
#endif
    } else
        subTune = 1;

    tuple_set_int(tuple, FIELD_SUBSONG_NUM, NULL, info->nsubTunes);
    tuple_set_int(tuple, FIELD_SUBSONG_ID, NULL, subTune);
    tuple_set_int(tuple, FIELD_TRACK_NUMBER, NULL, subTune);
}


static void xs_fill_subtunes(Tuple *tuple, xs_tuneinfo_t *info)
{
    gint count, found;
    gint subtunes[info->nsubTunes];

    for (found = count = 0; count < info->nsubTunes; count++) {
        if (count + 1 == info->startTune || !xs_cfg.subAutoMinOnly ||
            info->subTunes[count].tuneLength < 0 ||
            info->subTunes[count].tuneLength >= xs_cfg.subAutoMinTime)
            subtunes[found ++] = count + 1;
    }

    tuple_set_subtunes (tuple, found, subtunes);
}

Tuple * xs_probe_for_tuple(const gchar *filename, xs_file_t *fd)
{
    Tuple *tuple;
    xs_tuneinfo_t *info;
    gint tune = -1;

    XS_MUTEX_LOCK(xs_status);
    if (!xs_sidplayfp_probe(fd)) {
        XS_MUTEX_UNLOCK(xs_status);
        return NULL;
    }
    XS_MUTEX_UNLOCK(xs_status);

    /* Get information from URL */
    tuple = tuple_new_from_filename (filename);
    tune = tuple_get_int (tuple, FIELD_SUBSONG_NUM, NULL);

    /* Get tune information from emulation engine */
    XS_MUTEX_LOCK(xs_status);
    info = xs_sidplayfp_getinfo (filename);
    XS_MUTEX_UNLOCK(xs_status);

    if (info == NULL)
        return tuple;

    xs_get_song_tuple_info(tuple, info, tune);

    if (xs_cfg.subAutoEnable && info->nsubTunes > 1 && ! tune)
        xs_fill_subtunes(tuple, info);

    xs_tuneinfo_free(info);

    return tuple;
}
