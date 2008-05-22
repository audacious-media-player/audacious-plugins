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
gboolean xs_sidplay1_init(xs_status_t * myStatus)
{
    gint tmpFreq;
    xs_sidplay1_t *myEngine;
    assert(myStatus);

    /* Allocate internal structures */
    myEngine = (xs_sidplay1_t *) g_malloc0(sizeof(xs_sidplay1_t));
    if (!myEngine) return FALSE;

    /* Initialize engine */
    myEngine->currEng = new emuEngine();
    if (!myEngine->currEng) {
        xs_error("[SIDPlay1] Could not initialize emulation engine.\n");
        g_free(myEngine);
        return FALSE;
    }

    /* Verify endianess */
    if (!myEngine->currEng->verifyEndianess()) {
        xs_error("[SIDPlay1] Endianess verification failed.\n");
        delete myEngine->currEng;
        g_free(myEngine);
        return FALSE;
    }

    myStatus->sidEngine = myEngine;

    /* Get current configuration */
    myEngine->currEng->getConfig(myEngine->currConfig);

    /* Configure channel parameters */
    switch (myStatus->audioChannels) {

    case XS_CHN_AUTOPAN:
        myEngine->currConfig.channels = SIDEMU_STEREO;
        myEngine->currConfig.autoPanning = SIDEMU_CENTEREDAUTOPANNING;
        myEngine->currConfig.volumeControl = SIDEMU_FULLPANNING;
        break;

    case XS_CHN_STEREO:
        myEngine->currConfig.channels = SIDEMU_STEREO;
        myEngine->currConfig.autoPanning = SIDEMU_NONE;
        myEngine->currConfig.volumeControl = SIDEMU_NONE;
        break;

    case XS_CHN_MONO:
    default:
        myEngine->currConfig.channels = SIDEMU_MONO;
        myEngine->currConfig.autoPanning = SIDEMU_NONE;
        myEngine->currConfig.volumeControl = SIDEMU_NONE;
        myStatus->audioChannels = XS_CHN_MONO;
        break;
    }


    /* Memory mode settings */
    switch (xs_cfg.memoryMode) {
    case XS_MPU_TRANSPARENT_ROM:
        myEngine->currConfig.memoryMode = MPU_TRANSPARENT_ROM;
        break;

    case XS_MPU_PLAYSID_ENVIRONMENT:
        myEngine->currConfig.memoryMode = MPU_PLAYSID_ENVIRONMENT;
        break;

    case XS_MPU_BANK_SWITCHING:
    default:
        myEngine->currConfig.memoryMode = MPU_BANK_SWITCHING;
        xs_cfg.memoryMode = XS_MPU_BANK_SWITCHING;
        break;
    }


    /* Audio parameters sanity checking and setup */
    myEngine->currConfig.bitsPerSample = myStatus->audioBitsPerSample;
    tmpFreq = myStatus->audioFrequency;

    if (myStatus->oversampleEnable) {
        if ((tmpFreq * myStatus->oversampleFactor) > SIDPLAY1_MAX_FREQ) {
            myStatus->oversampleEnable = FALSE;
        } else {
            tmpFreq = (tmpFreq * myStatus->oversampleFactor);
        }
    } else {
        if (tmpFreq > SIDPLAY1_MAX_FREQ)
            tmpFreq = SIDPLAY1_MAX_FREQ;
    }

    myEngine->currConfig.frequency = tmpFreq;

    switch (myStatus->audioBitsPerSample) {
    case XS_RES_8BIT:
        switch (myStatus->audioFormat) {
        case FMT_S8:
            myStatus->audioFormat = FMT_S8;
            myEngine->currConfig.sampleFormat = SIDEMU_SIGNED_PCM;
            break;

        case FMT_U8:
        default:
            myStatus->audioFormat = FMT_U8;
            myEngine->currConfig.sampleFormat = SIDEMU_UNSIGNED_PCM;
            break;
        }
        break;

    case XS_RES_16BIT:
    default:
        switch (myStatus->audioFormat) {
        case FMT_U16_NE:
        case FMT_U16_LE:
        case FMT_U16_BE:
            myStatus->audioFormat = FMT_U16_NE;
            myEngine->currConfig.sampleFormat = SIDEMU_UNSIGNED_PCM;
            break;

        case FMT_S16_NE:
        case FMT_S16_LE:
        case FMT_S16_BE:
        default:
            myStatus->audioFormat = FMT_S16_NE;
            myEngine->currConfig.sampleFormat = SIDEMU_SIGNED_PCM;
            break;
        }
        break;
    }

    /* Clockspeed settings */
    switch (xs_cfg.clockSpeed) {
    case XS_CLOCK_NTSC:
        myEngine->currConfig.clockSpeed = SIDTUNE_CLOCK_NTSC;
        break;

    case XS_CLOCK_PAL:
    default:
        myEngine->currConfig.clockSpeed = SIDTUNE_CLOCK_PAL;
        xs_cfg.clockSpeed = XS_CLOCK_PAL;
        break;
    }

    myEngine->currConfig.forceSongSpeed = xs_cfg.forceSpeed;
    
    
    /* Configure rest of the emulation */
    /* if (xs_cfg.forceModel) */
    myEngine->currConfig.mos8580 = xs_cfg.mos8580;
    myEngine->currConfig.emulateFilter = xs_cfg.emulateFilters;
    myEngine->currConfig.filterFs = xs_cfg.sid1FilterFs;
    myEngine->currConfig.filterFm = xs_cfg.sid1FilterFm;
    myEngine->currConfig.filterFt = xs_cfg.sid1FilterFt;


    /* Now set the emulator configuration */
    if (!myEngine->currEng->setConfig(myEngine->currConfig)) {
        xs_error("[SIDPlay1] Emulator engine configuration failed!\n");
        return FALSE;
    }
    
    /* Create sidtune object */
    myEngine->currTune = new sidTune(0);
    if (!myEngine->currTune) {
        xs_error("[SIDPlay1] Could not initialize SIDTune object.\n");
        return FALSE;
    }
    
    return TRUE;
}


