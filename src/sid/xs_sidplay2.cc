/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   libSIDPlay v2 support

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

#ifdef HAVE_SIDPLAY2

#include <stdio.h>
#include "xs_sidplay2.h"
#include "xs_config.h"


#include <sidplay/sidplay2.h>
#ifdef HAVE_SIDPLAY2_COMI
#  include <sidplay/sidlazyiptr.h>
#endif


class xs_sidplay2_t {
public:
#ifdef HAVE_SIDPLAY2_COMI
    SidIPtr<ISidplay2> currEng;
    SidLazyIPtr<ISidUnknown> currBuilder;
#else
    sidplay2 *currEng;
    sidbuilder *currBuilder;
#endif
    sid2_config_t currConfig;
    SidTune *currTune;
    guint8 *buf;
    size_t bufSize;
    
    xs_sidplay2_t(void);
    virtual ~xs_sidplay2_t(void) { ; }
};


#ifdef HAVE_RESID_BUILDER
#  include <sidplay/builders/resid.h>
#endif
#ifdef HAVE_HARDSID_BUILDER
#  include <sidplay/builders/hardsid.h>
#endif


xs_sidplay2_t::xs_sidplay2_t(void)
#ifdef HAVE_SIDPLAY2_COMI
:currEng(sidplay2::create())
#else
:currEng(NULL)
#endif
{
    buf = NULL;
    bufSize = 0;
    currTune = NULL;
    currBuilder = NULL;    
}


