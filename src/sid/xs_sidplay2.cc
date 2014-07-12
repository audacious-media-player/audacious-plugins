/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   libSIDPlay v2 support

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2009 Tecnic Software productions (TNSP)

   Ported to sidplayfp:
   (C) Copyright 2013 Cristian Morales Vega and Hans de Goede

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

#include "xs_sidplay2.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidDatabase.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/builders/residfp.h>

class xs_sidplayfp_t {
public:
    sidplayfp *currEng;
    sidbuilder *currBuilder;
    SidConfig currConfig;
    SidTune *currTune;
    void *buf;
    int64_t bufSize;

    xs_sidplayfp_t(void);
    virtual ~xs_sidplayfp_t(void) { ; }
};


xs_sidplayfp_t::xs_sidplayfp_t(void)
:currEng(NULL)
{
    buf = NULL;
    bufSize = 0;
    currTune = NULL;
    currBuilder = NULL;
}


/* We need to 'export' all this pseudo-C++ crap */
extern "C" {

/* Check if we can play the given file
 */
bool_t xs_sidplayfp_probe(VFSFile *f)
{
    char tmpBuf[5];

    if (f == NULL) return FALSE;

    if (vfs_fread(tmpBuf, sizeof(char), 4, f) != 4)
        return FALSE;

    if (!strncmp(tmpBuf, "PSID", 4) || !strncmp(tmpBuf, "RSID", 4))
        return TRUE;
    else
        return FALSE;
}


/* Initialize SIDPlayFP
 */
bool_t xs_sidplayfp_init(xs_status_t * status)
{
    xs_sidplayfp_t *engine;
    assert(status != NULL);

    /* Allocate internal structures */
    engine = new xs_sidplayfp_t();
    status->sidEngine = engine;
    if (!engine) return FALSE;

    /* Initialize the engine */
    engine->currEng = new sidplayfp;
    if (!engine->currEng) {
        xs_error("[SIDPlayFP] Could not initialize emulation engine.\n");
        return FALSE;
    }

    /* Get current configuration */
    engine->currConfig = engine->currEng->config();

    /* Configure channels and stuff */
    switch (status->audioChannels)
    {
    case XS_CHN_STEREO:
        engine->currConfig.playback = SidConfig::STEREO;
        break;

    case XS_CHN_MONO:
        engine->currConfig.playback = SidConfig::MONO;
        break;
    }

    /* Audio parameters sanity checking and setup */
    engine->currConfig.frequency = status->audioFrequency;

    /* Initialize builder object */
    ReSIDfpBuilder *rs = new ReSIDfpBuilder("ReSIDfp builder");
    engine->currBuilder = (sidbuilder *) rs;

    /* Builder object created, initialize it */
    engine->currBuilder->create(engine->currEng->info().maxsids());
    if (!engine->currBuilder->getStatus()) {
        xs_error("reSID->create() failed.\n");
        return FALSE;
    }

    engine->currBuilder->filter(xs_cfg.emulateFilters);
    if (!engine->currBuilder->getStatus()) {
        xs_error("reSID->filter(%d) failed.\n", xs_cfg.emulateFilters);
        return FALSE;
    }

    engine->currConfig.sidEmulation = engine->currBuilder;

    /* Clockspeed settings */
    switch (xs_cfg.clockSpeed) {
    case XS_CLOCK_NTSC:
        engine->currConfig.defaultC64Model = SidConfig::NTSC;
        break;

    default:
        xs_error("[SIDPlayFP] Invalid clockSpeed=%d, falling back to PAL.\n",
            xs_cfg.clockSpeed);

    case XS_CLOCK_PAL:
        engine->currConfig.defaultC64Model = SidConfig::PAL;
        xs_cfg.clockSpeed = XS_CLOCK_PAL;
        break;
    }

    /* Configure rest of the emulation */
    if (xs_cfg.mos8580)
        engine->currConfig.defaultSidModel = SidConfig::MOS8580;
    else
        engine->currConfig.defaultSidModel = SidConfig::MOS6581;

    engine->currConfig.forceSidModel = xs_cfg.forceModel;

    /* Now set the emulator configuration */
    if (!engine->currEng->config(engine->currConfig)) {
        xs_error("[SIDPlayFP] Emulator engine configuration failed!\n");
        return FALSE;
    }

    /* Create the sidtune */
    engine->currTune = new SidTune(0);
    if (!engine->currTune) {
        xs_error("[SIDPlayFP] Could not initialize SIDTune object.\n");
        return FALSE;
    }

    return TRUE;
}


/* Close SIDPlayFP engine
 */
void xs_sidplayfp_close(xs_status_t * status)
{
    xs_sidplayfp_t *engine;
    assert(status != NULL);

    engine = (xs_sidplayfp_t *) status->sidEngine;

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

    xs_sidplayfp_delete(status);

    delete engine;
    status->sidEngine = NULL;
}


/* Initialize current song and sub-tune
 */
bool_t xs_sidplayfp_initsong(xs_status_t * status)
{
    xs_sidplayfp_t *engine;
    assert(status != NULL);

    engine = (xs_sidplayfp_t *) status->sidEngine;
    if (engine == NULL) return FALSE;

    if (!engine->currTune->selectSong(status->currSong)) {
        xs_error("[SIDPlayFP] currTune->selectSong() failed\n");
        return FALSE;
    }

    if (!engine->currEng->load(engine->currTune)) {
        xs_error("[SIDPlayFP] currEng->load() failed\n");
        return FALSE;
    }

    return TRUE;
}


/* Emulate and render audio data to given buffer
 */
unsigned xs_sidplayfp_fillbuffer(xs_status_t * status, char * audioBuffer, unsigned audioBufSize)
{
    xs_sidplayfp_t *engine;
    assert(status != NULL);

    engine = (xs_sidplayfp_t *) status->sidEngine;
    if (!engine) return 0;

    return engine->currEng->play((short *)audioBuffer, audioBufSize / 2) * 2;
}


/* Load a given SID-tune file
 */
bool_t xs_sidplayfp_load(xs_status_t * status, const char * pcFilename)
{
    /* This is safe, since xmms-sid.c always calls us with xs_status locked */
    static int loaded_roms = 0;

    xs_sidplayfp_t *engine;
    assert(status != NULL);

    engine = (xs_sidplayfp_t *) status->sidEngine;
    if (!engine) return FALSE;

    /* In xs_sidplayfp_init aud-vfs is not initialized yet, so try to load
       the optional rom files on the first xs_sidplayfp_load call. */
    if (!loaded_roms) {
        int64_t size = 0;
        void *kernal = NULL, *basic = NULL, *chargen = NULL;

        vfs_file_get_contents("file://" SIDDATADIR "sidplayfp/kernal", &kernal, &size);
        if (size != 8192) {
            free(kernal);
            kernal = NULL;
        }

        vfs_file_get_contents("file://" SIDDATADIR "sidplayfp/basic", &basic, &size);
        if(size != 8192) {
            free(basic);
            basic = NULL;
        }

        vfs_file_get_contents("file://" SIDDATADIR "sidplayfp/chargen", &chargen, &size);
        if(size != 4096) {
            free(chargen);
            chargen = NULL;
        }

        engine->currEng->setRoms((uint8_t*)kernal, (uint8_t*)basic, (uint8_t*)chargen);
        free(kernal);
        free(basic);
        free(chargen);
        loaded_roms = 1;
    }

    /* Try to get the tune */
    vfs_file_get_contents(pcFilename, &engine->buf, &engine->bufSize);
    if(!engine->bufSize) {
        free(engine->buf);
        engine->buf = NULL;
        return FALSE;
    }

    engine->currTune->read((uint8_t*)engine->buf, engine->bufSize);

    return engine->currTune->getStatus();
}


/* Delete INTERNAL information
 */
void xs_sidplayfp_delete(xs_status_t * status)
{
    xs_sidplayfp_t *engine;
    assert(status != NULL);

    engine = (xs_sidplayfp_t *) status->sidEngine;
    if (engine == NULL) return;

    free(engine->buf);
    engine->buf = NULL;
    engine->bufSize = 0;
}


xs_tuneinfo_t* xs_sidplayfp_getinfo(const char *sidFilename)
{
    /* This is safe, since xmms-sid.c always calls us with xs_status locked */
    static int got_db = -1;
    static SidDatabase database;

    xs_tuneinfo_t *result;
    const SidTuneInfo *myInfo;
    SidTune *myTune;
    void *buf = NULL;
    int64_t bufSize = 0;

    /* Load file */
    vfs_file_get_contents(sidFilename, &buf, &bufSize);

    /* Check if the tune exists and is readable */
    if (!bufSize || !(myTune = new SidTune((uint8_t*)buf, bufSize))) {
        free(buf);
        return NULL;
    }
    free(buf);

    if (!myTune->getStatus()) {
        delete myTune;
        return NULL;
    }

    /* Get general tune information */
    myInfo = myTune->getInfo();

    /* Allocate tuneinfo structure and set information */
    result = xs_tuneinfo_new(sidFilename,
        myInfo->songs(), myInfo->startSong(),
        myInfo->infoString(0), myInfo->infoString(1), myInfo->infoString(2),
        myInfo->loadAddr(), myInfo->initAddr(), myInfo->playAddr(),
        myInfo->dataFileLen(), myInfo->formatString(), myInfo->sidModel1());

    for (int i = 0; i < result->nsubTunes; i++) {
        if (result->subTunes[i].tuneLength >= 0)
            continue;

        if (got_db == -1)
            got_db = database.open(SIDDATADIR "sidplayfp/Songlengths.txt");

        if (got_db) {
            myTune->selectSong(i + 1);
            result->subTunes[i].tuneLength = database.length(*myTune);
        }
    }

    delete myTune;

    return result;
}

bool_t xs_sidplayfp_updateinfo(xs_status_t *myStatus)
{
    const SidTuneInfo *myInfo;
    SidTune *myTune;
    xs_sidplayfp_t *myEngine;
    xs_tuneinfo_t *i;

    /* Check if we have required structures initialized */
    if (!myStatus || !myStatus->tuneInfo || !myStatus->sidEngine)
        return FALSE;

    myEngine = (xs_sidplayfp_t *) myStatus->sidEngine;
    myTune = myEngine->currTune;
    if (!myTune)
        return FALSE;

    /* Get currently playing tune information */
    myInfo = myTune->getInfo();

    /* NOTICE! Here we assume that libSIDPlay[12] headers define
     * SIDTUNE_SIDMODEL_* similarly to our enums in xs_config.h ...
     */
    i = myStatus->tuneInfo;
    i->sidModel = myInfo->sidModel1();

    if ((myStatus->currSong > 0) && (myStatus->currSong <= i->nsubTunes)) {
        int tmpSpeed = -1;

        switch (myInfo->clockSpeed()) {
        case SidTuneInfo::CLOCK_PAL:
            tmpSpeed = XS_CLOCK_PAL;
            break;
        case SidTuneInfo::CLOCK_NTSC:
            tmpSpeed = XS_CLOCK_NTSC;
            break;
        case SidTuneInfo::CLOCK_ANY:
            tmpSpeed = XS_CLOCK_ANY;
            break;
        case SidTuneInfo::CLOCK_UNKNOWN:
            switch (myInfo->songSpeed()) {
            case SidTuneInfo::SPEED_VBI:
                tmpSpeed = XS_CLOCK_VBI;
                break;
            case SidTuneInfo::SPEED_CIA_1A:
                tmpSpeed = XS_CLOCK_CIA;
                break;
            default:
                tmpSpeed = myInfo->songSpeed();
                break;
            }
            break;
        default:
            tmpSpeed = myInfo->clockSpeed();
            break;
        }

        i->subTunes[myStatus->currSong - 1].tuneSpeed = tmpSpeed;
    }

    return TRUE;
}

}    /* extern "C" */
