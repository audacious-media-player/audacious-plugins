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

#include "xs_config.h"
#include "xs_length.h"
#include "xs_stil.h"
#include "xs_filter.h"
#include "xs_fileinfo.h"
#include "xs_interface.h"
#include "xs_glade.h"
#include "xs_player.h"
#include "xs_slsup.h"


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

void xs_get_song_aud_tuple_info(Tuple *pResult, t_xs_tuneinfo *pInfo, gint subTune);

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

	XSDEBUG("xs_reinit() thread = %p\n", g_thread_self());

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
static gchar * xs_has_tracknumber(gchar *pcFilename)
{
	gchar *tmpSep = xs_strrchr(pcFilename, '?');
	if (tmpSep && g_ascii_isdigit(*(tmpSep + 1)))
		return tmpSep;
	else
		return NULL;
}

gboolean xs_get_trackinfo(const gchar *pcFilename, gchar **pcResult, gint *pTrack)
{
	gchar *tmpSep;

	*pcResult = g_strdup(pcFilename);
	tmpSep = xs_has_tracknumber(*pcResult);

	if (tmpSep) {
		*tmpSep = '\0';
		*pTrack = atoi(tmpSep + 1);
		return TRUE;
	} else {
		*pTrack = -1;
		return FALSE;
	}
}


/*
 * Start playing the given file
 */
