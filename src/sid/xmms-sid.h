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

#include <libaudcore/objects.h>

/*
 * Some constants and defines
 */

/* Size for some small buffers (always static variables) */
#define XS_BUF_SIZE             (2048)

/* Default audio rendering frequency in Hz
 */
#define XS_AUDIO_FREQ           (44100)

/* Size of data buffer used for SID-tune MD5 hash calculation.
 * If this is too small, the computed hash will be incorrect.
 * Largest SID files I've seen are ~70kB. */
#define XS_SIDBUF_SIZE          (128*1024)

/* Plugin-wide typedefs
 */
typedef struct {
    int tuneSpeed, tuneLength;
    bool tunePlayed;
} xs_subtuneinfo_t;

typedef struct {
    String sidFilename, sidName, sidComposer, sidCopyright, sidFormat;
    int loadAddr, initAddr, playAddr, dataFileLen, sidModel;
    int nsubTunes, startTune;
    xs_subtuneinfo_t *subTunes;
} xs_tuneinfo_t;

/* Plugin function prototypes
 */
#define xs_verror(fmt, args) vfprintf(stderr, fmt, args)
#define xs_error(...) fprintf(stderr, __VA_ARGS__)

#endif /* XMMS_SID_H */
