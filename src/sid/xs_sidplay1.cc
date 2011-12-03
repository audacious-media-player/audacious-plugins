/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   libSIDPlay v1 support

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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "xmms-sid.h"

#ifdef HAVE_SIDPLAY1

#include <stdio.h>
#include "xs_sidplay1.h"
#include "xs_config.h"

#include <sidplay/player.h>
#include <sidplay/myendian.h>
#include <sidplay/fformat.h>


/* Maximum audio frequency supported by libSIDPlay v1 */
#define SIDPLAY1_MAX_FREQ    (48000)


typedef struct {
    emuEngine *currEng;
    emuConfig currConfig;
    sidTune *currTune;
    guint8 *buf;
    size_t bufSize;
} xs_sidplay1_t;


/* We need to 'export' all this pseudo-C++ crap */
extern "C" {


/* Return song information
 */
#define TFUNCTION   xs_sidplay1_getinfo
#define TFUNCTION2  xs_sidplay1_updateinfo
#define TTUNEINFO   sidTuneInfo
#define TTUNE       sidTune
#define TENGINE     xs_sidplay1_t
#include "xs_sidplay.h"


/* Check if we can play the given file
 */
gboolean xs_sidplay1_probe(xs_file_t *f)
{
    gchar tmpBuf[4];

    if (!f) return FALSE;

    if (xs_fread(tmpBuf, sizeof(gchar), 4, f) != 4)
        return FALSE;

    if (!strncmp(tmpBuf, "PSID", 4))
        return TRUE;
    else
        return FALSE;
}


/* Initialize SIDPlay1
 */
gboolean xs_sidplay1_init(xs_status_t * status)
{
    gint tmpFreq;
    xs_sidplay1_t *engine;
    assert(status != NULL);

    /* Allocate internal structures */
    engine = (xs_sidplay1_t *) g_malloc0(sizeof(xs_sidplay1_t));
    if (!engine) return FALSE;

    /* Initialize engine */
    engine->currEng = new emuEngine();
    if (!engine->currEng) {
        xs_error("[SIDPlay1] Could not initialize emulation engine.\n");
        g_free(engine);
        return FALSE;
    }

    /* Verify endianess */
    if (!engine->currEng->verifyEndianess()) {
        xs_error("[SIDPlay1] Endianess verification failed.\n");
        delete engine->currEng;
        g_free(engine);
        return FALSE;
    }

    status->sidEngine = engine;

    /* Get current configuration */
    engine->currEng->getConfig(engine->currConfig);

    /* Configure channel parameters */
    switch (status->audioChannels) {

    case XS_CHN_AUTOPAN:
        engine->currConfig.channels = SIDEMU_STEREO;
        engine->currConfig.autoPanning = SIDEMU_CENTEREDAUTOPANNING;
        engine->currConfig.volumeControl = SIDEMU_FULLPANNING;
        break;

    case XS_CHN_STEREO:
        engine->currConfig.channels = SIDEMU_STEREO;
        engine->currConfig.autoPanning = SIDEMU_NONE;
        engine->currConfig.volumeControl = SIDEMU_NONE;
        break;

    case XS_CHN_MONO:
    default:
        engine->currConfig.channels = SIDEMU_MONO;
        engine->currConfig.autoPanning = SIDEMU_NONE;
        engine->currConfig.volumeControl = SIDEMU_NONE;
        status->audioChannels = XS_CHN_MONO;
        break;
    }


    /* Memory mode settings */
    switch (xs_cfg.memoryMode) {
    case XS_MPU_TRANSPARENT_ROM:
        engine->currConfig.memoryMode = MPU_TRANSPARENT_ROM;
        break;

    case XS_MPU_PLAYSID_ENVIRONMENT:
        engine->currConfig.memoryMode = MPU_PLAYSID_ENVIRONMENT;
        break;

    case XS_MPU_BANK_SWITCHING:
    default:
        engine->currConfig.memoryMode = MPU_BANK_SWITCHING;
        xs_cfg.memoryMode = XS_MPU_BANK_SWITCHING;
        break;
    }


    /* Audio parameters sanity checking and setup */
    engine->currConfig.bitsPerSample = status->audioBitsPerSample;
    tmpFreq = status->audioFrequency;

    if (status->oversampleEnable) {
        if ((tmpFreq * status->oversampleFactor) > SIDPLAY1_MAX_FREQ) {
            status->oversampleEnable = FALSE;
        } else {
            tmpFreq = (tmpFreq * status->oversampleFactor);
        }
    } else {
        if (tmpFreq > SIDPLAY1_MAX_FREQ)
            tmpFreq = SIDPLAY1_MAX_FREQ;
    }

    engine->currConfig.frequency = tmpFreq;

    switch (status->audioBitsPerSample) {
    case XS_RES_8BIT:
        switch (status->audioFormat) {
        case FMT_S8:
            status->audioFormat = FMT_S8;
            engine->currConfig.sampleFormat = SIDEMU_SIGNED_PCM;
            break;

        case FMT_U8:
        default:
            status->audioFormat = FMT_U8;
            engine->currConfig.sampleFormat = SIDEMU_UNSIGNED_PCM;
            break;
        }
        break;

    case XS_RES_16BIT:
    default:
        switch (status->audioFormat) {
        case FMT_U16_LE:
        case FMT_U16_BE:
            status->audioFormat = FMT_U16_NE;
            engine->currConfig.sampleFormat = SIDEMU_UNSIGNED_PCM;
            break;

        case FMT_S16_LE:
        case FMT_S16_BE:
        default:
            status->audioFormat = FMT_S16_NE;
            engine->currConfig.sampleFormat = SIDEMU_SIGNED_PCM;
            break;
        }
        break;
    }

    /* Clockspeed settings */
    switch (xs_cfg.clockSpeed) {
    case XS_CLOCK_NTSC:
        engine->currConfig.clockSpeed = SIDTUNE_CLOCK_NTSC;
        break;

    case XS_CLOCK_PAL:
    default:
        engine->currConfig.clockSpeed = SIDTUNE_CLOCK_PAL;
        xs_cfg.clockSpeed = XS_CLOCK_PAL;
        break;
    }

    engine->currConfig.forceSongSpeed = xs_cfg.forceSpeed;


    /* Configure rest of the emulation */
    /* if (xs_cfg.forceModel) */
    engine->currConfig.mos8580 = xs_cfg.mos8580;
    engine->currConfig.emulateFilter = xs_cfg.emulateFilters;
    engine->currConfig.filterFs = xs_cfg.sid1Filter.fs;
    engine->currConfig.filterFm = xs_cfg.sid1Filter.fm;
    engine->currConfig.filterFt = xs_cfg.sid1Filter.ft;


    /* Now set the emulator configuration */
    if (!engine->currEng->setConfig(engine->currConfig)) {
        xs_error("[SIDPlay1] Emulator engine configuration failed!\n");
        return FALSE;
    }

    /* Create sidtune object */
    engine->currTune = new sidTune(0);
    if (!engine->currTune) {
        xs_error("[SIDPlay1] Could not initialize SIDTune object.\n");
        return FALSE;
    }

    return TRUE;
}


/* Close SIDPlay1 engine
 */
void xs_sidplay1_close(xs_status_t * status)
{
    xs_sidplay1_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay1_t *) status->sidEngine;

    /* Free internals */
    if (engine->currEng) {
        delete engine->currEng;
        engine->currEng = NULL;
    }

    if (engine->currTune) {
        delete engine->currTune;
        engine->currTune = NULL;
    }

    xs_sidplay1_delete(status);

    g_free(engine);
    status->sidEngine = NULL;
}


