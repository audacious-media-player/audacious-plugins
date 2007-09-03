/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Main source file

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

   You should have received a copy of the GNU General Public License along along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "xmms-sid.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdarg.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "xs_config.h"
#include "xs_length.h"
#include "xs_stil.h"
#include "xs_filter.h"
#include "xs_fileinfo.h"
#include "xs_interface.h"
#include "xs_glade.h"
#include "xs_player.h"
#include "xs_slsup.h"
#include "audacious/playlist.h"


/*
 * Include player engines
 */
#ifdef HAVE_SIDPLAY1
#include "xs_sidplay1.h"
#endif
#ifdef HAVE_SIDPLAY2
#include "xs_sidplay2.h"
#endif


/*
 * List of players and links to their functions
 */
t_xs_player xs_playerlist[] = {
#ifdef HAVE_SIDPLAY1
	{XS_ENG_SIDPLAY1,
	 xs_sidplay1_probe,
	 xs_sidplay1_init, xs_sidplay1_close,
	 xs_sidplay1_initsong, xs_sidplay1_fillbuffer,
	 xs_sidplay1_load, xs_sidplay1_delete,
	 xs_sidplay1_getinfo, xs_sidplay1_updateinfo,
	 NULL
	},
#endif
#ifdef HAVE_SIDPLAY2
	{XS_ENG_SIDPLAY2,
	 xs_sidplay2_probe,
	 xs_sidplay2_init, xs_sidplay2_close,
	 xs_sidplay2_initsong, xs_sidplay2_fillbuffer,
	 xs_sidplay2_load, xs_sidplay2_delete,
	 xs_sidplay2_getinfo, xs_sidplay2_updateinfo,
	 xs_sidplay2_flush
	},
#endif
};

const gint xs_nplayerlist = (sizeof(xs_playerlist) / sizeof(t_xs_player));


/*
 * Global variables
 */
t_xs_status xs_status;
XS_MUTEX(xs_status);
static XS_THREAD_T xs_decode_thread;

Tuple * xs_get_song_tuple_info(gchar *songFilename, gint subTune);

/*
 * Error messages
 */
