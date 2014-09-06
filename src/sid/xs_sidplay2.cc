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

#include "xs_config.h"
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

#include <libaudcore/vfs.h>

struct SidState {
    sidplayfp *currEng;
    sidbuilder *currBuilder;
    SidConfig currConfig;
    SidTune *currTune;
};

static SidState state;


/* Check if we can play the given file
 */
bool xs_sidplayfp_probe(const void *buf, int64_t bufSize)
{
    if (bufSize < 4)
        return false;

    return !memcmp(buf, "PSID", 4) || !memcmp(buf, "RSID", 4);
}


/* Initialize SIDPlayFP
 */
bool xs_sidplayfp_init()
{
    /* Initialize the engine */
    state.currEng = new sidplayfp;

    /* Get current configuration */
    state.currConfig = state.currEng->config();

    /* Configure channels and stuff */
    switch (xs_cfg.audioChannels)
    {
    case XS_CHN_STEREO:
        state.currConfig.playback = SidConfig::STEREO;
        break;

    case XS_CHN_MONO:
        state.currConfig.playback = SidConfig::MONO;
        break;
    }

    /* Audio parameters sanity checking and setup */
    state.currConfig.frequency = xs_cfg.audioFrequency;

    /* Initialize builder object */
    state.currBuilder = new ReSIDfpBuilder("ReSIDfp builder");

    /* Builder object created, initialize it */
    state.currBuilder->create(state.currEng->info().maxsids());
    if (!state.currBuilder->getStatus()) {
        xs_error("reSID->create() failed.\n");
        return false;
    }

    state.currBuilder->filter(xs_cfg.emulateFilters);
    if (!state.currBuilder->getStatus()) {
        xs_error("reSID->filter(%d) failed.\n", xs_cfg.emulateFilters);
        return false;
    }

    state.currConfig.sidEmulation = state.currBuilder;

    /* Clockspeed settings */
    switch (xs_cfg.clockSpeed) {
    case XS_CLOCK_NTSC:
        state.currConfig.defaultC64Model = SidConfig::NTSC;
        break;

    default:
        xs_error("[SIDPlayFP] Invalid clockSpeed=%d, falling back to PAL.\n",
            xs_cfg.clockSpeed);

    case XS_CLOCK_PAL:
        state.currConfig.defaultC64Model = SidConfig::PAL;
        xs_cfg.clockSpeed = XS_CLOCK_PAL;
        break;
    }

    state.currConfig.forceC64Model = xs_cfg.forceSpeed;

    /* Configure rest of the emulation */
    if (xs_cfg.mos8580)
        state.currConfig.defaultSidModel = SidConfig::MOS8580;
    else
        state.currConfig.defaultSidModel = SidConfig::MOS6581;

    state.currConfig.forceSidModel = xs_cfg.forceModel;

    /* Now set the emulator configuration */
    if (!state.currEng->config(state.currConfig)) {
        xs_error("[SIDPlayFP] Emulator engine configuration failed!\n");
        return false;
    }

    /* Create the sidtune */
    state.currTune = new SidTune(0);
    if (!state.currTune) {
        xs_error("[SIDPlayFP] Could not initialize SIDTune object.\n");
        return false;
    }

    return true;
}


/* Close SIDPlayFP engine
 */
void xs_sidplayfp_close()
{
    /* Free internals */
    if (state.currBuilder) {
        delete state.currBuilder;
        state.currBuilder = nullptr;
    }

    if (state.currEng) {
        delete state.currEng;
        state.currEng = nullptr;
    }

    if (state.currTune) {
        delete state.currTune;
        state.currTune = nullptr;
    }
}


/* Initialize current song and sub-tune
 */
bool xs_sidplayfp_initsong(int subtune)
{
    if (!state.currTune->selectSong(subtune)) {
        xs_error("[SIDPlayFP] currTune->selectSong() failed\n");
        return false;
    }

    if (!state.currEng->load(state.currTune)) {
        xs_error("[SIDPlayFP] currEng->load() failed\n");
        return false;
    }

    return true;
}


/* Emulate and render audio data to given buffer
 */
