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
static	glong 	seek_time = -1;

static GThread *decode_thread;
static GMutex *decode_mutex;
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
    plugin_cleanup,
    NULL,
    NULL,
    NULL,
    NULL,
    get_song_info,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    file_mseek,
};

int
get_song_length (char *filename)
{
	SNDFILE	*tmp_sndfile;
	SF_INFO tmp_sfinfo;
	gchar *realfn = NULL;

	realfn = g_filename_from_uri(filename, NULL, NULL);
	tmp_sndfile = sf_open (realfn ? realfn : filename, SFM_READ, &tmp_sfinfo);
	g_free(realfn); realfn = NULL;

	if (!tmp_sndfile) {
		return 0;
	}

	sf_close (tmp_sndfile);
	tmp_sndfile = NULL;

	if (tmp_sfinfo.samplerate <= 0)
		return 0;

	return (int) ceil (1000.0 * tmp_sfinfo.frames / tmp_sfinfo.samplerate);
}

static gchar *get_title(char *filename)
{
	gchar *title;
	gchar *realfn = NULL;

	realfn = g_filename_from_uri(filename, NULL, NULL);
	title = g_path_get_basename(realfn ? realfn : filename);
	g_free(realfn); realfn = NULL;
	return title;
}

static void
plugin_init (void)
{
	seek_time = -1;

	decode_mutex = g_mutex_new();
	decode_cond = g_cond_new();
}

static void
plugin_cleanup (void)
{
	g_cond_free(decode_cond);
	g_mutex_free(decode_mutex);
}

static int
is_our_file (char *filename)
{
	SNDFILE	*tmp_sndfile;
	SF_INFO tmp_sfinfo;
	gchar *realfn = NULL; 

	realfn = g_filename_from_uri(filename, NULL, NULL);

	/* Have to open the file to see if libsndfile can handle it. */
	tmp_sndfile = sf_open (realfn ? realfn : filename, SFM_READ, &tmp_sfinfo);
	g_free(realfn); realfn = NULL;

	if (!tmp_sndfile) {
		return FALSE;
	}

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

	for (;;)
 	{
		GTimeVal sleeptime;

		/* sf_read_short will return 0 for all reads at EOF. */
		samples = sf_read_short (sndfile, buffer, BUFFER_SIZE);

		if (samples > 0 && playback->playing == TRUE) {
			while ((playback->output->buffer_free () < samples) &&
                   playback->playing == TRUE) {
                g_get_current_time(&sleeptime);
                g_time_val_add(&sleeptime, 500000);
                g_mutex_lock(decode_mutex);
				g_cond_timed_wait(decode_cond, decode_mutex, &sleeptime);
                g_mutex_unlock(decode_mutex);

				if (playback->playing == FALSE)
					break;	
			}

			produce_audio (playback->output->written_time (), FMT_S16_NE, sfinfo.channels, 
				samples * sizeof (short), buffer, &playback->playing);
		}
		else {
            while(playback->output->buffer_playing()) {
                g_get_current_time(&sleeptime);
                g_time_val_add(&sleeptime, 500000);
                g_mutex_lock(decode_mutex);
                g_cond_timed_wait(decode_cond, decode_mutex, &sleeptime);
                g_mutex_unlock(decode_mutex);

                if(playback->playing == FALSE)
                    break;
            }

			playback->eof = TRUE;
			playback->playing = FALSE;

			g_mutex_unlock(decode_mutex);
			break;	
		}

		/* Do seek if seek_time is valid. */
		if (seek_time >= 0) {
			sf_seek (sndfile, (sf_count_t)((gint64)seek_time * (gint64)sfinfo.samplerate / 1000L),
                     SEEK_SET);
			playback->output->flush (seek_time);
			seek_time = -1;
   		}

		if (playback->playing == FALSE)
			break;	
	}

	sf_close (sndfile);
	sndfile = NULL;
	seek_time = -1;

	playback->output->close_audio();

	return NULL;
}

static void
play_start (InputPlayback *playback)
{
	gchar *realfn = NULL;
	int pcmbitwidth;
	gchar *song_title;

	if (sndfile) /* already opened */
		return;

	pcmbitwidth = 32;
	song_title = get_title(playback->filename);

	realfn = g_filename_from_uri(playback->filename, NULL, NULL);
	sndfile = sf_open (realfn ? realfn : playback->filename, SFM_READ, &sfinfo);
	g_free(realfn); realfn = NULL;

	if (!sndfile)
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

	decode_thread = g_thread_self();
        play_loop(playback);
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
file_mseek (InputPlayback *playback, gulong millisecond)
{
	if (! sfinfo.seekable)
		return;

	seek_time = (glong)millisecond;

	while (seek_time != -1)
		xmms_usleep (80000);
}

static void
file_seek (InputPlayback *playback, int time)
{
    gulong millisecond = time * 1000;
    file_mseek(playback, millisecond);
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

void init(void)
{
        wav_ip.description = g_strdup_printf(_("sndfile WAV plugin"));
}

void fini(void)
{
	g_free(wav_ip.description);
}

InputPlugin *wav_iplist[] = { &wav_ip, NULL };

DECLARE_PLUGIN(wav-sndfile, init, fini, wav_iplist, NULL, NULL, NULL, NULL)
