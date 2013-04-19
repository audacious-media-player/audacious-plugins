/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   libSIDPlay v2 support

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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "xmms-sid.h"

#include <stdio.h>
#include "xs_sidplay2.h"
#include "xs_config.h"


#include <sidplay/sidplay2.h>


class xs_sidplay2_t {
public:
    sidplay2 *currEng;
    sidbuilder *currBuilder;
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
:currEng(NULL)
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

    if (f == NULL) return FALSE;

    if (xs_fread(tmpBuf, sizeof(gchar), 4, f) != 4)
        return FALSE;

    if (!strncmp(tmpBuf, "PSID", 4) || !strncmp(tmpBuf, "RSID", 4))
        return TRUE;
    else
        return FALSE;
}


/* Initialize SIDPlay2
 */
gboolean xs_sidplay2_init(xs_status_t * status)
{
    gint tmpFreq, i;
    xs_sidplay2_t *engine;
    sid_filter_t tmpFilter;
    xs_sid_filter_t *f;
    assert(status != NULL);

    /* Allocate internal structures */
    engine = new xs_sidplay2_t();
    status->sidEngine = engine;
    if (!engine) return FALSE;

    /* Initialize the engine */
    engine->currEng = new sidplay2;
    if (!engine->currEng) {
        xs_error("[SIDPlay2] Could not initialize emulation engine.\n");
        return FALSE;
    }

    /* Get current configuration */
    engine->currConfig = engine->currEng->config();

    /* Configure channels and stuff */
    switch (status->audioChannels) {

    case XS_CHN_AUTOPAN:
        engine->currConfig.playback = sid2_stereo;
        break;

    case XS_CHN_STEREO:
        engine->currConfig.playback = sid2_stereo;
        break;

    case XS_CHN_MONO:
    default:
        engine->currConfig.playback = sid2_mono;
        status->audioChannels = XS_CHN_MONO;
        break;
    }


    /* Memory mode settings */
    switch (xs_cfg.memoryMode) {
    case XS_MPU_BANK_SWITCHING:
        engine->currConfig.environment = sid2_envBS;
        break;

    case XS_MPU_TRANSPARENT_ROM:
        engine->currConfig.environment = sid2_envTP;
        break;

    case XS_MPU_PLAYSID_ENVIRONMENT:
        engine->currConfig.environment = sid2_envPS;
        break;

    case XS_MPU_REAL:
    default:
        engine->currConfig.environment = sid2_envR;
        xs_cfg.memoryMode = XS_MPU_REAL;
        break;
    }


    /* Audio parameters sanity checking and setup */
    engine->currConfig.precision = status->audioBitsPerSample;
    tmpFreq = status->audioFrequency;

    if (status->oversampleEnable)
        tmpFreq = (tmpFreq * status->oversampleFactor);

    engine->currConfig.frequency = tmpFreq;

    switch (status->audioBitsPerSample) {
    case XS_RES_8BIT:
        status->audioFormat = FMT_U8;
        engine->currConfig.sampleFormat = SID2_LITTLE_UNSIGNED;
        break;

    case XS_RES_16BIT:
    default:
        switch (status->audioFormat) {
        case FMT_U16_LE:
            engine->currConfig.sampleFormat = SID2_LITTLE_UNSIGNED;
            break;

        case FMT_U16_BE:
            engine->currConfig.sampleFormat = SID2_BIG_UNSIGNED;
            break;

        case FMT_S16_LE:
            engine->currConfig.sampleFormat = SID2_LITTLE_SIGNED;
            break;

        case FMT_S16_BE:
            engine->currConfig.sampleFormat = SID2_BIG_SIGNED;
            break;

        default:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
            status->audioFormat = FMT_S16_LE;
            engine->currConfig.sampleFormat = SID2_LITTLE_SIGNED;
#else
            status->audioFormat = FMT_S16_BE;
            engine->currConfig.sampleFormat = SID2_BIG_SIGNED;
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
    XSDEBUG("init builder #%i, maxsids=%i\n", xs_cfg.sid2Builder, (engine->currEng->info()).maxsids);
#ifdef HAVE_RESID_BUILDER
    if (xs_cfg.sid2Builder == XS_BLD_RESID) {
        ReSIDBuilder *rs = new ReSIDBuilder("ReSID builder");
        engine->currBuilder = (sidbuilder *) rs;
        if (rs) {
            /* Builder object created, initialize it */
            rs->create((engine->currEng->info()).maxsids);
            if (!*rs) {
                xs_error("reSID->create() failed.\n");
                return FALSE;
            }

            rs->filter(xs_cfg.emulateFilters);
            if (!*rs) {
                xs_error("reSID->filter(%d) failed.\n", xs_cfg.emulateFilters);
                return FALSE;
            }

            // FIXME FIX ME: support other configurable parameters ...
            // ... WHEN/IF resid-builder+libsidplay2 gets fixed
            rs->sampling(tmpFreq);
            if (!*rs) {
                xs_error("reSID->sampling(%d) failed.\n", tmpFreq);
                return FALSE;
            }
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
        HardSIDBuilder *hs = new HardSIDBuilder("HardSID builder");
        engine->currBuilder = (sidbuilder *) hs;
        if (hs) {
            /* Builder object created, initialize it */
            hs->create((engine->currEng->info()).maxsids);
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

    if (!engine->currBuilder) {
        xs_error("[SIDPlay2] Could not initialize SIDBuilder object.\n");
        return FALSE;
    }

    engine->currConfig.sidEmulation = engine->currBuilder;
    XSDEBUG("%s\n", engine->currBuilder->credits());

    /* Clockspeed settings */
    switch (xs_cfg.clockSpeed) {
    case XS_CLOCK_NTSC:
        engine->currConfig.clockDefault = SID2_CLOCK_NTSC;
        break;

    default:
        xs_error("[SIDPlay2] Invalid clockSpeed=%d, falling back to PAL.\n",
            xs_cfg.clockSpeed);

    case XS_CLOCK_PAL:
        engine->currConfig.clockDefault = SID2_CLOCK_PAL;
        xs_cfg.clockSpeed = XS_CLOCK_PAL;
        break;
    }


    /* Configure rest of the emulation */
    if (xs_cfg.forceSpeed) {
        engine->currConfig.clockForced = true;
        engine->currConfig.clockSpeed = engine->currConfig.clockDefault;
    } else {
        engine->currConfig.clockForced = false;
        engine->currConfig.clockSpeed = SID2_CLOCK_CORRECT;
    }


    if (xs_cfg.sid2OptLevel >= 0 && xs_cfg.sid2OptLevel <= SID2_MAX_OPTIMISATION) {
        engine->currConfig.optimisation = xs_cfg.sid2OptLevel;
    } else {
        xs_error("Invalid sid2OptLevel=%d, falling back to %d.\n",
            xs_cfg.sid2OptLevel, SID2_DEFAULT_OPTIMISATION);

        xs_cfg.sid2OptLevel =
        engine->currConfig.optimisation = SID2_DEFAULT_OPTIMISATION;
    }

    if (xs_cfg.mos8580)
        engine->currConfig.sidDefault = SID2_MOS8580;
    else
        engine->currConfig.sidDefault = SID2_MOS6581;

    if (xs_cfg.forceModel)
        engine->currConfig.sidModel = engine->currConfig.sidDefault;
    else
        engine->currConfig.sidModel = SID2_MODEL_CORRECT;


    /* XXX: Should this be configurable? libSIDPlay1 does not support it, though */
    engine->currConfig.sidSamples = TRUE;


    /* Now set the emulator configuration */
    if (engine->currEng->config(engine->currConfig) < 0) {
        xs_error("[SIDPlay2] Emulator engine configuration failed!\n");
        return FALSE;
    }

    /* Create the sidtune */
    engine->currTune = new SidTune(0);
    if (!engine->currTune) {
        xs_error("[SIDPlay2] Could not initialize SIDTune object.\n");
        return FALSE;
    }

    return TRUE;
}


/* Close SIDPlay2 engine
 */
void xs_sidplay2_close(xs_status_t * status)
{
    xs_sidplay2_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay2_t *) status->sidEngine;

    /* Free internals */
    if (engine->currBuilder) {
        delete engine->currBuilder;
        engine->currBuilder = NULL;
    }

    if (engine->currEng) {
        delete engine->currEng;
        engine->currEng = NULL;
    }

    if (engine->currTune) {
        delete engine->currTune;
        engine->currTune = NULL;
    }

    xs_sidplay2_delete(status);

    delete engine;
    status->sidEngine = NULL;
}


/* Initialize current song and sub-tune
 */
gboolean xs_sidplay2_initsong(xs_status_t * status)
{
    xs_sidplay2_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay2_t *) status->sidEngine;
    if (engine == NULL) return FALSE;

    if (!engine->currTune->selectSong(status->currSong)) {
        xs_error("[SIDPlay2] currTune->selectSong() failed\n");
        return FALSE;
    }

    if (engine->currEng->load(engine->currTune) < 0) {
        xs_error("[SIDPlay2] currEng->load() failed\n");
        return FALSE;
    }

    status->isInitialized = TRUE;

    return TRUE;
}


/* Emulate and render audio data to given buffer
 */
guint xs_sidplay2_fillbuffer(xs_status_t * status, gchar * audioBuffer, guint audioBufSize)
{
    xs_sidplay2_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay2_t *) status->sidEngine;
    if (!engine) return 0;

    return engine->currEng->play(audioBuffer, audioBufSize);
}


/* Load a given SID-tune file
 */
gboolean xs_sidplay2_load(xs_status_t * status, const gchar * pcFilename)
{
    xs_sidplay2_t *engine;
    assert(status != NULL);
    status->isInitialized = FALSE;

    engine = (xs_sidplay2_t *) status->sidEngine;
    if (!engine) return FALSE;

    /* Try to get the tune */
    if (!pcFilename) return FALSE;

    if (xs_fload_buffer(pcFilename, &(engine->buf), &(engine->bufSize)) != 0)
        return FALSE;

    if (!engine->currTune->read(engine->buf, engine->bufSize))
        return FALSE;

    return TRUE;
}


/* Delete INTERNAL information
 */
void xs_sidplay2_delete(xs_status_t * status)
{
    xs_sidplay2_t *engine;
    assert(status != NULL);

    engine = (xs_sidplay2_t *) status->sidEngine;
    if (engine == NULL) return;

    g_free(engine->buf);
    engine->buf = NULL;
    engine->bufSize = 0;
}


/* Hardware backend flushing
 */
void xs_sidplay2_flush(xs_status_t * status)
{
    assert(status != NULL);

#ifdef HAVE_HARDSID_BUILDER
#ifdef HSID_SID2_COM
    IfPtr<HardSIDBuilder> hs(status->currBuilder);
    if (hs)
        hs->flush();
#else
    if (xs_cfg.sid2Builder == XS_BLD_HARDSID)
        ((HardSIDBuilder *) status->currBuilder)->flush();
#endif
#endif
}


}    /* extern "C" */
