/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Main header file

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
#ifndef XMMS_SID_H
#define XMMS_SID_H

#include <libaudcore/index.h>
#include <libaudcore/objects.h>

/*
 * Some constants and defines
 */

/* Default audio rendering frequency in Hz
 */
#define XS_AUDIO_FREQ           (44100)

/* Plugin-wide typedefs
 */
typedef struct {
    int tuneLength = -1;
} xs_subtuneinfo_t;

typedef struct {
    String sidName, sidComposer, sidCopyright, sidFormat;
    int nsubTunes, startTune;
    Index<xs_subtuneinfo_t> subTunes;
} xs_tuneinfo_t;

#endif /* XMMS_SID_H */