void xs_error(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "AUD-SID: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#ifndef DEBUG_NP
void XSDEBUG(const char *fmt, ...)
{
#ifdef DEBUG
	va_list ap;
	fprintf(stderr, "XSDEBUG: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
#endif
}
#endif


/*
 * Initialization functions
 */
void xs_reinit(void)
{
	gint iPlayer;
	gboolean isInitialized;

	/* Stop playing, if we are */
	XS_MUTEX_LOCK(xs_status);
	if (xs_status.isPlaying) {
		XS_MUTEX_UNLOCK(xs_status);
		xs_stop(NULL);
	} else {
		XS_MUTEX_UNLOCK(xs_status);
	}

	XS_MUTEX_LOCK(xs_status);
	XS_MUTEX_LOCK(xs_cfg);

	/* Initialize status and sanitize configuration */
	xs_memset(&xs_status, 0, sizeof(xs_status));

	if (xs_cfg.audioFrequency < 8000)
		xs_cfg.audioFrequency = 8000;

	if (xs_cfg.oversampleFactor < XS_MIN_OVERSAMPLE)
		xs_cfg.oversampleFactor = XS_MIN_OVERSAMPLE;
	else if (xs_cfg.oversampleFactor > XS_MAX_OVERSAMPLE)
		xs_cfg.oversampleFactor = XS_MAX_OVERSAMPLE;

	if (xs_cfg.audioChannels != XS_CHN_MONO)
		xs_cfg.oversampleEnable = FALSE;

	xs_status.audioFrequency = xs_cfg.audioFrequency;
	xs_status.audioBitsPerSample = xs_cfg.audioBitsPerSample;
	xs_status.audioChannels = xs_cfg.audioChannels;
	xs_status.audioFormat = -1;
	xs_status.oversampleEnable = xs_cfg.oversampleEnable;
	xs_status.oversampleFactor = xs_cfg.oversampleFactor;

	/* Try to initialize emulator engine */
	XSDEBUG("initializing emulator engine #%i...\n", xs_cfg.playerEngine);

	iPlayer = 0;
	isInitialized = FALSE;
	while ((iPlayer < xs_nplayerlist) && !isInitialized) {
		if (xs_playerlist[iPlayer].plrIdent == xs_cfg.playerEngine) {
			if (xs_playerlist[iPlayer].plrInit(&xs_status)) {
				isInitialized = TRUE;
				xs_status.sidPlayer = (t_xs_player *) & xs_playerlist[iPlayer];
			}
		}
		iPlayer++;
	}

	XSDEBUG("init#1: %s, %i\n", (isInitialized) ? "OK" : "FAILED", iPlayer);

	iPlayer = 0;
	while ((iPlayer < xs_nplayerlist) && !isInitialized) {
		if (xs_playerlist[iPlayer].plrInit(&xs_status)) {
			isInitialized = TRUE;
			xs_status.sidPlayer = (t_xs_player *) & xs_playerlist[iPlayer];
			xs_cfg.playerEngine = xs_playerlist[iPlayer].plrIdent;
		} else
			iPlayer++;
	}

	XSDEBUG("init#2: %s, %i\n", (isInitialized) ? "OK" : "FAILED", iPlayer);


	/* Get settings back, in case the chosen emulator backend changed them */
	xs_cfg.audioFrequency = xs_status.audioFrequency;
	xs_cfg.audioBitsPerSample = xs_status.audioBitsPerSample;
	xs_cfg.audioChannels = xs_status.audioChannels;
	xs_cfg.oversampleEnable = xs_status.oversampleEnable;

	XS_MUTEX_UNLOCK(xs_status);
	XS_MUTEX_UNLOCK(xs_cfg);

	/* Initialize song-length database */
	xs_songlen_close();
	if (xs_cfg.songlenDBEnable && (xs_songlen_init() != 0)) {
		xs_error(_("Error initializing song-length database!\n"));
	}

	/* Initialize STIL database */
	xs_stil_close();
	if (xs_cfg.stilDBEnable && (xs_stil_init() != 0)) {
		xs_error(_("Error initializing STIL database!\n"));
	}

}


/*
 * Initialize XMMS-SID
 */
void xs_init(void)
{
	XSDEBUG("xs_init()\n");

	/* Initialize and get configuration */
	xs_memset(&xs_cfg, 0, sizeof(xs_cfg));
	xs_init_configuration();
	xs_read_configuration();

	/* Initialize subsystems */
	xs_reinit();

	XSDEBUG("OK\n");
}


/*
 * Shut down XMMS-SID
 */
void xs_close(void)
{
	XSDEBUG("xs_close(): shutting down...\n");

	/* Stop playing, free structures */
	xs_stop(NULL);

	xs_tuneinfo_free(xs_status.tuneInfo);
	xs_status.tuneInfo = NULL;
	xs_status.sidPlayer->plrDeleteSID(&xs_status);
	xs_status.sidPlayer->plrClose(&xs_status);

	xs_songlen_close();
	xs_stil_close();

	XSDEBUG("shutdown finished.\n");
}


/*
 * Check whether the given file is handled by this plugin
 */
gint xs_is_our_file(gchar *pcFilename)
{
	gint result = 0;
	t_xs_file *f;
	assert(xs_status.sidPlayer);

	/* Check the filename */
	if (pcFilename == NULL)
		return 0;

//	XS_MUTEX_LOCK(xs_status);
	if ((f = xs_fopen(pcFilename, "rb")) != NULL) {
		if (xs_status.sidPlayer->plrProbe(f))
			result = 1;
		xs_fclose(f);
	}

//	XS_MUTEX_UNLOCK(xs_status);
	return result;
}


gboolean xs_get_trackinfo(const gchar *pcFilename, gchar **pcResult, gint *pTrack)
{
	gchar *tmpSep;

	*pcResult = g_strdup(pcFilename);
	tmpSep = xs_strrchr(*pcResult, '?');

	if (tmpSep && g_ascii_isdigit(*(tmpSep + 1))) {
		*tmpSep = '\0';
		*pTrack = atoi(tmpSep + 1);
		return TRUE;
	} else {
		*pTrack = -1;
		return FALSE;
	}
}


gint xs_is_our_file_vfs(gchar *pcFilename, t_xs_file *f)
{
	gint tmpResult = 0;
	assert(xs_status.sidPlayer);

	/* Check the filename */
	if (pcFilename == NULL)
		return 0;
	
	if (xs_status.sidPlayer->plrProbe(f)) {
		gchar *tmpFilename = NULL;
		gint tmpDummy = 0;

		if (xs_get_trackinfo(pcFilename, &tmpFilename, &tmpDummy)) {
			tmpResult = 1;
		} else {
			t_xs_tuneinfo *pInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename);
		
			if (pInfo->nsubTunes > 1) {
				gint i;
				
				for (i = 1; i <= pInfo->nsubTunes; i++) {
					gchar *tmpStr = g_strdup_printf("%s?%d", pcFilename, i);
					gboolean doAdd = FALSE;
					
					if (xs_cfg.subAutoMinOnly) {
						if (i == pInfo->startTune ||
							pInfo->subTunes[i - 1].tuneLength >= xs_cfg.subAutoMinTime)
							doAdd = TRUE;
					} else
						doAdd = TRUE;
					
					if (doAdd)
						playlist_add_url(playlist_get_active(), tmpStr);

					g_free(tmpStr);
				}

				tmpResult = -1;
			} else
				tmpResult = 1;
			
			xs_tuneinfo_free(pInfo);
		}
		
		g_free(tmpFilename);
	}
	
	return tmpResult;
}


/*
 * Start playing the given file
 */
void xs_play_file(InputPlayback *pb)
{
	t_xs_status myStatus;
	t_xs_tuneinfo *myTune;
	gboolean audioOpen = FALSE;
	gint audioGot, songLength, i, subTune;
	gchar *tmpFilename, *audioBuffer = NULL, *oversampleBuffer = NULL, *tmpTitle;
	Tuple *tmpTuple;

	assert(pb);
	assert(xs_status.sidPlayer);
	
	XSDEBUG("play '%s'\n", pb->filename);

	XS_MUTEX_LOCK(xs_status);

	/* Get tune information */
	xs_get_trackinfo(pb->filename, &tmpFilename, &subTune);

	if ((xs_status.tuneInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename)) == NULL) {
		g_free(tmpFilename);
		return;
	}

	/* Initialize the tune */
	if (!xs_status.sidPlayer->plrLoadSID(&xs_status, tmpFilename)) {
		g_free(tmpFilename);
		xs_tuneinfo_free(xs_status.tuneInfo);
		xs_status.tuneInfo = NULL;
		return;
	}
	
	g_free(tmpFilename);
	tmpFilename = NULL;

	XSDEBUG("load ok\n");

	/* Set general status information */
	xs_status.isPlaying = TRUE;
	xs_status.isError = FALSE;
	myTune = xs_status.tuneInfo;

	if (subTune < 1 || subTune > xs_status.tuneInfo->nsubTunes)
		xs_status.currSong = xs_status.tuneInfo->startTune;
	else
		xs_status.currSong = subTune;

	XSDEBUG("subtune #%i selected (#%d wanted), initializing...\n", xs_status.currSong, subTune);
	memcpy(&myStatus, &xs_status, sizeof(t_xs_status));
	XS_MUTEX_UNLOCK(xs_status);


	/* We are ready */
	xs_decode_thread = g_thread_self();
	pb->set_pb_ready(pb);


	/* Allocate audio buffer */
	audioBuffer = (gchar *) g_malloc(XS_AUDIOBUF_SIZE);
	if (audioBuffer == NULL) {
		xs_error(_("Couldn't allocate memory for audio data buffer!\n"));
		goto xs_err_exit;
	}

	if (myStatus.oversampleEnable) {
		oversampleBuffer = (gchar *) g_malloc(XS_AUDIOBUF_SIZE * myStatus.oversampleFactor);
		if (oversampleBuffer == NULL) {
			xs_error(_("Couldn't allocate memory for audio oversampling buffer!\n"));
			goto xs_err_exit;
		}
	}


	/* Check minimum playtime */
	songLength = myTune->subTunes[myStatus.currSong - 1].tuneLength;
	if (xs_cfg.playMinTimeEnable && (songLength >= 0)) {
		if (songLength < xs_cfg.playMinTime)
			songLength = xs_cfg.playMinTime;
	}

	/* Initialize song */
	if (!myStatus.sidPlayer->plrInitSong(&myStatus)) {
		xs_error(_("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n"),
		      myTune->sidFilename, myStatus.currSong);
		goto xs_err_exit;
	}
		
	/* Open the audio output */
	XSDEBUG("open audio output (%d, %d, %d)\n",
		myStatus.audioFormat, myStatus.audioFrequency, myStatus.audioChannels);
		
	if (!pb->output->open_audio(myStatus.audioFormat, myStatus.audioFrequency, myStatus.audioChannels)) {
		xs_error(_("Couldn't open XMMS audio output (fmt=%x, freq=%i, nchan=%i)!\n"),
			myStatus.audioFormat,
			myStatus.audioFrequency,
			myStatus.audioChannels);

		XS_MUTEX_LOCK(xs_status);
		xs_status.isError = TRUE;
		XS_MUTEX_UNLOCK(xs_status);
		goto xs_err_exit;
	}

	audioOpen = TRUE;

	/* Set song information for current subtune */
	XSDEBUG("foobar\n");
	myStatus.sidPlayer->plrUpdateSIDInfo(&myStatus);

	tmpTuple = xs_get_song_tuple_info(myTune->sidFilename, myStatus.currSong);
	tmpTitle = tuple_formatter_process_string(tmpTuple, get_gentitle_format());
	
	xs_plugin_ip.set_info(
		tmpTitle,
		(songLength > 0) ? (songLength * 1000) : 0,
		-1,
		myStatus.audioFrequency,
		myStatus.audioChannels);
		
	g_free(tmpTitle);
	

	XSDEBUG("playing\n");
	while (xs_status.isPlaying && myStatus.isPlaying) {
		/* Render audio data */
		if (myStatus.oversampleEnable) {
			/* Perform oversampled rendering */
			audioGot = myStatus.sidPlayer->plrFillBuffer(
				&myStatus,
				oversampleBuffer,
				(XS_AUDIOBUF_SIZE * myStatus.oversampleFactor));

			audioGot /= myStatus.oversampleFactor;

			/* Execute rate-conversion with filtering */
			if (xs_filter_rateconv(audioBuffer, oversampleBuffer,
				myStatus.audioFormat, myStatus.oversampleFactor, audioGot) < 0) {
				xs_error(_("Oversampling rate-conversion pass failed.\n"));
				XS_MUTEX_LOCK(xs_status);
				xs_status.isError = TRUE;
				XS_MUTEX_UNLOCK(xs_status);
				goto xs_err_exit;
			}
		} else {
			audioGot = myStatus.sidPlayer->plrFillBuffer(
				&myStatus, audioBuffer, XS_AUDIOBUF_SIZE);
		}

		/* I <3 visualice/haujobb */
		produce_audio(pb->output->written_time(),
			myStatus.audioFormat, myStatus.audioChannels,
			audioGot, audioBuffer, NULL);

		/* Wait a little */
		while (xs_status.isPlaying && (pb->output->buffer_free() < audioGot))
			xmms_usleep(500);

		/* Check if we have played enough */
		if (xs_cfg.playMaxTimeEnable) {
			if (xs_cfg.playMaxTimeUnknown) {
				if ((songLength < 0) &&
					(pb->output->output_time() >= (xs_cfg.playMaxTime * 1000)))
					myStatus.isPlaying = FALSE;
			} else {
				if (pb->output->output_time() >= (xs_cfg.playMaxTime * 1000))
					myStatus.isPlaying = FALSE;
			}
		}

		if (songLength >= 0) {
			if (pb->output->output_time() >= (songLength * 1000))
				myStatus.isPlaying = FALSE;
		}
	}

xs_err_exit:
	XSDEBUG("out of playing loop\n");
	
	/* Close audio output plugin */
	if (audioOpen) {
		XSDEBUG("close audio #2\n");
		pb->output->close_audio();
		XSDEBUG("closed\n");
	}

	g_free(audioBuffer);
	g_free(oversampleBuffer);

	/* Set playing status to false (stopped), thus when
	 * XMMS next calls xs_get_time(), it can return appropriate
	 * value "not playing" status and XMMS knows to move to
	 * next entry in the playlist .. or whatever it wishes.
	 */
	XS_MUTEX_LOCK(xs_status);
	xs_status.isPlaying = FALSE;
	XS_MUTEX_UNLOCK(xs_status);

	/* Exit the playing thread */
	XSDEBUG("exiting thread, bye.\n");
}


