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
#include "xs_title.h"
#include "xs_filter.h"
#include "xs_fileinfo.h"
#include "xs_interface.h"
#include "xs_glade.h"
#include "xs_player.h"

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

static GtkWidget *xs_subctrl = NULL;
static GtkObject *xs_subctrl_adj = NULL;
XS_MUTEX(xs_subctrl);

void		xs_subctrl_close(void);
void		xs_subctrl_update(void);

static t_xs_sldb *xs_sldb_db = NULL;
XS_MUTEX(xs_sldb_db);

gint		xs_songlen_init(void);
void		xs_songlen_close(void);
t_xs_sldb_node *xs_songlen_get(const gchar *);


/*
 * Error messages
 */
void xs_error(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "XMMS-SID: ");
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
	t_xs_file *f;
	assert(xs_status.sidPlayer);

	/* Check the filename */
	if (pcFilename == NULL)
		return FALSE;

	if ((f = xs_fopen(pcFilename, "rb")) != NULL) {
		if (xs_status.sidPlayer->plrProbe(f))
			return TRUE;
		xs_fclose(f);
	}

	return FALSE;
}


gint xs_is_our_file_vfs(gchar *pcFilename, t_xs_file *f)
{
	assert(xs_status.sidPlayer);

	/* Check the filename */
	if (pcFilename == NULL)
		return FALSE;

	return xs_status.sidPlayer->plrProbe(f);
}


/*
 * Main playing thread loop
 */
