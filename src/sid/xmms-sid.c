/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Main source file

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2007 Tecnic Software productions (TNSP)

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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdarg.h>
#include "xs_config.h"
#include "xs_length.h"
#include "xs_stil.h"
#include "xs_filter.h"
#include "xs_fileinfo.h"
#include "xs_interface.h"
#include "xs_glade.h"
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

#ifdef DEBUG
void XSDEBUG(const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_logv("AUD-SID", G_LOG_LEVEL_DEBUG, fmt, ap);
    va_end(ap);
}
#endif


/*
 * Initialization functions
 */
void xs_reinit(void)
{
    XSDEBUG("xs_reinit() thread = %p\n", g_thread_self());

    /* Stop playing, if we are */
    XS_MUTEX_LOCK(xs_status);
    if (xs_status.isPlaying) {
        XS_MUTEX_UNLOCK(xs_status);
        xs_stop(NULL);
    } else {
        XS_MUTEX_UNLOCK(xs_status);
    }

    XS_MUTEX_LOCK(xs_status);
    XS_MUTEX_LOCK(xs_cfg);

    /* Initialize status and sanitize configuration */
    memset(&xs_status, 0, sizeof(xs_status));

    if (xs_cfg.audioFrequency < 8000)
        xs_cfg.audioFrequency = 8000;

    if (xs_cfg.oversampleFactor < XS_MIN_OVERSAMPLE)
        xs_cfg.oversampleFactor = XS_MIN_OVERSAMPLE;
    else if (xs_cfg.oversampleFactor > XS_MAX_OVERSAMPLE)
        xs_cfg.oversampleFactor = XS_MAX_OVERSAMPLE;

    if (xs_cfg.audioChannels != XS_CHN_MONO)
        xs_cfg.oversampleEnable = FALSE;

    xs_status.audioFrequency = xs_cfg.audioFrequency;
    xs_status.audioBitsPerSample = xs_cfg.audioBitsPerSample;
    xs_status.audioChannels = xs_cfg.audioChannels;
    xs_status.audioFormat = -1;
    xs_status.oversampleEnable = xs_cfg.oversampleEnable;
    xs_status.oversampleFactor = xs_cfg.oversampleFactor;

    /* Try to initialize emulator engine */
    xs_init_emu_engine(&xs_cfg.playerEngine, &xs_status);

    /* Get settings back, in case the chosen emulator backend changed them */
    xs_cfg.audioFrequency = xs_status.audioFrequency;
    xs_cfg.audioBitsPerSample = xs_status.audioBitsPerSample;
    xs_cfg.audioChannels = xs_status.audioChannels;
    xs_cfg.oversampleEnable = xs_status.oversampleEnable;

    XS_MUTEX_UNLOCK(xs_status);
    XS_MUTEX_UNLOCK(xs_cfg);

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

}


/*
 * Initialize XMMS-SID
 */
void xs_init(void)
{
    XSDEBUG("xs_init()\n");

    /* Initialize and get configuration */
    xs_init_configuration();
    xs_read_configuration();

    /* Initialize subsystems */
    xs_reinit();

    XSDEBUG("OK\n");
}


/*
 * Shut down XMMS-SID
 */
void xs_close(void)
{
    XSDEBUG("xs_close(): shutting down...\n");

    /* Stop playing, free structures */
    xs_stop(NULL);

    xs_tuneinfo_free(xs_status.tuneInfo);
    xs_status.tuneInfo = NULL;
    xs_status.sidPlayer->plrDeleteSID(&xs_status);
    xs_status.sidPlayer->plrClose(&xs_status);

    xs_songlen_close();
    xs_stil_close();

    XSDEBUG("shutdown finished.\n");
}


/*
 * Start playing the given file
 */
