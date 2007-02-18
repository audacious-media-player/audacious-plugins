/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005 Audacious development team.
 *
 *  Based on the xmms_sndfile input plugin:
 *  Copyright (C) 2000, 2002 Erik de Castro Lopo
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * Rewritten 17-Feb-2007 (nenolod):
 *   - now uses conditional variables to ensure that sndfile mutex is
 *     entirely protected.
 *   - pausing works now
 *   - fixed some potential race conditions when dealing with NFS.
 *   - TITLE_LEN removed
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/titlestring.h>
#include <audacious/i18n.h>
#include "audacious/output.h"
#include "wav-sndfile.h"

#include <sndfile.h>

static	SNDFILE *sndfile = NULL;
static	SF_INFO sfinfo;

static	int 	song_length;
static	int 	bit_rate = 0;
static	int 	seek_time = -1;

static GThread *decode_thread;
static GMutex *decode_start_mutex;
static GMutex *decode_mutex;
static GCond *decode_start_cond;
static GCond *decode_cond;

InputPlugin wav_ip = {
    NULL,
    NULL,
    NULL,
    plugin_init,
    wav_about,
    NULL,
    is_our_file,
    NULL,
    play_start,
    play_stop,
    play_pause,
    file_seek,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    get_song_info,
    NULL,
    NULL
};

int
get_song_length (char *filename)
{
	SNDFILE	*tmp_sndfile;
	SF_INFO tmp_sfinfo;

	if (! (tmp_sndfile = sf_open (filename, SFM_READ, &tmp_sfinfo)))
		return 0;

	sf_close (tmp_sndfile);
	tmp_sndfile = NULL;

	if (tmp_sfinfo.samplerate <= 0)
		return 0;

	return (int) ceil (1000.0 * tmp_sfinfo.frames / tmp_sfinfo.samplerate);
}

static gchar *get_title(char *filename)
{
	gchar *title;
	title = g_path_get_basename(filename);
	return title;
}

static void
plugin_init (void)
{
	seek_time = -1;

	decode_start_mutex = g_mutex_new();
	decode_mutex = g_mutex_new();

	decode_start_cond = g_cond_new();
	decode_cond = g_cond_new();
}

static int
is_our_file (char *filename)
{
	SNDFILE	*tmp_sndfile;
	SF_INFO tmp_sfinfo;

	/* Have to open the file to see if libsndfile can handle it. */
	if (! (tmp_sndfile = sf_open (filename, SFM_READ, &tmp_sfinfo)))
		return FALSE;

	/* It can so close file and return TRUE. */
	sf_close (tmp_sndfile);
	tmp_sndfile = NULL;

	return TRUE;
}

static gpointer
play_loop (gpointer arg)
{
	static short buffer [BUFFER_SIZE];
	int samples;
	InputPlayback *playback = arg;

	do
	{
		GTimeVal sleeptime;

		g_get_current_time(&sleeptime);
		sleeptime.tv_usec += 10000;
		if (sleeptime.tv_usec >= 1000000)
		{
			sleeptime.tv_sec += 1;
			sleeptime.tv_usec -= 1000000;
		}

		g_mutex_lock(decode_mutex);

		/* sf_read_short will return 0 for all reads at EOF. */
		samples = sf_read_short (sndfile, buffer, BUFFER_SIZE);

		if (samples > 0 && playback->playing == TRUE)
		{
			while ((playback->output->buffer_free () < 
				(samples * sizeof (short))) && 
				playback->playing == TRUE)
			{
				g_cond_timed_wait(decode_cond,
					decode_mutex, &sleeptime);

				if (playback->playing == FALSE)
				{
					g_mutex_unlock(decode_mutex);
					break;	
				}
			}

			produce_audio (playback->output->written_time (), FMT_S16_NE, sfinfo.channels, 
				samples * sizeof (short), buffer, &playback->playing);

			g_cond_signal(decode_start_cond);
		}
		else
		{
			g_cond_timed_wait(decode_cond,
				decode_mutex, &sleeptime);

			if (playback->playing == FALSE)
			{
				g_mutex_unlock(decode_mutex);
				break;	
			}
		}

		/* Do seek if seek_time is valid. */
		if (seek_time > 0)
		{	sf_seek (sndfile, seek_time * sfinfo.samplerate, SEEK_SET);
			playback->output->flush (seek_time * 1000);
			seek_time = -1;
   		}

		g_mutex_unlock(decode_mutex);
	} while (playback->playing == TRUE);

	sf_close (sndfile);
	sndfile = NULL;
	seek_time = -1;

	g_thread_exit (NULL);
	return NULL;
} /* play_loop */

