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

#include <stdio.h>
#include <string.h>

#include "xmms-sid.h"
#include "xs_slsup.h"

/*
 * Configuration specific stuff
 */
pthread_mutex_t xs_cfg_mutex = PTHREAD_MUTEX_INITIALIZER;
struct xs_cfg_t xs_cfg;

/* Reset/initialize the configuration
 */
void xs_init_configuration(void)
{
    /* Lock configuration mutex */
    pthread_mutex_lock(&xs_cfg_mutex);

    memset(&xs_cfg, 0, sizeof(xs_cfg));

    /* Initialize values with sensible defaults */
    xs_cfg.audioChannels = XS_CHN_STEREO;
    xs_cfg.audioFrequency = XS_AUDIO_FREQ;

    xs_cfg.mos8580 = false;
    xs_cfg.forceModel = false;

    /* Filter values */
    xs_cfg.emulateFilters = true;

    xs_cfg.clockSpeed = XS_CLOCK_PAL;
    xs_cfg.forceSpeed = false;

    xs_cfg.playMaxTimeEnable = false;
    xs_cfg.playMaxTimeUnknown = false;
    xs_cfg.playMaxTime = 150;

    xs_cfg.playMinTimeEnable = false;
    xs_cfg.playMinTime = 15;

    xs_cfg.subAutoEnable = true;
    xs_cfg.subAutoMinOnly = true;
    xs_cfg.subAutoMinTime = 15;

    /* Unlock the configuration */
    pthread_mutex_unlock(&xs_cfg_mutex);
}
