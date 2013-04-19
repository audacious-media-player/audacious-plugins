/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Configuration dialog

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

#include "xs_config.h"

/*
 * Configuration specific stuff
 */
XS_MUTEX(xs_cfg);
struct xs_cfg_t xs_cfg;

/* Reset/initialize the configuration
 */
void xs_init_configuration(void)
{
    /* Lock configuration mutex */
    XSDEBUG("initializing configuration ...\n");
    XS_MUTEX_LOCK(xs_cfg);

    memset(&xs_cfg, 0, sizeof(xs_cfg));

    /* Initialize values with sensible defaults */
    xs_cfg.audioBitsPerSample = XS_RES_16BIT;
    xs_cfg.audioChannels = XS_CHN_MONO;
    xs_cfg.audioFrequency = XS_AUDIO_FREQ;

    xs_cfg.mos8580 = FALSE;
    xs_cfg.forceModel = FALSE;

    /* Filter values */
    xs_cfg.emulateFilters = TRUE;

    xs_cfg.playerEngine = XS_ENG_SIDPLAY2;
    xs_cfg.memoryMode = XS_MPU_REAL;

    xs_cfg.clockSpeed = XS_CLOCK_PAL;
    xs_cfg.forceSpeed = FALSE;

    xs_cfg.sid2OptLevel = 0;
    xs_cfg.sid2NFilterPresets = 0;

    xs_cfg.sid2Builder = XS_BLD_RESID;

    xs_cfg.oversampleEnable = FALSE;
    xs_cfg.oversampleFactor = XS_MIN_OVERSAMPLE;

    xs_cfg.playMaxTimeEnable = FALSE;
    xs_cfg.playMaxTimeUnknown = FALSE;
    xs_cfg.playMaxTime = 150;

    xs_cfg.playMinTimeEnable = FALSE;
    xs_cfg.playMinTime = 15;

    xs_cfg.songlenDBEnable = FALSE;
    xs_pstrcpy(&xs_cfg.songlenDBPath, "~/C64Music/DOCUMENTS/Songlengths.txt");

    xs_cfg.stilDBEnable = FALSE;
    xs_pstrcpy(&xs_cfg.stilDBPath, "~/C64Music/DOCUMENTS/STIL.txt");
    xs_pstrcpy(&xs_cfg.hvscPath, "~/C64Music");

    xs_cfg.subsongControl = XS_SSC_POPUP;
    xs_cfg.detectMagic = FALSE;

    xs_cfg.titleOverride = TRUE;

    xs_pstrcpy(&xs_cfg.titleFormat, "${artist} - ${title} (${copyright}) <${subsong-id}/${subsong-num}> [${sid-model}/${sid-speed}]");

    xs_cfg.subAutoEnable = TRUE;
    xs_cfg.subAutoMinOnly = TRUE;
    xs_cfg.subAutoMinTime = 15;


    /* Unlock the configuration */
    XS_MUTEX_UNLOCK(xs_cfg);
}
