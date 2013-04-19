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

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <pthread.h>
#include <audacious/plugin.h>

/*
 * Some constants and defines
 */

/* Size for some small buffers (always static variables) */
#define XS_BUF_SIZE             (1024)

/* Default audio rendering frequency in Hz
 */
#define XS_AUDIO_FREQ           (44100)

/* Size of data buffer used for SID-tune MD5 hash calculation.
 * If this is too small, the computed hash will be incorrect.
 * Largest SID files I've seen are ~70kB. */
#define XS_SIDBUF_SIZE          (128*1024)

/* Macros for mutexes and threads. These exist to be able to
 * easily change from pthreads to glib threads, etc, if necessary.
 */
#define XS_MPP(M)               M ## _mutex
#define XS_MUTEX(M)             pthread_mutex_t XS_MPP(M) = PTHREAD_MUTEX_INITIALIZER
#define XS_MUTEX_H(M)           extern pthread_mutex_t XS_MPP(M)
#define XS_MUTEX_LOCK(M)        pthread_mutex_lock(&XS_MPP(M))
#define XS_MUTEX_UNLOCK(M)      pthread_mutex_unlock(&XS_MPP(M))

/* Character sets used in STIL database and PlaySID file metadata
 */
#define XS_STIL_CHARSET	        "ISO-8859-1"
#define XS_SID_CHARSET          "ISO-8859-1"

/* Plugin-wide typedefs
 */
typedef struct {
    gint        tuneSpeed,
                tuneLength;
    gboolean    tunePlayed;
} xs_subtuneinfo_t;


typedef struct {
    gchar   *sidFilename,
            *sidName,
            *sidComposer,
            *sidCopyright,
            *sidFormat;
    gint    loadAddr,
            initAddr,
            playAddr,
            dataFileLen,
            sidModel;
    gint    nsubTunes, startTune;
    xs_subtuneinfo_t    *subTunes;
} xs_tuneinfo_t;


/* Plugin function prototypes
 */
gboolean    xs_init(void);
void        xs_close(void);
gboolean    xs_play_file(InputPlayback *, const gchar *, VFSFile *, gint, gint, gboolean);
void        xs_stop(InputPlayback *);
void xs_pause (InputPlayback * p, gboolean pause);
gint        xs_get_time(InputPlayback *);
Tuple *     xs_probe_for_tuple(const gchar *, VFSFile *);

void        xs_verror(const gchar *, va_list);
void        xs_error(const gchar *, ...);

#ifdef __cplusplus
}
#endif
#endif /* XMMS_SID_H */