void xs_play_file(InputPlayback *pb)
{
    xs_tuneinfo_t *tmpTune;
    gboolean audioOpen = FALSE;
    gint audioGot, tmpLength, subTune = -1;
    gchar *tmpFilename, *audioBuffer = NULL, *oversampleBuffer = NULL;
    Tuple *tmpTuple;

    assert(pb);
    assert(xs_status.sidPlayer != NULL);

    XSDEBUG("play '%s'\n", pb->filename);

    tmpFilename = aud_filename_split_subtune(pb->filename, &subTune);
    if (tmpFilename == NULL) return;

    /* Get tune information */
    XS_MUTEX_LOCK(xs_status);
    if ((xs_status.tuneInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename)) == NULL) {
        XS_MUTEX_UNLOCK(xs_status);
        g_free(tmpFilename);
        return;
    }

    /* Initialize the tune */
    if (!xs_status.sidPlayer->plrLoadSID(&xs_status, tmpFilename)) {
        XS_MUTEX_UNLOCK(xs_status);
        g_free(tmpFilename);
        xs_tuneinfo_free(xs_status.tuneInfo);
        xs_status.tuneInfo = NULL;
        return;
    }

    g_free(tmpFilename);
    tmpFilename = NULL;

    XSDEBUG("load ok\n");

    /* Set general status information */
    pb->playing = TRUE;
    xs_status.isPlaying = TRUE;
    pb->error = FALSE;
    pb->eof = FALSE;
    tmpTune = xs_status.tuneInfo;

    if (subTune < 1 || subTune > xs_status.tuneInfo->nsubTunes)
        xs_status.currSong = xs_status.tuneInfo->startTune;
    else
        xs_status.currSong = subTune;

    XSDEBUG("subtune #%i selected (#%d wanted), initializing...\n", xs_status.currSong, subTune);


    /* Allocate audio buffer */
    audioBuffer = (gchar *) g_malloc(XS_AUDIOBUF_SIZE);
    if (audioBuffer == NULL) {
        xs_error("Couldn't allocate memory for audio data buffer!\n");
        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    if (xs_status.oversampleEnable) {
        oversampleBuffer = (gchar *) g_malloc(XS_AUDIOBUF_SIZE * xs_status.oversampleFactor);
        if (oversampleBuffer == NULL) {
            xs_error("Couldn't allocate memory for audio oversampling buffer!\n");
            XS_MUTEX_UNLOCK(xs_status);
            goto xs_err_exit;
        }
    }


    /* Check minimum playtime */
    tmpLength = tmpTune->subTunes[xs_status.currSong - 1].tuneLength;
    if (xs_cfg.playMinTimeEnable && (tmpLength >= 0)) {
        if (tmpLength < xs_cfg.playMinTime)
            tmpLength = xs_cfg.playMinTime;
    }

    /* Initialize song */
    if (!xs_status.sidPlayer->plrInitSong(&xs_status)) {
        xs_error("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n",
            tmpTune->sidFilename, xs_status.currSong);
        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    /* Open the audio output */
    XSDEBUG("open audio output (%d, %d, %d)\n",
        xs_status.audioFormat, xs_status.audioFrequency, xs_status.audioChannels);

    if (!pb->output->open_audio(xs_status.audioFormat, xs_status.audioFrequency, xs_status.audioChannels)) {
        xs_error("Couldn't open audio output (fmt=%x, freq=%i, nchan=%i)!\n",
            xs_status.audioFormat,
            xs_status.audioFrequency,
            xs_status.audioChannels);

        pb->error = TRUE;
        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    audioOpen = TRUE;

    /* Set song information for current subtune */
    XSDEBUG("foobar #1\n");
    xs_status.sidPlayer->plrUpdateSIDInfo(&xs_status);
    tmpTuple = aud_tuple_new_from_filename(tmpTune->sidFilename);
    xs_get_song_tuple_info(tmpTuple, tmpTune, xs_status.currSong);
    XS_MUTEX_UNLOCK(xs_status);
    pb->set_tuple(pb, tmpTuple);

/*
    tmpTitle = aud_tuple_formatter_process_string(tmpTuple,
        xs_cfg.titleOverride ? xs_cfg.titleFormat : aud_get_gentitle_format());
*/

    pb->set_pb_ready(pb);

    XS_MUTEX_UNLOCK(xs_status);
    XSDEBUG("playing\n");
    while (pb->playing) {
        /* Render audio data */
        XS_MUTEX_LOCK(xs_status);
        if (xs_status.oversampleEnable) {
            /* Perform oversampled rendering */
            audioGot = xs_status.sidPlayer->plrFillBuffer(
                &xs_status,
                oversampleBuffer,
                (XS_AUDIOBUF_SIZE * xs_status.oversampleFactor));

            audioGot /= xs_status.oversampleFactor;

            /* Execute rate-conversion with filtering */
            if (xs_filter_rateconv(audioBuffer, oversampleBuffer,
                xs_status.audioFormat, xs_status.oversampleFactor, audioGot) < 0) {
                xs_error("Oversampling rate-conversion pass failed.\n");
                pb->error = TRUE;
                XS_MUTEX_UNLOCK(xs_status);
                goto xs_err_exit;
            }
        } else {
            audioGot = xs_status.sidPlayer->plrFillBuffer(
                &xs_status, audioBuffer, XS_AUDIOBUF_SIZE);
        }

        /* I <3 visualice/haujobb */
        pb->pass_audio(pb, xs_status.audioFormat,
            xs_status.audioChannels,
            audioGot, audioBuffer, NULL);

        XS_MUTEX_UNLOCK(xs_status);

        /* Wait a little */
        while (pb->playing && pb->output->buffer_free() < audioGot)
            g_usleep(500);

        /* Check if we have played enough */
        XS_MUTEX_LOCK(xs_status);
        if (xs_cfg.playMaxTimeEnable) {
            if (xs_cfg.playMaxTimeUnknown) {
                if (tmpLength < 0 &&
                    pb->output->output_time() >= xs_cfg.playMaxTime * 1000)
                    pb->playing = FALSE;
            } else {
                if (pb->output->output_time() >= xs_cfg.playMaxTime * 1000)
                    pb->playing = FALSE;
            }
        }

        if (tmpLength >= 0) {
            if (pb->output->output_time() >= tmpLength * 1000)
                pb->playing = FALSE;
        }
        XS_MUTEX_UNLOCK(xs_status);
    }

xs_err_exit:
    XSDEBUG("out of playing loop\n");

    /* Close audio output plugin */
    if (audioOpen) {
        XSDEBUG("close audio #2\n");
        pb->output->close_audio();
        XSDEBUG("closed\n");
    }

    g_free(audioBuffer);
    g_free(oversampleBuffer);

    /* Set playing status to false (stopped), thus when
     * XMMS next calls xs_get_time(), it can return appropriate
     * value "not playing" status and XMMS knows to move to
     * next entry in the playlist .. or whatever it wishes.
     */
    XS_MUTEX_LOCK(xs_status);
    pb->playing = FALSE;
    xs_status.isPlaying = FALSE;
    pb->eof = TRUE;

    /* Free tune information */
    xs_status.sidPlayer->plrDeleteSID(&xs_status);
    xs_tuneinfo_free(xs_status.tuneInfo);
    xs_status.tuneInfo = NULL;
    XS_MUTEX_UNLOCK(xs_status);

    /* Exit the playing thread */
    XSDEBUG("exiting thread, bye.\n");
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
    XSDEBUG("stop requested\n");

    /* Lock xs_status and stop playing thread */
    XS_MUTEX_LOCK(xs_status);
    if (pb != NULL && pb->playing) {
        XSDEBUG("stopping...\n");
        pb->playing = FALSE;
        xs_status.isPlaying = FALSE;
        XS_MUTEX_UNLOCK(xs_status);
    } else {
        XS_MUTEX_UNLOCK(xs_status);
    }

    XSDEBUG("ok\n");
}


/*
 * Pause/unpause the playing
 */
void xs_pause(InputPlayback *pb, short pauseState)
{
    pb->output->pause(pauseState);
}


/*
 * A stub seek function (Audacious will crash if seek is NULL)
 */
void xs_seek(InputPlayback *pb, gint time)
{
    (void) pb; (void) time;
}


/*
 * Return the playing "position/time"
 * Determine current position/time in song. Used by XMMS to update
 * the song clock and position slider and MOST importantly to determine
 * END OF SONG! Return value of -2 means error, XMMS opens an audio
 * error dialog. -1 means end of song (if one was playing currently).
 */
gint xs_get_time(InputPlayback *pb)
{
    /* If errorflag is set, return -2 to signal it to XMMS's idle callback */
    XS_MUTEX_LOCK(xs_status);
    if (pb->error) {
        XS_MUTEX_UNLOCK(xs_status);
        return -2;
    }

    /* If there is no tune, return -1 */
    if (!xs_status.tuneInfo) {
        XS_MUTEX_UNLOCK(xs_status);
        return -1;
    }

    /* If tune has ended, return -1 */
    if (!pb->playing) {
        XS_MUTEX_UNLOCK(xs_status);
        return -1;
    }

    XS_MUTEX_UNLOCK(xs_status);

    /* Return output time reported by audio output plugin */
    return pb->output->output_time();
}


/*
 * Return song information Tuple
 */
static void xs_get_song_tuple_info(Tuple *tuple, xs_tuneinfo_t *pInfo, gint subTune)
{
    gchar *tmpStr, tmpStr2[64];

    aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, pInfo->sidName);
    aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, pInfo->sidComposer);
    aud_tuple_associate_string(tuple, FIELD_COPYRIGHT, NULL, pInfo->sidCopyright);
    aud_tuple_associate_string(tuple, -1, "sid-format", pInfo->sidFormat);
    aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, "Commodore 64 SID PlaySID/RSID");

    switch (pInfo->sidModel) {
        case XS_SIDMODEL_6581: tmpStr = "6581"; break;
        case XS_SIDMODEL_8580: tmpStr = "8580"; break;
        case XS_SIDMODEL_ANY: tmpStr = "ANY"; break;
        default: tmpStr = "?"; break;
    }
    aud_tuple_associate_string(tuple, -1, "sid-model", tmpStr);

    /* Get sub-tune information, if available */
    if (subTune < 0 || pInfo->startTune > pInfo->nsubTunes)
        subTune = pInfo->startTune;

    if (subTune > 0 && subTune <= pInfo->nsubTunes) {
        gint tmpInt = pInfo->subTunes[subTune - 1].tuneLength;
        aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, (tmpInt < 0) ? -1 : tmpInt * 1000);

        tmpInt = pInfo->subTunes[subTune - 1].tuneSpeed;
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

        aud_tuple_associate_string(tuple, -1, "sid-speed", tmpStr);
    } else
        subTune = 1;

    aud_tuple_associate_int(tuple, FIELD_SUBSONG_NUM, NULL, pInfo->nsubTunes);
    aud_tuple_associate_int(tuple, FIELD_SUBSONG_ID, NULL, subTune);
    aud_tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, subTune);

    if (xs_cfg.titleOverride)
        aud_tuple_associate_string(tuple, FIELD_FORMATTER, NULL, xs_cfg.titleFormat);
}