/* Initialize current song and sub-tune
 */
gboolean xs_sidplay1_initsong(xs_status_t * status)
{
    xs_sidplay1_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay1_t *) status->sidEngine;
    if (!engine) return FALSE;

    if (!engine->currTune) {
        xs_error("[SIDPlay1] SID-tune struct pointer was NULL. This should not happen, report to XMMS-SID author.\n");
        return FALSE;
    }

    if (!engine->currTune->getStatus()) {
        xs_error("[SIDPlay1] SID-tune status check failed. This should not happen, report to XMMS-SID author.\n");
        return FALSE;
    }

    status->isInitialized = TRUE;

    return sidEmuInitializeSong(*engine->currEng, *engine->currTune, status->currSong);
}


/* Emulate and render audio data to given buffer
 */
guint xs_sidplay1_fillbuffer(xs_status_t * status, gchar * audioBuffer, guint audioBufSize)
{
    xs_sidplay1_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay1_t *) status->sidEngine;
    if (!engine) return 0;

    sidEmuFillBuffer(*engine->currEng, *engine->currTune, audioBuffer, audioBufSize);

    return audioBufSize;
}


/* Load a given SID-tune file
 */
gboolean xs_sidplay1_load(xs_status_t * status, const gchar * filename)
{
    xs_sidplay1_t *engine;
    assert(status != NULL);
    status->isInitialized = FALSE;

    engine = (xs_sidplay1_t *) status->sidEngine;
    if (!engine) return FALSE;

    /* Try to get the tune */
    if (!filename) return FALSE;

    if (xs_fload_buffer(filename, &(engine->buf), &(engine->bufSize)) != 0)
        return FALSE;

    if (!engine->currTune->load(engine->buf, engine->bufSize))
        return FALSE;

    return TRUE;
}


/* Delete INTERNAL information
 */
void xs_sidplay1_delete(xs_status_t * status)
{
    xs_sidplay1_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay1_t *) status->sidEngine;
    if (engine == NULL) return;

    g_free(engine->buf);
    engine->buf = NULL;
    engine->bufSize = 0;
}


}    /* extern "C" */
#endif    /* HAVE_SIDPLAY1 */