/* We need to 'export' all this pseudo-C++ crap */
extern "C" {


/* Return song information
 */
#define TFUNCTION   xs_sidplay2_getinfo
#define TFUNCTION2  xs_sidplay2_updateinfo
#define TTUNEINFO   SidTuneInfo
#define TTUNE       SidTune
#define TENGINE     xs_sidplay2_t
#include "xs_sidplay.h"


/* Check if we can play the given file
 */
gboolean xs_sidplay2_probe(xs_file_t *f)
{
    gchar tmpBuf[5];
    
    if (!f) return FALSE;
    
    if (xs_fread(tmpBuf, sizeof(gchar), 4, f) != 4)
        return FALSE;
    
    if (!strncmp(tmpBuf, "PSID", 4) || !strncmp(tmpBuf, "RSID", 4))
        return TRUE;
    else
        return FALSE;
}


/* Initialize SIDPlay2
 */
gboolean xs_sidplay2_init(xs_status_t * myStatus)
{
    gint tmpFreq, i;
    xs_sidplay2_t *myEngine;
    sid_filter_t tmpFilter;
    xs_sid2_filter_t *f;
    assert(myStatus);

    /* Allocate internal structures */
    myEngine = new xs_sidplay2_t();
    myStatus->sidEngine = myEngine;
    if (!myEngine) return FALSE;

    /* Initialize the engine */
#ifndef HAVE_SIDPLAY2_COMI
    myEngine->currEng = new sidplay2;
#endif
    if (!myEngine->currEng) {
        xs_error("[SIDPlay2] Could not initialize emulation engine.\n");
        return FALSE;
    }

    /* Get current configuration */
    myEngine->currConfig = myEngine->currEng->config();

    /* Configure channels and stuff */
    switch (myStatus->audioChannels) {

    case XS_CHN_AUTOPAN:
        myEngine->currConfig.playback = sid2_stereo;
        break;

    case XS_CHN_STEREO:
        myEngine->currConfig.playback = sid2_stereo;
        break;

    case XS_CHN_MONO:
    default:
        myEngine->currConfig.playback = sid2_mono;
        myStatus->audioChannels = XS_CHN_MONO;
        break;
    }


    /* Memory mode settings */
    switch (xs_cfg.memoryMode) {
    case XS_MPU_BANK_SWITCHING:
        myEngine->currConfig.environment = sid2_envBS;
        break;

    case XS_MPU_TRANSPARENT_ROM:
        myEngine->currConfig.environment = sid2_envTP;
        break;

    case XS_MPU_PLAYSID_ENVIRONMENT:
        myEngine->currConfig.environment = sid2_envPS;
        break;

    case XS_MPU_REAL:
    default:
        myEngine->currConfig.environment = sid2_envR;
        xs_cfg.memoryMode = XS_MPU_REAL;
        break;
    }


    /* Audio parameters sanity checking and setup */
    myEngine->currConfig.precision = myStatus->audioBitsPerSample;
    tmpFreq = myStatus->audioFrequency;

    if (myStatus->oversampleEnable)
        tmpFreq = (tmpFreq * myStatus->oversampleFactor);

    myEngine->currConfig.frequency = tmpFreq;

    switch (myStatus->audioBitsPerSample) {
    case XS_RES_8BIT:
        myStatus->audioFormat = FMT_U8;
        myEngine->currConfig.sampleFormat = SID2_LITTLE_UNSIGNED;
        break;

    case XS_RES_16BIT:
    default:
        switch (myStatus->audioFormat) {
        case FMT_U16_LE:
            myEngine->currConfig.sampleFormat = SID2_LITTLE_UNSIGNED;
            break;

        case FMT_U16_BE:
            myEngine->currConfig.sampleFormat = SID2_BIG_UNSIGNED;
            break;

        case FMT_U16_NE:
#if G_BYTE_ORDER == G_BIG_ENDIAN
            myEngine->currConfig.sampleFormat = SID2_BIG_UNSIGNED;
#else
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
            myEngine->currConfig.sampleFormat = SID2_LITTLE_UNSIGNED;
#else
#error Unsupported endianess!
#endif
#endif
            break;

        case FMT_S16_LE:
            myEngine->currConfig.sampleFormat = SID2_LITTLE_SIGNED;
            break;

        case FMT_S16_BE:
            myEngine->currConfig.sampleFormat = SID2_BIG_SIGNED;
            break;

        default:
            myStatus->audioFormat = FMT_S16_NE;
#if G_BYTE_ORDER == G_BIG_ENDIAN
            myEngine->currConfig.sampleFormat = SID2_BIG_SIGNED;
#else
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
            myEngine->currConfig.sampleFormat = SID2_LITTLE_SIGNED;
#else
#error Unsupported endianess!
#endif
#endif
            break;

        }
        break;
    }
    
    /* Convert filter */
    f = &(xs_cfg.sid2Filter);
    XSDEBUG("using filter '%s', %d points\n", f->name, f->npoints);
    if (f->npoints > XS_SIDPLAY2_NFPOINTS) {
        xs_error("[SIDPlay2] Invalid number of filter curve points (%d > %d)\n",
            f->npoints, XS_SIDPLAY2_NFPOINTS);
        f->npoints = XS_SIDPLAY2_NFPOINTS;
    }
    
    tmpFilter.points = f->npoints;
    for (i = 0; i < f->npoints; i++) {
        tmpFilter.cutoff[i][0] = f->points[i].x;
        tmpFilter.cutoff[i][1] = f->points[i].y;
    }

    /* Initialize builder object */
    XSDEBUG("init builder #%i, maxsids=%i\n", xs_cfg.sid2Builder, (myEngine->currEng->info()).maxsids);
#ifdef HAVE_RESID_BUILDER
    if (xs_cfg.sid2Builder == XS_BLD_RESID) {
#ifdef HAVE_SIDPLAY2_COMI
        myEngine->currBuilder = ReSIDBuilderCreate("");
        SidLazyIPtr<IReSIDBuilder> rs(myEngine->currBuilder);
        if (rs) {
            myEngine->currConfig.sidEmulation = rs->iaggregate();
#else
        ReSIDBuilder *rs = new ReSIDBuilder("ReSID builder");
        myEngine->currBuilder = (sidbuilder *) rs;
        if (rs) {
#endif
            /* Builder object created, initialize it */
            rs->create((myEngine->currEng->info()).maxsids);
            if (!*rs) {
                xs_error("reSID->create() failed.\n");
                return FALSE;
            }

            rs->filter(xs_cfg.emulateFilters);
            if (!*rs) {
                xs_error("reSID->filter(%d) failed.\n", xs_cfg.emulateFilters);
                return FALSE;
            }

#ifndef HAVE_SIDPLAY2_DISTORTION
            // FIXME FIX ME: support other configurable parameters ...
            // ... WHEN/IF resid-builder+libsidplay2 gets fixed
            rs->sampling(tmpFreq);
            if (!*rs) {
                xs_error("reSID->sampling(%d) failed.\n", tmpFreq);
                return FALSE;
            }
#endif
            
            if (tmpFilter.points > 0)
                rs->filter((sid_filter_t *) &tmpFilter);
            else
                rs->filter((sid_filter_t *) NULL);
            
            if (!*rs) {
                xs_error("reSID->filter(NULL) failed.\n");
                return FALSE;
            }
        }
    }
#endif
#ifdef HAVE_HARDSID_BUILDER
    if (xs_cfg.sid2Builder == XS_BLD_HARDSID) {
#ifdef HAVE_SIDPLAY2_COMI
        myEngine->currBuilder = HardSIDBuilderCreate("");
        SidLazyIPtr<IHardSIDBuilder> hs(myEngine->currBuilder);
        if (hs) {
            myEngine->currConfig.sidEmulation = hs->iaggregate();
#else
        HardSIDBuilder *hs = new HardSIDBuilder("HardSID builder");
        myEngine->currBuilder = (sidbuilder *) hs;
        if (hs) {
#endif
            /* Builder object created, initialize it */
            hs->create((myEngine->currEng->info()).maxsids);
            if (!*hs) {
                xs_error("hardSID->create() failed.\n");
                return FALSE;
            }

            hs->filter(xs_cfg.emulateFilters);
            if (!*hs) {
                xs_error("hardSID->filter(%d) failed.\n", xs_cfg.emulateFilters);
                return FALSE;
            }
        }
    }
#endif

    if (!myEngine->currBuilder) {
        xs_error("[SIDPlay2] Could not initialize SIDBuilder object.\n");
        return FALSE;
    }

#ifndef HAVE_SIDPLAY2_COMI
    myEngine->currConfig.sidEmulation = myEngine->currBuilder;
    XSDEBUG("%s\n", myEngine->currBuilder->credits());
#endif

    /* Clockspeed settings */
    switch (xs_cfg.clockSpeed) {
    case XS_CLOCK_NTSC:
        myEngine->currConfig.clockDefault = SID2_CLOCK_NTSC;
        break;

    default:
        xs_error("[SIDPlay2] Invalid clockSpeed=%d, falling back to PAL.\n",
            xs_cfg.clockSpeed);

    case XS_CLOCK_PAL:
        myEngine->currConfig.clockDefault = SID2_CLOCK_PAL;
        xs_cfg.clockSpeed = XS_CLOCK_PAL;
        break;
    }


    /* Configure rest of the emulation */
    
    if (xs_cfg.forceSpeed) { 
        myEngine->currConfig.clockForced = true;
        myEngine->currConfig.clockSpeed = myEngine->currConfig.clockDefault;
    } else {
        myEngine->currConfig.clockForced = false;
        myEngine->currConfig.clockSpeed = SID2_CLOCK_CORRECT;
    }
    
    if ((xs_cfg.sid2OptLevel >= 0) && (xs_cfg.sid2OptLevel <= SID2_MAX_OPTIMISATION))
        myEngine->currConfig.optimisation = xs_cfg.sid2OptLevel;
    else {
        xs_error("Invalid sid2OptLevel=%d, falling back to %d.\n",
            xs_cfg.sid2OptLevel, SID2_DEFAULT_OPTIMISATION);
        
        xs_cfg.sid2OptLevel =
        myEngine->currConfig.optimisation = SID2_DEFAULT_OPTIMISATION;
    }

    if (xs_cfg.mos8580)
        myEngine->currConfig.sidDefault = SID2_MOS8580;
    else
        myEngine->currConfig.sidDefault = SID2_MOS6581;

    if (xs_cfg.forceModel)
        myEngine->currConfig.sidModel = myEngine->currConfig.sidDefault;
    else
        myEngine->currConfig.sidModel = SID2_MODEL_CORRECT;

    
    /* XXX: Should this be configurable? libSIDPlay1 does not support it, though */
    myEngine->currConfig.sidSamples = TRUE;


    /* Now set the emulator configuration */
    if (myEngine->currEng->config(myEngine->currConfig) < 0) {
        xs_error("[SIDPlay2] Emulator engine configuration failed!\n");
        return FALSE;
    }

    /* Create the sidtune */
    myEngine->currTune = new SidTune(0);
    if (!myEngine->currTune) {
        xs_error("[SIDPlay2] Could not initialize SIDTune object.\n");
        return FALSE;
    }

    return TRUE;
}


/* Close SIDPlay2 engine
 */
void xs_sidplay2_close(xs_status_t * myStatus)
{
    xs_sidplay2_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay2_t *) myStatus->sidEngine;

    /* Free internals */
    if (myEngine->currBuilder) {
#ifndef HAVE_SIDPLAY2_COMI
        delete myEngine->currBuilder;
#endif
        myEngine->currBuilder = NULL;
    }

#ifndef HAVE_SIDPLAY2_COMI
    if (myEngine->currEng) {
        delete myEngine->currEng;
        myEngine->currEng = NULL;
    }
#endif

    if (myEngine->currTune) {
        delete myEngine->currTune;
        myEngine->currTune = NULL;
    }

    xs_sidplay2_delete(myStatus);

    delete myEngine;
    myStatus->sidEngine = NULL;
}


/* Initialize current song and sub-tune
 */
gboolean xs_sidplay2_initsong(xs_status_t * myStatus)
{
    xs_sidplay2_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay2_t *) myStatus->sidEngine;
    if (!myEngine) return FALSE;

    if (!myEngine->currTune->selectSong(myStatus->currSong)) {
        xs_error("[SIDPlay2] currTune->selectSong() failed\n");
        return FALSE;
    }

    if (myEngine->currEng->load(myEngine->currTune) < 0) {
        xs_error("[SIDPlay2] currEng->load() failed\n");
        return FALSE;
    }
    
    myStatus->isInitialized = TRUE;

    return TRUE;
}


/* Emulate and render audio data to given buffer
 */
guint xs_sidplay2_fillbuffer(xs_status_t * myStatus, gchar * audioBuffer, guint audioBufSize)
{
    xs_sidplay2_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay2_t *) myStatus->sidEngine;
    if (!myEngine) return 0;

    return myEngine->currEng->play(audioBuffer, audioBufSize);
}