static void xs_fill_subtunes(Tuple *tuple, xs_tuneinfo_t *info)
{
    gint count, found;
    
    tuple->subtunes = g_new(gint, info->nsubTunes);
    
    for (found = count = 0; count < info->nsubTunes; count++) {
        if (count + 1 == info->startTune || !xs_cfg.subAutoMinOnly ||
            info->subTunes[count].tuneLength >= xs_cfg.subAutoMinTime)
            tuple->subtunes[found++] = count + 1;
    }

    tuple->nsubtunes = found;
}


Tuple * xs_get_song_tuple(const gchar *filename)
{
    Tuple *tuple;
    gchar *tmpFilename;
    xs_tuneinfo_t *tmpInfo;
    gint tmpTune = -1;

    /* Get information from URL */
    tmpFilename = aud_filename_split_subtune(filename, &tmpTune);
    if (tmpFilename == NULL) return NULL;

    tuple = aud_tuple_new_from_filename(tmpFilename);
    if (tuple == NULL) {
        g_free(tmpFilename);
        return NULL;
    }

    /* Get tune information from emulation engine */
    XS_MUTEX_LOCK(xs_status);
    tmpInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename);
    XS_MUTEX_UNLOCK(xs_status);
    g_free(tmpFilename);

    if (tmpInfo == NULL)
        return tuple;

    xs_get_song_tuple_info(tuple, tmpInfo, tmpTune);

    if (xs_cfg.subAutoEnable && tmpInfo->nsubTunes > 1 && tmpTune < 0)
        xs_fill_subtunes(tuple, tmpInfo);

    xs_tuneinfo_free(tmpInfo);

    return tuple;
}


