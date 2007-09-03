/*  
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Main header file

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
 * purposes. Output goes to stderr. See also DEBUG_NP below.
 */
#undef DEBUG

/* Define to ISO C99 macro for debugging instead of varargs function.
 * This provides more useful information, but is incompatible with
 * older standards. If #undef'd, a varargs function is used instead.
 */
#define DEBUG_NP

/* Define to enable non-portable thread and mutex debugging code.
 * You need to #define DEBUG also to make this useful.
 * (Works probably with GNU/Linux pthreads implementation only)
 */
#undef XS_MUTEX_DEBUG

/* HardSID-support is not working and is untested, thus we disable it here.
 */
#undef HAVE_HARDSID_BUILDER

/* Size for some small buffers (always static variables) */
#define XS_BUF_SIZE		(1024)

/* If defined, some dynamically allocated temp. buffers are used.
 * Static (#undef) might give slight performance gain,
 * but fails on systems with limited stack space. */
#define XS_BUF_DYNAMIC

/* Configuration section identifier
 */
#define XS_PACKAGE_STRING	"Audacious-SID v0.8.0beta18"
#define XS_CONFIG_IDENT		"sid"

/* Default audio rendering frequency in Hz
 */
#define XS_AUDIO_FREQ		(44100)

/* Size of audio buffer. If you are experiencing lots of audio
 * "underruns" or clicks/gaps in output, try increasing this value.
 * Do notice, however, that it also affects the update frequency of
 * XMMS's visualization plugins... 
 */
#define XS_AUDIOBUF_SIZE	(2*1024)

/* Size of data buffer used for SID-tune MD5 hash calculation.
 * If this is too small, the computed hash will be incorrect.
 * Largest SID files I've seen are ~70kB. */
#define XS_SIDBUF_SIZE		(80*1024)

/* libSIDPlay1/2 constants (copied from internal headers/source)
 * For some stupid reason these are not available in public
 * headers, so we have to duplicate them here...
 */
#define XS_SIDPLAY1_FS		(400.0f)
#define XS_SIDPLAY1_FM		(60.0f)
#define XS_SIDPLAY1_FT		(0.05f)

#define XS_SIDPLAY2_NFPOINTS	(0x800)
#define XS_SIDPLAY2_FMAX	(24000)

/* Limits for oversampling
 */
#define XS_MIN_OVERSAMPLE	(2)
#define XS_MAX_OVERSAMPLE	(8)


/* Macros for mutexes and threads. These exist to be able to
 * easily change from pthreads to glib threads, etc, if necessary.
 */
#define XS_THREAD_T		GThread *
#define XS_THREAD_EXIT(M)	g_thread_exit(M)
#define XS_THREAD_JOIN(M)	g_thread_join(M)
#define XS_MPP(M)		M ## _mutex
#define XS_MUTEX(M)		GStaticMutex	XS_MPP(M) = G_STATIC_MUTEX_INIT
#define XS_MUTEX_H(M)		extern GStaticMutex XS_MPP(M)
#define XS_MUTEX_LOCK(M)	g_static_mutex_lock(&XS_MPP(M))
#define XS_MUTEX_UNLOCK(M)	g_static_mutex_unlock(&XS_MPP(M))

/* Character set conversion helper macros
 */
#define XS_CS_FILENAME(M)	g_filename_to_utf8(M, -1, NULL, NULL, NULL)
#define XS_CS_SID(M)		g_convert(M, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL)
#define XS_CS_STIL(M)		g_convert(M, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL)
#define XS_CS_FREE(M)		g_free(M)

/* Shorthands for linked lists
 */
#define LPREV	(pNode->pPrev)
#define LTHIS	(pNode)
#define LNEXT	(pNode->pNext)


/* Plugin-wide typedefs
 */
typedef struct {
	gint		tuneSpeed,
			tuneLength;
	gboolean	tunePlayed;
} t_xs_subtuneinfo;


typedef struct {
	gchar		*sidFilename,
			*sidName,
			*sidComposer,
			*sidCopyright,
			*sidFormat;
	gint		loadAddr,
			initAddr,
			playAddr,
			dataFileLen,
			sidModel;
	gint		nsubTunes, startTune;
	t_xs_subtuneinfo	*subTunes;
} t_xs_tuneinfo;


/* Global variables
 */
extern InputPlugin	xs_plugin_ip;


/* Plugin function prototypes
 */
void	xs_init(void);
void	xs_reinit(void);
void	xs_close(void);
gint	xs_is_our_file(gchar *);
gint	xs_is_our_file_vfs(gchar *, t_xs_file *);
void	xs_play_file(InputPlayback *);
void	xs_stop(InputPlayback *);
void	xs_pause(InputPlayback *, short);
void	xs_seek(InputPlayback *, gint);
gint	xs_get_time(InputPlayback *);
Tuple *	xs_get_song_tuple(gchar *);
void	xs_about(void);


t_xs_tuneinfo *xs_tuneinfo_new(const gchar * pcFilename,
		gint nsubTunes, gint startTune, const gchar * sidName,
		const gchar * sidComposer, const gchar * sidCopyright,
		gint loadAddr, gint initAddr, gint playAddr,
		gint dataFileLen, const gchar *sidFormat, gint sidModel);
void	xs_tuneinfo_free(t_xs_tuneinfo *);

void	xs_error(const char *, ...);


/* Debugging
 */
#ifndef DEBUG_NP
void	XSDEBUG(const char *, ...);
#else
#  ifdef DEBUG
#    define XSDEBUG(...) { fprintf(stderr, "XS[%s:%s:%d]: ", __FILE__, __FUNCTION__, (int) __LINE__); fprintf(stderr, __VA_ARGS__); }
#  else
#    define XSDEBUG(...) /* stub */
#  endif
#endif


/* And even some Gtk+ macro crap here, yay.
 */
#define XS_DEF_WINDOW_DELETE(ME, MV)					\
gboolean xs_ ## ME ## _delete(GtkWidget *w, GdkEvent *e, gpointer d) {	\
	(void) w; (void) e; (void) d;					\
	if (xs_ ## MV ) {						\
		gtk_widget_destroy(xs_ ## MV );				\
		xs_ ## MV = NULL;					\
	}								\
	return FALSE;							\
}

#define XS_DEF_WINDOW_CLOSE(ME, MV)			\
void xs_ ## ME (GtkButton *b, gpointer d) {		\
	(void) b; (void) d;				\
	gtk_widget_destroy(xs_ ## MV );			\
	xs_ ## MV = NULL;				\
}

#define XS_SIGNAL_CONNECT(SOBJ, SNAME, SFUNC, SDATA)		\
	g_signal_connect(G_OBJECT(SOBJ), SNAME, G_CALLBACK(SFUNC), SDATA)

#ifdef __cplusplus
}
#endif
#endif /* XMMS_SID_H */
