/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   File information window

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

#include "xs_slsup.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "xs_config.h"


/* Allocate a new tune information structure
 */
xs_tuneinfo_t *xs_tuneinfo_new(const char * filename,
        int nsubTunes, int startTune, const char * sidName,
        const char * sidComposer, const char * sidCopyright,
        int loadAddr, int initAddr, int playAddr,
        int dataFileLen, const char *sidFormat, int sidModel)
{
    xs_tuneinfo_t *result;
    int i;

    /* Allocate structure */
    result = new xs_tuneinfo_t ();

    result->sidFilename = String (filename);

    /* Allocate space for subtune information */
    result->subTunes = g_new0 (xs_subtuneinfo_t, nsubTunes + 1);

    result->sidName = String (sidName);
    result->sidComposer = String (sidComposer);
    result->sidCopyright = String (sidCopyright);

    result->nsubTunes = nsubTunes;
    result->startTune = startTune;

    result->loadAddr = loadAddr;
    result->initAddr = initAddr;
    result->playAddr = playAddr;
    result->dataFileLen = dataFileLen;
    result->sidFormat = String (sidFormat);

    result->sidModel = sidModel;

    /* Fill in sub-tune information */
    for (i = 0; i < result->nsubTunes; i++) {
        result->subTunes[i].tuneLength = -1;
        result->subTunes[i].tuneSpeed = -1;
    }

    return result;
}


/* Free given tune information structure
 */
void xs_tuneinfo_free(xs_tuneinfo_t * tune)
{
    if (!tune) return;

    g_free(tune->subTunes);

    delete tune;
}