/* Load a given SID-tune file
 */
gboolean xs_sidplay2_load(xs_status_t * myStatus, gchar * pcFilename)
{
    xs_sidplay2_t *myEngine;
    assert(myStatus);
    myStatus->isInitialized = FALSE;

    myEngine = (xs_sidplay2_t *) myStatus->sidEngine;
    if (!myEngine) return FALSE;

    /* Try to get the tune */
    if (!pcFilename) return FALSE;

    if (xs_fload_buffer(pcFilename, &(myEngine->buf), &(myEngine->bufSize)) != 0)
        return FALSE;
    
    if (!myEngine->currTune->read(myEngine->buf, myEngine->bufSize))
        return FALSE;

    return TRUE;
}


/* Delete INTERNAL information
 */
void xs_sidplay2_delete(xs_status_t * myStatus)
{
    xs_sidplay2_t *myEngine;
    assert(myStatus);

    myEngine = (xs_sidplay2_t *) myStatus->sidEngine;
    if (!myEngine) return;
    
    g_free(myEngine->buf);
    myEngine->buf = NULL;
    myEngine->bufSize = 0;
}


/* Hardware backend flushing
 */
void xs_sidplay2_flush(xs_status_t * myStatus)
{
    assert(myStatus);

#ifdef HAVE_HARDSID_BUILDER
#ifdef HSID_SID2_COM
    IfPtr<HardSIDBuilder> hs(myStatus->currBuilder);
    if (hs)
        hs->flush();
#else
    if (xs_cfg.sid2Builder == XS_BLD_HARDSID)
        ((HardSIDBuilder *) myStatus->currBuilder)->flush();
#endif
#endif
}


}    /* extern "C" */
#endif    /* HAVE_SIDPLAY2 */
