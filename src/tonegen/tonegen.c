/*
 *  Copyright 2000,2001 Haavard Kvaalen <havardk@sol.no>
 *
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

#include "config.h"
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define MIN_FREQ 10
#define MAX_FREQ 20000
#define OUTPUT_FREQ 44100

#ifndef PI
#define PI 3.14159265358979323846
#endif

static InputPlugin tone_ip;

static gboolean going;
static gboolean audio_error;
static GThread *play_thread;

static void tone_init(void)
{
	aud_uri_set_plugin("tone://", &tone_ip);
}

static void tone_about(void)
{
	static GtkWidget *box;
	if (!box)
	{
		box = audacious_info_dialog(
			_("About Tone Generator"),
	/* I18N: UTF-8 Translation: "Haavard Kvaalen" -> "H\303\245vard Kv\303\245len" */
			_("Sinus tone generator by Haavard Kvaalen <havardk@xmms.org>\n"
			  "Modified by Daniel J. Peng <danielpeng@bigfoot.com>\n\n"
		  	"To use it, add a URL: tone://frequency1;frequency2;frequency3;...\n"
		  	"e.g. tone://2000;2005 to play a 2000Hz tone and a 2005Hz tone"),
			_("Ok"), FALSE, NULL, NULL);
		g_signal_connect(GTK_OBJECT(box), "destroy",
				   (GCallback)gtk_widget_destroyed, &box);
	}
}

static int tone_is_our_file(char *filename)
{
	if (!strncmp(filename, "tone://", 7))
		return TRUE;
	return FALSE;
}

#define BUF_SAMPLES 512
#define BUF_BYTES BUF_SAMPLES * sizeof(float)

static void* play_loop(void *arg)
{
        InputPlayback *playback = arg;
	GArray* frequencies = playback->data;
	float data[BUF_SAMPLES];
	gsize i;
	struct {
		double wd;
		unsigned int period, t;
	} *tone;

	tone = g_malloc(frequencies->len * sizeof(*tone));

	for (i = 0; i < frequencies->len; i++)
	{
		double f = g_array_index(frequencies, double, i);
		tone[i].wd = 2 * PI * f / OUTPUT_FREQ;
		tone[i].period = (G_MAXINT * 2U / OUTPUT_FREQ) *
							(OUTPUT_FREQ / f);
		tone[i].t = 0;
	}

	while (going)
	{
		for (i = 0; i < BUF_SAMPLES; i++)
		{
			gsize j;
			double sum_sines;

			for (sum_sines = 0, j = 0; j < frequencies->len; j++)
			{
				sum_sines += sin(tone[j].wd * tone[j].t);
				if (tone[j].t > tone[j].period)
					tone[j].t -= tone[j].period;
				tone[j].t++;
			}
			data[i] = (sum_sines * 0.999 / (double)frequencies->len); /* dithering can cause a little bit of clipping */
		}
		playback->pass_audio(playback, FMT_FLOAT, 1, BUF_BYTES, data, &going);
	}

	g_array_free(frequencies, TRUE);
	g_free(tone);

	/* Make sure the output plugin stops prebuffering */
	playback->output->buffer_free();
	playback->output->buffer_free();

	return(NULL);
}

static GArray* tone_filename_parse(const char* filename)
{
	GArray *frequencies = g_array_new(FALSE, FALSE, sizeof(double));
	char **strings, **ptr;

	if (strncmp(filename,"tone://", 7))
		return NULL;

	filename += 7;
	strings = g_strsplit(filename, ";", 100);

	for (ptr = strings; *ptr != NULL; ptr++)
	{
		double freq = strtod(*ptr, NULL);
		if (freq >= MIN_FREQ && freq <= MAX_FREQ)
			g_array_append_val(frequencies, freq);
	}
	g_strfreev(strings);

	if (frequencies->len == 0)
	{
		g_array_free(frequencies, TRUE);
		frequencies = NULL;
	}

	return frequencies;
}

static char* tone_title(char *filename)
{
	GArray *freqs;
	char* title;
	gsize i;

	freqs = tone_filename_parse(filename);
	if (freqs == NULL)
		return NULL;

	title = g_strdup_printf(_("%s %.1f Hz"), _("Tone Generator: "),
				g_array_index(freqs, double, 0));
	for (i = 1; i < freqs->len; i++)
	{
		char *old_title;
		old_title = title;
		title = g_strdup_printf("%s;%.1f Hz", old_title,
					g_array_index(freqs, double, i));
		g_free(old_title);
	}
	g_array_free(freqs, TRUE);

	return title;
}


static void tone_play(InputPlayback *playback)
{
        char *filename = playback->filename;
	GArray* frequencies;
	char *name;

	frequencies = tone_filename_parse(filename);
	if (frequencies == NULL)
		return;

 	going = TRUE;
	audio_error = FALSE;
	if (playback->output->open_audio(FMT_FLOAT, OUTPUT_FREQ, 1) == 0)
	{
		audio_error = TRUE;
		going = FALSE;
		return;
	}

	name = tone_title(filename);
	playback->set_params(playback, name, -1, 16 * OUTPUT_FREQ, OUTPUT_FREQ, 1);
	g_free(name);
	playback->data = frequencies;
	play_thread = g_thread_self();
	playback->set_pb_ready(playback);
	play_loop(playback);
}

static void tone_stop(InputPlayback *playback)
{
	if (going)
	{
		going = FALSE;
		g_thread_join(play_thread);
		playback->output->close_audio();
	}
}

static void tone_pause(InputPlayback *playback, short paused)
{
	playback->output->pause(paused);
}

static int tone_get_time(InputPlayback *playback)
{
	if (audio_error)
		return -2;
	if (!going && !playback->output->buffer_playing())
		return -1;
	return playback->output->output_time();
}

static void tone_song_info(char *filename, char **title, int *length)
{
	*length = -1;
	*title = tone_title(filename);
}

static InputPlugin tone_ip =
{
	.description = "Tone Generator",
	.init = tone_init,
	.about = tone_about,
	.is_our_file = tone_is_our_file,
	.play_file = tone_play,
	.stop = tone_stop,
	.pause = tone_pause,
	.get_time = tone_get_time,
	.get_song_info = tone_song_info,
};

InputPlugin *tonegen_iplist[] = { &tone_ip, NULL };

DECLARE_PLUGIN(tonegen, NULL, NULL, tonegen_iplist, NULL, NULL, NULL, NULL, NULL);