static void
play_start (InputPlayback *playback)
{
        char *filename = playback->filename;
	int pcmbitwidth;
	gchar *song_title;

	if (sndfile)
		return;

	pcmbitwidth = 32;

	song_title = get_title(filename);

	if (! (sndfile = sf_open (filename, SFM_READ, &sfinfo)))
		return;

	bit_rate = sfinfo.samplerate * pcmbitwidth;

	if (sfinfo.samplerate > 0)
		song_length = (int) ceil (1000.0 * sfinfo.frames / sfinfo.samplerate);
	else
		song_length = 0;

	if (! playback->output->open_audio (FMT_S16_NE, sfinfo.samplerate, sfinfo.channels))
	{
		sf_close (sndfile);
		sndfile = NULL;
		return;
	}

	wav_ip.set_info (song_title, song_length, bit_rate, sfinfo.samplerate, sfinfo.channels);
	g_free (song_title);

	playback->playing = TRUE;

	decode_thread = g_thread_create ((GThreadFunc)play_loop, playback, TRUE, NULL);

	g_mutex_lock(decode_start_mutex);
	g_cond_wait(decode_start_cond, decode_start_mutex);
	g_mutex_unlock(decode_start_mutex);
}

static void
play_pause (InputPlayback *playback, gshort p)
{
	playback->output->pause(p);
}

static void
play_stop (InputPlayback *playback)
{
	if (decode_thread == NULL)
		return;

	g_mutex_lock(decode_mutex);
	playback->playing = FALSE;
	g_mutex_unlock(decode_mutex);
	g_cond_signal(decode_cond);

	g_thread_join (decode_thread);

	sndfile = NULL;
	decode_thread = NULL;
	seek_time = -1;
}

static void
file_seek (InputPlayback *playback, int time)
{
	if (! sfinfo.seekable)
		return;

	seek_time = time;

	while (seek_time != -1)
		xmms_usleep (80000);
}

static void
get_song_info (char *filename, char **title, int *length)
{
	(*length) = get_song_length(filename);
	(*title) = get_title(filename);
}

static void wav_about(void)
{
        static GtkWidget *box;
	if (!box)
	{
        	box = xmms_show_message(
			_("About sndfile WAV support"),
			_("Adapted for Audacious usage by Tony Vroon <chainsaw@gentoo.org>\n"
			  "from the xmms_sndfile plugin which is:\n"
			  "Copyright (C) 2000, 2002 Erik de Castro Lopo\n\n"
			  "This program is free software ; you can redistribute it and/or modify \n"
			  "it under the terms of the GNU General Public License as published by \n"
			  "the Free Software Foundation ; either version 2 of the License, or \n"
			  "(at your option) any later version. \n \n"
			  "This program is distributed in the hope that it will be useful, \n"
			  "but WITHOUT ANY WARRANTY ; without even the implied warranty of \n"
			  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  \n"
			  "See the GNU General Public License for more details. \n\n"
			  "You should have received a copy of the GNU General Public \n"
			  "License along with this program ; if not, write to \n"
			  "the Free Software Foundation, Inc., \n"
			  "51 Franklin Street, Fifth Floor, \n"
			  "Boston, MA  02110-1301  USA"),
			_("Ok"), FALSE, NULL, NULL);
		g_signal_connect(G_OBJECT(box), "destroy",
			(GCallback)gtk_widget_destroyed, &box);
	}
}


InputPlugin *get_iplugin_info(void)
{
        wav_ip.description = g_strdup_printf(_("sndfile WAV plugin"));
        return &wav_ip;
}