/*
 * Stop playing
 * Here we set the playing status to stop and wait for playing
 * thread to shut down. In any "correctly" done plugin, this is
 * also the function where you close the output-plugin, but since
 * XMMS-SID has special behaviour (audio opened/closed in the
 * playing thread), we don't do that here.
 *
 * Finally tune and other memory allocations are free'd.
 */
void xs_stop(InputPlayback *pb)
{
	(void) pb;
	
	XSDEBUG("stop requested\n");

	/* Lock xs_status and stop playing thread */
	XS_MUTEX_LOCK(xs_status);
	if (xs_status.isPlaying) {
		XSDEBUG("stopping...\n");
		xs_status.isPlaying = FALSE;
		XS_MUTEX_UNLOCK(xs_status);
		XS_THREAD_JOIN(xs_decode_thread);
	} else {
		XS_MUTEX_UNLOCK(xs_status);
	}

	XSDEBUG("done, updating status\n");
	
	/* Free tune information */
	XS_MUTEX_LOCK(xs_status);
	xs_status.sidPlayer->plrDeleteSID(&xs_status);
	xs_tuneinfo_free(xs_status.tuneInfo);
	xs_status.tuneInfo = NULL;
	XS_MUTEX_UNLOCK(xs_status);
	XSDEBUG("ok\n");
}