/* Close SIDPlay1 engine
 */
void xs_sidplay1_close(xs_status_t * myStatus)
{
    xs_sidplay1_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay1_t *) myStatus->sidEngine;

    /* Free internals */
    if (myEngine->currEng) {
        delete myEngine->currEng;
        myEngine->currEng = NULL;
    }

    if (myEngine->currTune) {
        delete myEngine->currTune;
        myEngine->currTune = NULL;
    }

    xs_sidplay1_delete(myStatus);
    
    g_free(myEngine);
    myStatus->sidEngine = NULL;
}


/* Initialize current song and sub-tune
 */
gboolean xs_sidplay1_initsong(xs_status_t * myStatus)
{
    xs_sidplay1_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay1_t *) myStatus->sidEngine;
    if (!myEngine) return FALSE;

    if (!myEngine->currTune) {
        xs_error("[SIDPlay1] SID-tune struct pointer was NULL. This should not happen, report to XMMS-SID author.\n");
        return FALSE;
    }

    if (!myEngine->currTune->getStatus()) {
        xs_error("[SIDPlay1] SID-tune status check failed. This should not happen, report to XMMS-SID author.\n");
        return FALSE;
    }

    myStatus->isInitialized = TRUE;

    return sidEmuInitializeSong(*myEngine->currEng, *myEngine->currTune, myStatus->currSong);
}


/* Emulate and render audio data to given buffer
 */
guint xs_sidplay1_fillbuffer(xs_status_t * myStatus, gchar * audioBuffer, guint audioBufSize)
{
    xs_sidplay1_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay1_t *) myStatus->sidEngine;
    if (!myEngine) return 0;

    sidEmuFillBuffer(*myEngine->currEng, *myEngine->currTune, audioBuffer, audioBufSize);

    return audioBufSize;
}


/* Load a given SID-tune file
 */
gboolean xs_sidplay1_load(xs_status_t * myStatus, gchar * filename)
{
    xs_sidplay1_t *myEngine;
    assert(myStatus);
    myStatus->isInitialized = FALSE;

    myEngine = (xs_sidplay1_t *) myStatus->sidEngine;
    if (!myEngine) return FALSE;

    /* Try to get the tune */
    if (!filename) return FALSE;
    
    if (xs_fload_buffer(filename, &(myEngine->buf), &(myEngine->bufSize)) != 0)
        return FALSE;
    
    if (!myEngine->currTune->load(myEngine->buf, myEngine->bufSize))
        return FALSE;

    return TRUE;
}


/* Delete INTERNAL information
 */
void xs_sidplay1_delete(xs_status_t * myStatus)
{
    xs_sidplay1_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay1_t *) myStatus->sidEngine;
    if (!myEngine) return;
    
    g_free(myEngine->buf);
    myEngine->buf = NULL;
    myEngine->bufSize = 0;
}


}    /* extern "C" */
#endif    /* HAVE_SIDPLAY1 */