unsigned xs_sidplayfp_fillbuffer(char * audioBuffer, unsigned audioBufSize)
{
    return state.currEng->play((short *)audioBuffer, audioBufSize / 2) * 2;
}


/* Load a given SID-tune file
 */
bool xs_sidplayfp_load(const void *buf, int64_t bufSize)
{
    static int loaded_roms = 0;

    /* In xs_sidplayfp_init aud-vfs is not initialized yet, so try to load
       the optional rom files on the first xs_sidplayfp_load call. */
    if (!loaded_roms) {
        int64_t size = 0;
        void *kernal = nullptr, *basic = nullptr, *chargen = nullptr;

        vfs_file_get_contents("file://" SIDDATADIR "sidplayfp/kernal", &kernal, &size);
        if (size != 8192) {
            free(kernal);
            kernal = nullptr;
        }

        vfs_file_get_contents("file://" SIDDATADIR "sidplayfp/basic", &basic, &size);
        if(size != 8192) {
            free(basic);
            basic = nullptr;
        }

        vfs_file_get_contents("file://" SIDDATADIR "sidplayfp/chargen", &chargen, &size);
        if(size != 4096) {
            free(chargen);
            chargen = nullptr;
        }

        state.currEng->setRoms((uint8_t*)kernal, (uint8_t*)basic, (uint8_t*)chargen);
        free(kernal);
        free(basic);
        free(chargen);
        loaded_roms = 1;
    }

    /* Try to get the tune */
    state.currTune->read((const uint8_t*)buf, bufSize);

    return state.currTune->getStatus();
}


bool xs_sidplayfp_getinfo(xs_tuneinfo_t &ti, const char *filename, const void *buf, int64_t bufSize)
{
    static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
    static int got_db = -1;
    static SidDatabase database;

    /* Check if the tune exists and is readable */
    SidTune myTune((const uint8_t*)buf, bufSize);

    if (!myTune.getStatus())
        return false;

    /* Get general tune information */
    const SidTuneInfo *myInfo = myTune.getInfo();

    /* Allocate tuneinfo structure and set information */
    ti.sidFilename = String (filename);

    ti.sidName = String (myInfo->infoString(0));
    ti.sidComposer = String (myInfo->infoString(1));
    ti.sidCopyright = String (myInfo->infoString(2));

    ti.nsubTunes = myInfo->songs();
    ti.startTune = myInfo->startSong();

    ti.loadAddr = myInfo->loadAddr();
    ti.initAddr = myInfo->initAddr();
    ti.playAddr = myInfo->playAddr();
    ti.dataFileLen = myInfo->dataFileLen();
    ti.sidFormat = String (myInfo->formatString());

    ti.sidModel = myInfo->sidModel1();

    /* Allocate space for subtune information */
    ti.subTunes.insert(0, ti.nsubTunes);

    /* Fill in sub-tune information */
    for (int i = 0; i < ti.nsubTunes; i++)
    {
        ti.subTunes[i].tuneLength = -1;
        ti.subTunes[i].tuneSpeed = -1;
    }

    pthread_mutex_lock(&db_mutex);

    if (got_db == -1)
        got_db = database.open(SIDDATADIR "sidplayfp/Songlengths.txt");

    if (got_db)
    {
        for (int i = 0; i < ti.nsubTunes; i++)
        {
            myTune.selectSong(i + 1);
            ti.subTunes[i].tuneLength = database.length(myTune);
        }
    }

    pthread_mutex_unlock(&db_mutex);

    return true;
}

bool xs_sidplayfp_updateinfo(xs_tuneinfo_t &ti, int subtune)
{
    /* Check if we have required structures initialized */
    SidTune *myTune = state.currTune;
    if (!myTune)
        return false;

    /* Get currently playing tune information */
    const SidTuneInfo *myInfo = myTune->getInfo();

    /* NOTICE! Here we assume that libSIDPlay[12] headers define
     * SIDTUNE_SIDMODEL_* similarly to our enums in xs_config.h ...
     */
    ti.sidModel = myInfo->sidModel1();

    if ((subtune > 0) && (subtune <= ti.nsubTunes)) {
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

        ti.subTunes[subtune - 1].tuneSpeed = tmpSpeed;
    }

    return true;
}
