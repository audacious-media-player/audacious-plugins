/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   libSIDPlay skeleton functions used both by v1 and 2 of the backends

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 2005-2009 Tecnic Software productions (TNSP)

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


/* Updates the information of currently playing tune
 */
gboolean TFUNCTION2(xs_status_t *myStatus)
{
    TTUNEINFO myInfo;
    TTUNE *myTune;
    TENGINE *myEngine;
    xs_tuneinfo_t *i;

    /* Check if we have required structures initialized */
    if (!myStatus || !myStatus->tuneInfo || !myStatus->sidEngine)
        return FALSE;

    myEngine = (TENGINE *) myStatus->sidEngine;
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
        gint tmpSpeed = -1;

        switch (myInfo->clockSpeed()) {
        case SIDTUNE_CLOCK_PAL:
            tmpSpeed = XS_CLOCK_PAL;
            break;
        case SIDTUNE_CLOCK_NTSC:
            tmpSpeed = XS_CLOCK_NTSC;
            break;
        case SIDTUNE_CLOCK_ANY:
            tmpSpeed = XS_CLOCK_ANY;
            break;
        case SIDTUNE_CLOCK_UNKNOWN:
            switch (myInfo->songSpeed()) {
            case SIDTUNE_SPEED_VBI:
                tmpSpeed = XS_CLOCK_VBI;
                break;
            case SIDTUNE_SPEED_CIA_1A:
                tmpSpeed = XS_CLOCK_CIA;
                break;
            default:
                tmpSpeed = myInfo->songSpeed();
                break;
            }
        default:
            tmpSpeed = myInfo->clockSpeed();
            break;
        }

        i->subTunes[myStatus->currSong - 1].tuneSpeed = tmpSpeed;
    }

    return TRUE;
}

/* Undefine these */
#undef TFUNCTION
#undef TFUNCTION2
#undef TTUNEINFO
#undef TTUNE