/*
 * Pause/unpause the playing
 */
void xs_pause(InputPlayback *pb, short pauseState)
{
	pb->output->pause(pauseState);
}


/*
 * A stub seek function (Audacious will crash if seek is NULL)
 */
void xs_seek(InputPlayback *pb, gint iTime)
{
}


/*
 * Return the playing "position/time"
 * Determine current position/time in song. Used by XMMS to update
 * the song clock and position slider and MOST importantly to determine
 * END OF SONG! Return value of -2 means error, XMMS opens an audio
 * error dialog. -1 means end of song (if one was playing currently).
 */
gint xs_get_time(InputPlayback *pb)
{
	/* If errorflag is set, return -2 to signal it to XMMS's idle callback */
	XS_MUTEX_LOCK(xs_status);
	if (xs_status.isError) {
		XS_MUTEX_UNLOCK(xs_status);
		return -2;
	}

	/* If there is no tune, return -1 */
	if (!xs_status.tuneInfo) {
		XS_MUTEX_UNLOCK(xs_status);
		return -1;
	}

	/* If tune has ended, return -1 */
	if (!xs_status.isPlaying) {
		XS_MUTEX_UNLOCK(xs_status);
		return -1;
	}

	XS_MUTEX_UNLOCK(xs_status);

	/* Return output time reported by audio output plugin */
	return pb->output->output_time();
}