Tuple * xs_probe_for_tuple(const gchar *filename, xs_file_t *fd)
{
    Tuple *tuple;
    gchar *tmpFilename;
    xs_tuneinfo_t *tmpInfo;
    gint tmpTune = -1;

    assert(xs_status.sidPlayer != NULL);

    if (filename == NULL)
        return NULL;

    XS_MUTEX_LOCK(xs_status);
    if (!xs_status.sidPlayer->plrProbe(fd)) {
        XS_MUTEX_UNLOCK(xs_status);
        return NULL;
    }
    XS_MUTEX_UNLOCK(xs_status);

    /* Get information from URL */
    tmpFilename = aud_filename_split_subtune(filename, &tmpTune);
    if (tmpFilename == NULL) return NULL;

    tuple = aud_tuple_new_from_filename(tmpFilename);
    if (tuple == NULL) {
        g_free(tmpFilename);
        return NULL;
    }

    /* Get tune information from emulation engine */
    XS_MUTEX_LOCK(xs_status);
    tmpInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename);
    XS_MUTEX_UNLOCK(xs_status);
    g_free(tmpFilename);

    if (tmpInfo == NULL)
        return tuple;

    xs_get_song_tuple_info(tuple, tmpInfo, tmpTune);

    if (xs_cfg.subAutoEnable && tmpInfo->nsubTunes > 1 && tmpTune < 0)
        xs_fill_subtunes(tuple, tmpInfo);

    xs_tuneinfo_free(tmpInfo);

    return tuple;
}