void xs_play_file(InputPlayback *pb)
{
	t_xs_tuneinfo *tmpTune;
	gboolean audioOpen = FALSE;
	gint audioGot, tmpLength, subTune;
	gchar *tmpFilename, *audioBuffer = NULL, *oversampleBuffer = NULL, *tmpTitle;
	Tuple *tmpTuple;

	assert(pb);
	assert(xs_status.sidPlayer);
	
	XSDEBUG("play '%s'\n", pb->filename);

	XS_MUTEX_LOCK(xs_status);

	/* Get tune information */
	xs_get_trackinfo(pb->filename, &tmpFilename, &subTune);
	if ((xs_status.tuneInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename)) == NULL) {
		XS_MUTEX_UNLOCK(xs_status);
		g_free(tmpFilename);
		return;
	}

	/* Initialize the tune */
	if (!xs_status.sidPlayer->plrLoadSID(&xs_status, tmpFilename)) {
		XS_MUTEX_UNLOCK(xs_status);
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
	tmpTune = xs_status.tuneInfo;

	if (subTune < 1 || subTune > xs_status.tuneInfo->nsubTunes)
		xs_status.currSong = xs_status.tuneInfo->startTune;
	else
		xs_status.currSong = subTune;

	XSDEBUG("subtune #%i selected (#%d wanted), initializing...\n", xs_status.currSong, subTune);


	/* We are ready */
	xs_decode_thread = g_thread_self();
	XSDEBUG("playing thread = %p\n", xs_decode_thread);
	pb->set_pb_ready(pb);


	/* Allocate audio buffer */
	audioBuffer = (gchar *) g_malloc(XS_AUDIOBUF_SIZE);
	if (audioBuffer == NULL) {
		xs_error(_("Couldn't allocate memory for audio data buffer!\n"));
		XS_MUTEX_UNLOCK(xs_status);
		goto xs_err_exit;
	}
	
	if (xs_status.oversampleEnable) {
		oversampleBuffer = (gchar *) g_malloc(XS_AUDIOBUF_SIZE * xs_status.oversampleFactor);
		if (oversampleBuffer == NULL) {
			xs_error(_("Couldn't allocate memory for audio oversampling buffer!\n"));
			XS_MUTEX_UNLOCK(xs_status);
			goto xs_err_exit;
		}
	}


	/* Check minimum playtime */
	tmpLength = tmpTune->subTunes[xs_status.currSong - 1].tuneLength;
	if (xs_cfg.playMinTimeEnable && (tmpLength >= 0)) {
		if (tmpLength < xs_cfg.playMinTime)
			tmpLength = xs_cfg.playMinTime;
	}

	/* Initialize song */
	if (!xs_status.sidPlayer->plrInitSong(&xs_status)) {
		xs_error(_("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n"),
		      tmpTune->sidFilename, xs_status.currSong);
		XS_MUTEX_UNLOCK(xs_status);
		goto xs_err_exit;
	}
		
	/* Open the audio output */
	XSDEBUG("open audio output (%d, %d, %d)\n",
		xs_status.audioFormat, xs_status.audioFrequency, xs_status.audioChannels);
		
	if (!pb->output->open_audio(xs_status.audioFormat, xs_status.audioFrequency, xs_status.audioChannels)) {
		xs_error(_("Couldn't open XMMS audio output (fmt=%x, freq=%i, nchan=%i)!\n"),
			xs_status.audioFormat,
			xs_status.audioFrequency,
			xs_status.audioChannels);

		xs_status.isError = TRUE;
		XS_MUTEX_UNLOCK(xs_status);
		goto xs_err_exit;
	}

	audioOpen = TRUE;

	/* Set song information for current subtune */
	XSDEBUG("foobar #1\n");
	xs_status.sidPlayer->plrUpdateSIDInfo(&xs_status);
	XS_MUTEX_UNLOCK(xs_status);
	tmpTuple = aud_tuple_new_from_filename(tmpTune->sidFilename);
	xs_get_song_aud_tuple_info(tmpTuple, tmpTune, xs_status.currSong);

	tmpTitle = aud_tuple_formatter_process_string(tmpTuple,
		xs_cfg.titleOverride ? xs_cfg.titleFormat : get_gentitle_format());
	
	XSDEBUG("foobar #4\n");
	XS_MUTEX_LOCK(xs_status);
	pb->set_params(pb,
		tmpTitle,
		(tmpLength > 0) ? (tmpLength * 1000) : 0,
		-1,
		xs_status.audioFrequency,
		xs_status.audioChannels);
		
	g_free(tmpTitle);
	
	XS_MUTEX_UNLOCK(xs_status);
	XSDEBUG("playing\n");
	while (xs_status.isPlaying) {
		/* Render audio data */
		XS_MUTEX_LOCK(xs_status);
		if (xs_status.oversampleEnable) {
			/* Perform oversampled rendering */
			audioGot = xs_status.sidPlayer->plrFillBuffer(
				&xs_status,
				oversampleBuffer,
				(XS_AUDIOBUF_SIZE * xs_status.oversampleFactor));

			audioGot /= xs_status.oversampleFactor;

			/* Execute rate-conversion with filtering */
			if (xs_filter_rateconv(audioBuffer, oversampleBuffer,
				xs_status.audioFormat, xs_status.oversampleFactor, audioGot) < 0) {
				xs_error(_("Oversampling rate-conversion pass failed.\n"));
				xs_status.isError = TRUE;
				XS_MUTEX_UNLOCK(xs_status);
				goto xs_err_exit;
			}
		} else {
			audioGot = xs_status.sidPlayer->plrFillBuffer(
				&xs_status, audioBuffer, XS_AUDIOBUF_SIZE);
		}

		/* I <3 visualice/haujobb */
		pb->pass_audio(pb, xs_status.audioFormat, xs_status.audioChannels,
			audioGot, audioBuffer, NULL);
		
		XS_MUTEX_UNLOCK(xs_status);

		/* Wait a little */
		while (xs_status.isPlaying && (pb->output->buffer_free() < audioGot))
			g_usleep(500);

		/* Check if we have played enough */
		XS_MUTEX_LOCK(xs_status);
		if (xs_cfg.playMaxTimeEnable) {
			if (xs_cfg.playMaxTimeUnknown) {
				if ((tmpLength < 0) &&
					(pb->output->output_time() >= (xs_cfg.playMaxTime * 1000)))
					xs_status.isPlaying = FALSE;
			} else {
				if (pb->output->output_time() >= (xs_cfg.playMaxTime * 1000))
					xs_status.isPlaying = FALSE;
			}
		}

		if (tmpLength >= 0) {
			if (pb->output->output_time() >= (tmpLength * 1000))
				xs_status.isPlaying = FALSE;
		}
		XS_MUTEX_UNLOCK(xs_status);
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
void xs_seek(InputPlayback *pb, gint time)
{
	(void) pb; (void) time;
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


/*
 * Return song information Tuple
 */
void xs_get_song_aud_tuple_info(Tuple *pResult, t_xs_tuneinfo *pInfo, gint subTune)
{
	gchar *tmpStr, tmpStr2[64];

	aud_tuple_associate_string(pResult, FIELD_TITLE, NULL, pInfo->sidName);
	aud_tuple_associate_string(pResult, FIELD_ARTIST, NULL, pInfo->sidComposer);
	aud_tuple_associate_string(pResult, FIELD_GENRE, NULL, "SID-tune");
	aud_tuple_associate_string(pResult, FIELD_COPYRIGHT, NULL, pInfo->sidCopyright);

	if (xs_cfg.subAutoEnable)
		aud_tuple_associate_int(pResult, FIELD_SUBSONG_NUM, NULL, pInfo->nsubTunes);

	aud_tuple_associate_int(pResult, -1, "subtunes", pInfo->nsubTunes);
	aud_tuple_associate_string(pResult, -1, "sid-format", pInfo->sidFormat);

	switch (pInfo->sidModel) {
		case XS_SIDMODEL_6581: tmpStr = "6581"; break;
		case XS_SIDMODEL_8580: tmpStr = "8580"; break;
		case XS_SIDMODEL_ANY: tmpStr = "ANY"; break;
		default: tmpStr = "?"; break;
	}
	aud_tuple_associate_string(pResult, -1, "sid-model", tmpStr);
	
	/* Get sub-tune information, if available */
	if (subTune < 0 || pInfo->startTune > pInfo->nsubTunes)
		subTune = pInfo->startTune;
	
	if ((subTune > 0) && (subTune <= pInfo->nsubTunes)) {
		gint tmpInt = pInfo->subTunes[subTune - 1].tuneLength;
		aud_tuple_associate_int(pResult, FIELD_LENGTH, NULL, (tmpInt < 0) ? -1 : tmpInt * 1000);
		
		tmpInt = pInfo->subTunes[subTune - 1].tuneSpeed;
		if (tmpInt > 0) {
			switch (tmpInt) {
			case XS_CLOCK_PAL: tmpStr = "PAL"; break;
			case XS_CLOCK_NTSC: tmpStr = "NTSC"; break;
			case XS_CLOCK_ANY: tmpStr = "ANY"; break;
			case XS_CLOCK_VBI: tmpStr = "VBI"; break;
			case XS_CLOCK_CIA: tmpStr = "CIA"; break;
			default:
				g_snprintf(tmpStr2, sizeof(tmpStr2), "%dHz", tmpInt);
				tmpStr = tmpStr2;
				break;
			}
		} else
			tmpStr = "?";

		aud_tuple_associate_string(pResult, -1, "sid-speed", tmpStr);
	} else
		subTune = 1;

	aud_tuple_associate_int(pResult, -1, "subtune", subTune);
	aud_tuple_associate_int(pResult, FIELD_SUBSONG_ID, NULL, subTune);
	aud_tuple_associate_int(pResult, FIELD_TRACK_NUMBER, NULL, subTune);

	if (xs_cfg.titleOverride)
		aud_tuple_associate_string(pResult, FIELD_FORMATTER, NULL, xs_cfg.titleFormat);
}


Tuple * xs_get_song_tuple(gchar *songFilename)
{
	Tuple *tmpResult;
	gchar *tmpFilename;
	t_xs_tuneinfo *tmpInfo;
	gint tmpTune;

	/* Get information from URL */
	xs_get_trackinfo(songFilename, &tmpFilename, &tmpTune);

	tmpResult = aud_tuple_new_from_filename(tmpFilename);
	if (!tmpResult) {
		g_free(tmpFilename);
		return NULL;
	}

	/* Get tune information from emulation engine */
	XS_MUTEX_LOCK(xs_status);
	tmpInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename);
	XS_MUTEX_UNLOCK(xs_status);
	g_free(tmpFilename);

	if (!tmpInfo)
		return tmpResult;
	
	xs_get_song_aud_tuple_info(tmpResult, tmpInfo, tmpTune);
	xs_tuneinfo_free(tmpInfo);

	return tmpResult;
}


Tuple *xs_probe_for_tuple(gchar *songFilename, t_xs_file *fd)
{
	Tuple *tmpResult;
	gchar *tmpFilename;
	t_xs_tuneinfo *tmpInfo;
	gint tmpTune;

	assert(xs_status.sidPlayer);

	if (songFilename == NULL)
		return NULL;

	XS_MUTEX_LOCK(xs_status);
	if (!xs_status.sidPlayer->plrProbe(fd)) {
		XS_MUTEX_UNLOCK(xs_status);
		return NULL;
	}
	XS_MUTEX_UNLOCK(xs_status);


	/* Get information from URL */
	xs_get_trackinfo(songFilename, &tmpFilename, &tmpTune);

	tmpResult = aud_tuple_new_from_filename(tmpFilename);
	if (!tmpResult) {
		g_free(tmpFilename);
		return NULL;
	}

	/* Get tune information from emulation engine */
	XS_MUTEX_LOCK(xs_status);
	tmpInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename);
	XS_MUTEX_UNLOCK(xs_status);
	g_free(tmpFilename);

	if (!tmpInfo)
		return tmpResult;
	
	xs_get_song_aud_tuple_info(tmpResult, tmpInfo, tmpTune);
	xs_tuneinfo_free(tmpInfo);

	return tmpResult;
}