/* Return song information Tuple
 */
Tuple * xs_get_song_tuple_info(gchar *songFilename, gint subTune)
{
	t_xs_tuneinfo *pInfo;
	Tuple *pResult;
	gchar *tmpStr;

	pResult = tuple_new_from_filename(songFilename);

	/* Get tune information from emulation engine */
	XS_MUTEX_LOCK(xs_status);
	pInfo = xs_status.sidPlayer->plrGetSIDInfo(songFilename);
	if (!pInfo) {
		XS_MUTEX_UNLOCK(xs_status);
		return pResult;
	}
	XS_MUTEX_UNLOCK(xs_status);
	
	tuple_associate_string(pResult, "title", pInfo->sidName);
	tuple_associate_string(pResult, "artist", pInfo->sidComposer);
	tuple_associate_string(pResult, "genre", "SID-tune");
	tuple_associate_string(pResult, "copyright", pInfo->sidCopyright);
	tuple_associate_string(pResult, "format", pInfo->sidFormat);
	tuple_associate_int(pResult, "subtunes", pInfo->nsubTunes);

	switch (pInfo->sidModel) {
		case XS_SIDMODEL_6581: tmpStr = "6581"; break;
		case XS_SIDMODEL_8580: tmpStr = "8580"; break;
		case XS_SIDMODEL_ANY: tmpStr = "ANY"; break;
		default: tmpStr = "?"; break;
	}
	tuple_associate_string(pResult, "sid-model", tmpStr);

	/* Get sub-tune information, if available */
	if (subTune < 0 || pInfo->startTune > pInfo->nsubTunes)
		subTune = pInfo->startTune;
	
	if ((subTune > 0) && (subTune <= pInfo->nsubTunes)) {
		gint tmpInt = pInfo->subTunes[subTune - 1].tuneLength;
		tuple_associate_int(pResult, "length", (tmpInt < 0) ? -1 : tmpInt * 1000);
	} else
		subTune = 1;

	tuple_associate_int(pResult, "subtune", subTune);
	tuple_associate_int(pResult, "track-number", subTune);

	/* Free tune information */
	xs_tuneinfo_free(pInfo);
	return pResult;
}

Tuple * xs_get_song_tuple(gchar *songFilename)
{
	Tuple *pResult;
	gchar *tmpFilename;
	gint subTune;

	xs_get_trackinfo(songFilename, &tmpFilename, &subTune);
	pResult = xs_get_song_tuple_info(tmpFilename, subTune);
	g_free(tmpFilename);

	return pResult;
}
