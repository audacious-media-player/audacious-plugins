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

#include "xs_support.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Some constants and defines
 */
/* #define to enable spurious debugging messages for development
 * purposes. Output through g_logv().
 */
#undef DEBUG

/* Define to enable non-portable thread and mutex debugging code.
 * You need to #define DEBUG also to make this useful.
 * (Works probably with GNU/Linux pthreads implementation only)
 */
#undef XS_MUTEX_DEBUG

/* HardSID-support is not working and is untested, thus we disable it here.
 */
#undef HAVE_HARDSID_BUILDER

/* Size for some small buffers (always static variables) */
#define XS_BUF_SIZE             (1024)

/* If defined, some dynamically allocated temp. buffers are used.
 * Static (#undef) might give slight performance gain,
 * but fails on systems with limited stack space. */
#define XS_BUF_DYNAMIC

/* Configuration section identifier
 */
#define XS_PACKAGE_STRING       "Audacious-SID"
#define XS_CONFIG_IDENT         "sid"

/* Default audio rendering frequency in Hz
 */
#define XS_AUDIO_FREQ           (44100)

/* Size of data buffer used for SID-tune MD5 hash calculation.
 * If this is too small, the computed hash will be incorrect.
 * Largest SID files I've seen are ~70kB. */
#define XS_SIDBUF_SIZE          (128*1024)

/* libSIDPlay1/2 constants (copied from internal headers/source)
 * For some stupid reason these are not available in public
 * headers, so we have to duplicate them here...
 */
#define XS_SIDPLAY1_FS          (400.0f)
#define XS_SIDPLAY1_FM          (60.0f)
#define XS_SIDPLAY1_FT          (0.05f)

#define XS_SIDPLAY2_NFPOINTS    (0x800)
#define XS_SIDPLAY2_FMAX        (24000)

/* Limits for oversampling
 */
#define XS_MIN_OVERSAMPLE       (2)
#define XS_MAX_OVERSAMPLE       (8)


/* Macros for mutexes and threads. These exist to be able to
 * easily change from pthreads to glib threads, etc, if necessary.
 */
#define XS_THREAD_T             GThread *
#define XS_THREAD_EXIT(M)       g_thread_exit(M)
#define XS_THREAD_JOIN(M)       g_thread_join(M)
#define XS_MPP(M)               M ## _mutex
#define XS_MUTEX(M)             GStaticMutex XS_MPP(M) = G_STATIC_MUTEX_INIT
#define XS_MUTEX_H(M)           extern GStaticMutex XS_MPP(M)
#ifdef XS_MUTEX_DEBUG
#  define XS_MUTEX_LOCK(M)    {                             \
    gboolean tmpRes;                                        \
    XSDEBUG("XS_MUTEX_TRYLOCK(" #M ")\n");                  \
    tmpRes = g_static_mutex_trylock(&XS_MPP(M));            \
    XSDEBUG("[" #M "] = %s\n", tmpRes ? "TRUE" : "FALSE");  \
    }
#  define XS_MUTEX_UNLOCK(M)    { XSDEBUG("XS_MUTEX_UNLOCK(" #M ")\n"); g_static_mutex_unlock(&XS_MPP(M)); }
#else
#  define XS_MUTEX_LOCK(M)      g_static_mutex_lock(&XS_MPP(M))
#  define XS_MUTEX_UNLOCK(M)    g_static_mutex_unlock(&XS_MPP(M))
#endif

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


/* Global variables
 */
extern InputPlugin    xs_plugin_ip;


/* Plugin function prototypes
 */
gboolean    xs_init(void);
void        xs_reinit(void);
void        xs_close(void);
gboolean    xs_play_file(InputPlayback *, const gchar *, VFSFile *, gint, gint, gboolean);
void        xs_stop(InputPlayback *);
void xs_pause (InputPlayback * p, gboolean pause);
gint        xs_get_time(InputPlayback *);
Tuple *     xs_probe_for_tuple(const gchar *, xs_file_t *);
void        xs_about(void);

void        xs_verror(const gchar *, va_list);
void        xs_error(const gchar *, ...);


/* Debugging
 */
void    XSDEBUG(const gchar *, ...);


/* And even some Gtk+ macro crap here, yay.
 */
#define XS_DEF_WINDOW_DELETE(ME, MV)                    \
gboolean xs_ ## ME ## _delete(GtkWidget *w, GdkEvent *e, gpointer d) {    \
    (void) w; (void) e; (void) d;                    \
    if (xs_ ## MV ) {                        \
        gtk_widget_destroy(xs_ ## MV );                \
        xs_ ## MV = NULL;                    \
    }                                \
    return FALSE;                            \
}

#define XS_DEF_WINDOW_CLOSE(ME, MV)            \
void xs_ ## ME (GtkButton *b, gpointer d) {        \
    (void) b; (void) d;                \
    gtk_widget_destroy(xs_ ## MV );            \
    xs_ ## MV = NULL;                \
}

#define XS_SIGNAL_CONNECT(SOBJ, SNAME, SFUNC, SDATA)        \
    g_signal_connect(G_OBJECT(SOBJ), SNAME, G_CALLBACK(SFUNC), SDATA)

#define XS_WINDOW_PRESENT(XX) gtk_window_present(GTK_WINDOW( XX ))

#ifdef __cplusplus
}
#endif
#endif /* XMMS_SID_H */