void *xs_playthread(void *argPointer)
{
	InputPlayback *pb = argPointer;
	t_xs_status myStatus;
	t_xs_tuneinfo *myTune;
	gboolean audioOpen = FALSE, doPlay = FALSE, isFound = FALSE;
	gint audioGot, songLength, i;
	gchar *audioBuffer = NULL, *oversampleBuffer = NULL, *tmpTitle;

	(void) argPointer;

	/* Initialize */
	XSDEBUG("entering player thread\n");
	XS_MUTEX_LOCK(xs_status);
	memcpy(&myStatus, &xs_status, sizeof(t_xs_status));
	myTune = xs_status.tuneInfo;
	for (i = 0; i <= myTune->nsubTunes; i++)
		myTune->subTunes[i].tunePlayed = FALSE;
	XS_MUTEX_UNLOCK(xs_status);

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

	/*
	 * Main player loop: while not stopped, loop here - play subtunes
	 */
	audioOpen = FALSE;
	doPlay = TRUE;
	while (xs_status.isPlaying && doPlay) {
		/* Automatic sub-tune change logic */
		XS_MUTEX_LOCK(xs_cfg);
		XS_MUTEX_LOCK(xs_status);
		myStatus.isPlaying = TRUE;
		
		if (xs_status.currSong < 1 || myStatus.currSong < 1) {
			XS_MUTEX_UNLOCK(xs_status);
			XS_MUTEX_UNLOCK(xs_cfg);
			goto xs_err_exit;
		}
		
		if (xs_cfg.subAutoEnable && (myStatus.currSong == xs_status.currSong)) {
			/* Check if currently selected sub-tune has been played already */
			if (myTune->subTunes[myStatus.currSong-1].tunePlayed) {
				/* Find a tune that has not been played */
				XSDEBUG("tune #%i already played, finding next match ...\n", myStatus.currSong);
				isFound = FALSE;
				i = 0;
				while (!isFound && (++i <= myTune->nsubTunes)) {
					if (xs_cfg.subAutoMinOnly) {
						/* A tune with minimum length must be found */
						if (!myTune->subTunes[i-1].tunePlayed &&
							myTune->subTunes[i-1].tuneLength >= xs_cfg.subAutoMinTime)
							isFound = TRUE;
					} else {
						/* Any unplayed tune is okay */
						if (!myTune->subTunes[i-1].tunePlayed)
							isFound = TRUE;
					}
				}

				if (isFound) {
					/* Set the new sub-tune */
					XSDEBUG("found #%i\n", i);
					xs_status.currSong = i;
				} else
					/* This is the end */
					doPlay = FALSE;

				XS_MUTEX_UNLOCK(xs_status);
				XS_MUTEX_UNLOCK(xs_cfg);
				continue;	/* This is ugly, but ... */
			}
		}

		/* Tell that we are initializing, update sub-tune controls */
		myStatus.currSong = xs_status.currSong;
		myTune->subTunes[myStatus.currSong-1].tunePlayed = TRUE;
		XS_MUTEX_UNLOCK(xs_status);
		XS_MUTEX_UNLOCK(xs_cfg);

		XSDEBUG("subtune #%i selected, initializing...\n", myStatus.currSong);

		GDK_THREADS_ENTER();
		xs_subctrl_update();
		GDK_THREADS_LEAVE();

		/* Check minimum playtime */
		songLength = myTune->subTunes[myStatus.currSong-1].tuneLength;
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
		
		if (!pb->output->
			open_audio(myStatus.audioFormat, myStatus.audioFrequency, myStatus.audioChannels)) {
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
		XSDEBUG("set tune info\n");
		myStatus.sidPlayer->plrUpdateSIDInfo(&myStatus);
		tmpTitle = xs_make_titlestring(myTune, myStatus.currSong);
		
		xs_plugin_ip.set_info(
			tmpTitle,
			(songLength > 0) ? (songLength * 1000) : 0,
			-1,
			myStatus.audioFrequency,
			myStatus.audioChannels);
		
		g_free(tmpTitle);
		
		XSDEBUG("playing\n");

		/*
		 * Play the subtune
		 */
		while (xs_status.isPlaying && myStatus.isPlaying && (xs_status.currSong == myStatus.currSong)) {
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
			while (xs_status.isPlaying &&
				(xs_status.currSong == myStatus.currSong) &&
				(pb->output->buffer_free() < audioGot))
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

		XSDEBUG("subtune ended/stopped\n");

		/* Close audio output plugin */
		if (audioOpen) {
			XSDEBUG("close audio #1\n");
			pb->output->close_audio();
			audioOpen = FALSE;
			XSDEBUG("closed\n");
		}

		/* Now determine if we continue by selecting other subtune or something */
		if (!myStatus.isPlaying && !xs_cfg.subAutoEnable)
			doPlay = FALSE;
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
	XS_THREAD_EXIT(NULL);
}


/*
 * Start playing the given file
 * Here we load the tune and initialize the playing thread.
 * Usually you would also initialize the output-plugin, but
 * this is XMMS-SID and we do it on the player thread instead.
 */
void xs_play_file(InputPlayback *pb)
{
	assert(pb);
	assert(xs_status.sidPlayer);
	
	XSDEBUG("play '%s'\n", pb->filename);

	/* Get tune information */
	if ((xs_status.tuneInfo = xs_status.sidPlayer->plrGetSIDInfo(pb->filename)) == NULL)
		return;

	/* Initialize the tune */
	if (!xs_status.sidPlayer->plrLoadSID(&xs_status, pb->filename)) {
		xs_tuneinfo_free(xs_status.tuneInfo);
		xs_status.tuneInfo = NULL;
		return;
	}

	XSDEBUG("load ok\n");

	/* Set general status information */
	xs_status.isPlaying = TRUE;
	xs_status.isError = FALSE;
	xs_status.currSong = xs_status.tuneInfo->startTune;

	/* Start the playing thread! */
	xs_decode_thread = g_thread_create((GThreadFunc) xs_playthread, pb, TRUE, NULL);
	if (xs_decode_thread == NULL) {
		xs_error(_("Couldn't create playing thread!\n"));
		xs_tuneinfo_free(xs_status.tuneInfo);
		xs_status.tuneInfo = NULL;
		xs_status.sidPlayer->plrDeleteSID(&xs_status);
	}

	/* Okay, here the playing thread has started up and we
	 * return from here to XMMS. Rest is up to XMMS's GUI
	 * and playing thread.
	 */
	XSDEBUG("systems should be up?\n");
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

	/* Close the sub-tune control window, if any */
	xs_subctrl_close();

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
	
	/* Status is now stopped, update the sub-tune
	 * controller in fileinfo window (if open)
	 */
	xs_fileinfo_update();

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
	xs_subctrl_close();
	xs_fileinfo_update();
	pb->output->pause(pauseState);
}


/*
 * Pop-up subtune selector
 */
void xs_subctrl_setsong(void)
{
	gint n;

	XS_MUTEX_LOCK(xs_status);
	XS_MUTEX_LOCK(xs_subctrl);

	if (xs_status.tuneInfo && xs_status.isPlaying) {
		n = (gint) GTK_ADJUSTMENT(xs_subctrl_adj)->value;
		if ((n >= 1) && (n <= xs_status.tuneInfo->nsubTunes))
			xs_status.currSong = n;
	}

	XS_MUTEX_UNLOCK(xs_subctrl);
	XS_MUTEX_UNLOCK(xs_status);
}


void xs_subctrl_prevsong(void)
{
	XS_MUTEX_LOCK(xs_status);

	if (xs_status.tuneInfo && xs_status.isPlaying) {
		if (xs_status.currSong > 1)
			xs_status.currSong--;
	}

	XS_MUTEX_UNLOCK(xs_status);

	xs_subctrl_update();
}


void xs_subctrl_nextsong(void)
{
	XS_MUTEX_LOCK(xs_status);

	if (xs_status.tuneInfo && xs_status.isPlaying) {
		if (xs_status.currSong < xs_status.tuneInfo->nsubTunes)
			xs_status.currSong++;
	}

	XS_MUTEX_UNLOCK(xs_status);

	xs_subctrl_update();
}


void xs_subctrl_update(void)
{
	GtkAdjustment *tmpAdj;

	XS_MUTEX_LOCK(xs_status);
	XS_MUTEX_LOCK(xs_subctrl);

	/* Check if control window exists, we are currently playing and have a tune */
	if (xs_subctrl) {
		if (xs_status.tuneInfo && xs_status.isPlaying) {
			tmpAdj = GTK_ADJUSTMENT(xs_subctrl_adj);

			tmpAdj->value = xs_status.currSong;
			tmpAdj->lower = 1;
			tmpAdj->upper = xs_status.tuneInfo->nsubTunes;
			XS_MUTEX_UNLOCK(xs_status);
			XS_MUTEX_UNLOCK(xs_subctrl);
			gtk_adjustment_value_changed(tmpAdj);
		} else {
			XS_MUTEX_UNLOCK(xs_status);
			XS_MUTEX_UNLOCK(xs_subctrl);
			xs_subctrl_close();
		}
	} else {
		XS_MUTEX_UNLOCK(xs_subctrl);
		XS_MUTEX_UNLOCK(xs_status);
	}

	xs_fileinfo_update();
}


void xs_subctrl_close(void)
{
	XS_MUTEX_LOCK(xs_subctrl);

	if (xs_subctrl) {
		gtk_widget_destroy(xs_subctrl);
		xs_subctrl = NULL;
	}

	XS_MUTEX_UNLOCK(xs_subctrl);
}


gboolean xs_subctrl_keypress(GtkWidget * win, GdkEventKey * ev)
{
	(void) win;

	if (ev->keyval == GDK_Escape)
		xs_subctrl_close();

	return FALSE;
}


void xs_subctrl_open(void)
{
	GtkWidget *frame25, *hbox15, *subctrl_prev, *subctrl_current, *subctrl_next;

	XS_MUTEX_LOCK(xs_subctrl);
	if (!xs_status.tuneInfo || !xs_status.isPlaying ||
		xs_subctrl || (xs_status.tuneInfo->nsubTunes <= 1)) {
		XS_MUTEX_UNLOCK(xs_subctrl);
		return;
	}

	/* Create the pop-up window */
	xs_subctrl = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(xs_subctrl), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_widget_set_name(xs_subctrl, "xs_subctrl");
	g_object_set_data(G_OBJECT(xs_subctrl), "xs_subctrl", xs_subctrl);

	gtk_window_set_title(GTK_WINDOW(xs_subctrl), _("Subtune Control"));
	gtk_window_set_position(GTK_WINDOW(xs_subctrl), GTK_WIN_POS_MOUSE);
	gtk_container_set_border_width(GTK_CONTAINER(xs_subctrl), 0);
	gtk_window_set_policy(GTK_WINDOW(xs_subctrl), FALSE, FALSE, FALSE);

	g_signal_connect(G_OBJECT(xs_subctrl), "destroy", G_CALLBACK(gtk_widget_destroyed), &xs_subctrl);

	g_signal_connect(G_OBJECT(xs_subctrl), "focus_out_event", G_CALLBACK(xs_subctrl_close), NULL);

	gtk_widget_realize(xs_subctrl);
	gdk_window_set_decorations(xs_subctrl->window, (GdkWMDecoration) 0);


	/* Create the control widgets */
	frame25 = gtk_frame_new(NULL);
	gtk_container_add(GTK_CONTAINER(xs_subctrl), frame25);
	gtk_container_set_border_width(GTK_CONTAINER(frame25), 2);
	gtk_frame_set_shadow_type(GTK_FRAME(frame25), GTK_SHADOW_OUT);

	hbox15 = gtk_hbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(frame25), hbox15);

	subctrl_prev = gtk_button_new_with_label(" < ");
	gtk_widget_set_name(subctrl_prev, "subctrl_prev");
	gtk_box_pack_start(GTK_BOX(hbox15), subctrl_prev, FALSE, FALSE, 0);

	xs_subctrl_adj = gtk_adjustment_new(xs_status.currSong, 1, xs_status.tuneInfo->nsubTunes, 1, 1, 0);
	g_signal_connect(G_OBJECT(xs_subctrl_adj), "value_changed", G_CALLBACK(xs_subctrl_setsong), NULL);

	subctrl_current = gtk_hscale_new(GTK_ADJUSTMENT(xs_subctrl_adj));
	gtk_widget_set_size_request(subctrl_current, 80, -1);
	gtk_widget_set_name(subctrl_current, "subctrl_current");
	gtk_box_pack_start(GTK_BOX(hbox15), subctrl_current, FALSE, TRUE, 0);
	gtk_scale_set_digits(GTK_SCALE(subctrl_current), 0);
	gtk_range_set_update_policy(GTK_RANGE(subctrl_current), GTK_UPDATE_DELAYED);
	gtk_widget_grab_focus(subctrl_current);

	subctrl_next = gtk_button_new_with_label(" > ");
	gtk_widget_set_name(subctrl_next, "subctrl_next");
	gtk_box_pack_start(GTK_BOX(hbox15), subctrl_next, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(subctrl_prev), "clicked", G_CALLBACK(xs_subctrl_prevsong), NULL);

	g_signal_connect(G_OBJECT(subctrl_next), "clicked", G_CALLBACK(xs_subctrl_nextsong), NULL);

	g_signal_connect(G_OBJECT(xs_subctrl), "key_press_event", G_CALLBACK(xs_subctrl_keypress), NULL);

	gtk_widget_show_all(xs_subctrl);

	XS_MUTEX_UNLOCK(xs_subctrl);
}


/*
 * Set the time-seek position
 * The playing thread will do the "seeking", which means sub-tune
 * changing in XMMS-SID's case. iTime argument is time in seconds,
 * in contrast to milliseconds used in other occasions.
 *
 * This function is called whenever position slider is clicked or
 * other method of seeking is used (keyboard, etc.)
 */
void xs_seek(InputPlayback *pb, gint iTime)
{
	/* Check status */
	XS_MUTEX_LOCK(xs_status);
	if (!xs_status.tuneInfo || !xs_status.isPlaying) {
		XS_MUTEX_UNLOCK(xs_status);
		return;
	}

	/* Act according to settings */
	switch (xs_cfg.subsongControl) {
	case XS_SSC_SEEK:
		if (iTime < xs_status.lastTime) {
			if (xs_status.currSong > 1)
				xs_status.currSong--;
		} else if (iTime > xs_status.lastTime) {
			if (xs_status.currSong < xs_status.tuneInfo->nsubTunes)
				xs_status.currSong++;
		}
		break;

	case XS_SSC_POPUP:
		xs_subctrl_open();
		break;

		/* If we have song-position patch, check settings */
#ifdef HAVE_SONG_POSITION
	case XS_SSC_PATCH:
		if ((iTime > 0) && (iTime <= xs_status.tuneInfo->nsubTunes))
			xs_status.currSong = iTime;
		break;
#endif
	}

	XS_MUTEX_UNLOCK(xs_status);
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

	/* Let's see what we do */
	switch (xs_cfg.subsongControl) {
	case XS_SSC_SEEK:
		xs_status.lastTime = (pb->output->output_time() / 1000);
		break;

#ifdef HAVE_SONG_POSITION
	case XS_SSC_PATCH:
		set_song_position(xs_status.currSong, 1, xs_status.tuneInfo->nsubTunes);
		break;
#endif
	}

	XS_MUTEX_UNLOCK(xs_status);

	/* Return output time reported by audio output plugin */
	return pb->output->output_time();
}


/* Return song information: called by XMMS when initially loading the playlist.
 * Subsequent changes to information are made by the player thread,
 * which uses xs_plugin_ip.set_info();
 */
void xs_get_song_info(gchar * songFilename, gchar ** songTitle, gint * songLength)
{
	t_xs_tuneinfo *pInfo;

	/* Get tune information from emulation engine */
	pInfo = xs_status.sidPlayer->plrGetSIDInfo(songFilename);
	if (!pInfo)
		return;

	/* Get sub-tune information, if available */
	if ((pInfo->startTune > 0) && (pInfo->startTune <= pInfo->nsubTunes)) {
		gint tmpInt;
		
		(*songTitle) = xs_make_titlestring(pInfo, pInfo->startTune);

		tmpInt = pInfo->subTunes[pInfo->startTune-1].tuneLength;
		if (tmpInt < 0)
			(*songLength) = -1;
		else
			(*songLength) = (tmpInt * 1000);
	}

	/* Free tune information */
	xs_tuneinfo_free(pInfo);
}


TitleInput * xs_get_song_tuple(gchar *songFilename)
{
	t_xs_tuneinfo *pInfo;
	TitleInput *pResult = NULL;

	/* Get tune information from emulation engine */
	pInfo = xs_status.sidPlayer->plrGetSIDInfo(songFilename);
	if (!pInfo)
		return NULL;

	/* Get sub-tune information, if available */
	if ((pInfo->startTune > 0) && (pInfo->startTune <= pInfo->nsubTunes)) {
		gint tmpInt;
		
		pResult = xs_make_titletuple(pInfo, pInfo->startTune);

		tmpInt = pInfo->subTunes[pInfo->startTune-1].tuneLength;
		if (tmpInt < 0)
			pResult->length = -1;
		else
			pResult->length = (tmpInt * 1000);
	}

	/* Free tune information */
	xs_tuneinfo_free(pInfo);

	return pResult;
}


/* Allocate a new tune information structure
 */
t_xs_tuneinfo *xs_tuneinfo_new(const gchar * pcFilename,
		gint nsubTunes, gint startTune, const gchar * sidName,
		const gchar * sidComposer, const gchar * sidCopyright,
		gint loadAddr, gint initAddr, gint playAddr,
		gint dataFileLen, const gchar *sidFormat, gint sidModel)
{
	t_xs_tuneinfo *pResult;
	t_xs_sldb_node *tmpLength;
	gint i;

	/* Allocate structure */
	pResult = (t_xs_tuneinfo *) g_malloc0(sizeof(t_xs_tuneinfo));
	if (!pResult) {
		xs_error(_("Could not allocate memory for t_xs_tuneinfo ('%s')\n"),
			pcFilename);
		return NULL;
	}

	pResult->sidFilename = g_filename_to_utf8(pcFilename, -1, NULL, NULL, NULL);
	if (!pResult->sidFilename) {
		xs_error(_("Could not allocate sidFilename ('%s')\n"),
			pcFilename);
		g_free(pResult);
		return NULL;
	}

	/* Allocate space for subtune information */
	pResult->subTunes = g_malloc0(sizeof(t_xs_subtuneinfo) * (nsubTunes + 1));
	if (!pResult->subTunes) {
		xs_error(_("Could not allocate memory for t_xs_subtuneinfo ('%s', %i)\n"),
			pcFilename, nsubTunes);

		g_free(pResult->sidFilename);
		g_free(pResult);
		return NULL;
	}

	/* The following allocations don't matter if they fail */
	pResult->sidName = XS_CS_SID(sidName);
	pResult->sidComposer = XS_CS_SID(sidComposer);
	pResult->sidCopyright = XS_CS_SID(sidCopyright);

	pResult->nsubTunes = nsubTunes;
	pResult->startTune = startTune;

	pResult->loadAddr = loadAddr;
	pResult->initAddr = initAddr;
	pResult->playAddr = playAddr;
	pResult->dataFileLen = dataFileLen;
	pResult->sidFormat = XS_CS_SID(sidFormat);
	
	pResult->sidModel = sidModel;

	/* Get length information (NOTE: Do not free this!) */
	tmpLength = xs_songlen_get(pcFilename);
	
	/* Fill in sub-tune information */
	for (i = 0; i < pResult->nsubTunes; i++) {
		if (tmpLength && (i < tmpLength->nLengths))
			pResult->subTunes[i].tuneLength = tmpLength->sLengths[i];
		else
			pResult->subTunes[i].tuneLength = -1;
		
		pResult->subTunes[i].tuneSpeed = -1;
	}
	
	return pResult;
}


/* Free given tune information structure
 */
void xs_tuneinfo_free(t_xs_tuneinfo * pTune)
{
	if (!pTune) return;

	g_free(pTune->subTunes);
	g_free(pTune->sidFilename);
	g_free(pTune->sidName);
	g_free(pTune->sidComposer);
	g_free(pTune->sidCopyright);
	g_free(pTune->sidFormat);
	g_free(pTune);
}


/* Song length database handling glue
 */
gint xs_songlen_init(void)
{
	XS_MUTEX_LOCK(xs_cfg);

	if (!xs_cfg.songlenDBPath) {
		XS_MUTEX_UNLOCK(xs_cfg);
		return -1;
	}

	XS_MUTEX_LOCK(xs_sldb_db);

	/* Check if already initialized */
	if (xs_sldb_db)
		xs_sldb_free(xs_sldb_db);

	/* Allocate database */
	xs_sldb_db = (t_xs_sldb *) g_malloc0(sizeof(t_xs_sldb));
	if (!xs_sldb_db) {
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_sldb_db);
		return -2;
	}

	/* Read the database */
	if (xs_sldb_read(xs_sldb_db, xs_cfg.songlenDBPath) != 0) {
		xs_sldb_free(xs_sldb_db);
		xs_sldb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_sldb_db);
		return -3;
	}

	/* Create index */
	if (xs_sldb_index(xs_sldb_db) != 0) {
		xs_sldb_free(xs_sldb_db);
		xs_sldb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_sldb_db);
		return -4;
	}

	XS_MUTEX_UNLOCK(xs_cfg);
	XS_MUTEX_UNLOCK(xs_sldb_db);
	return 0;
}


void xs_songlen_close(void)
{
	XS_MUTEX_LOCK(xs_sldb_db);
	xs_sldb_free(xs_sldb_db);
	xs_sldb_db = NULL;
	XS_MUTEX_UNLOCK(xs_sldb_db);
}


t_xs_sldb_node *xs_songlen_get(const gchar * pcFilename)
{
	t_xs_sldb_node *pResult;

	XS_MUTEX_LOCK(xs_sldb_db);

	if (xs_cfg.songlenDBEnable && xs_sldb_db)
		pResult = xs_sldb_get(xs_sldb_db, pcFilename);
	else
		pResult = NULL;

	XS_MUTEX_UNLOCK(xs_sldb_db);

	return pResult;
}
