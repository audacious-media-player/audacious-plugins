/*  XMMS - ALSA output plugin
 *  Copyright (C) 2001-2003 Matthieu Sozeau <mattam@altern.org>
 *  Copyright (C) 1998-2003  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2006  Haavard Kvaalen
 *  Copyright (C) 2005       Takashi Iwai
 *  Copyright (C) 2007-2008  William Pitcock
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
 *  CHANGES
 *
 *  2005.01.05  Takashi Iwai <tiwai@suse.de>
 *	Impelemented the multi-threaded mode with an audio-thread.
 *	Many fixes and cleanups.
 */

/*#define AUD_DEBUG*/

#include "alsa.h"
#include <ctype.h>
#include <math.h>

static snd_pcm_t *alsa_pcm;
static snd_output_t *logs;


/* Number of bytes that we have received from the input plugin */
static guint64 alsa_total_written;
/* Number of bytes that we have sent to the sound card */
static guint64 alsa_hw_written;
static guint64 output_time_offset;

/* device buffer/period sizes in bytes */
static gint hw_buffer_size, hw_period_size;		/* in output bytes */
static gint hw_buffer_size_in, hw_period_size_in;	/* in input bytes */

/* Set/Get volume */
static snd_mixer_elem_t *pcm_element;
static snd_mixer_t *mixer;

static gboolean going, paused, mixer_start = TRUE;
static gboolean prebuffer, remove_prebuffer;

static gboolean alsa_can_pause;

/* for audio thread */
static GThread *audio_thread;	 /* audio loop thread */
static gint thread_buffer_size;	 /* size of intermediate buffer in bytes */
static gchar *thread_buffer;	 /* audio intermediate buffer */
static gint rd_index, wr_index;	 /* current read/write position in int-buffer */
static gint buffer_empty;	 /* is the buffer empty? */
static gboolean pause_request;	 /* pause status currently requested */
static gint flush_request;	 /* flush status (time) currently requested */
static gint prebuffer_size;
GStaticMutex alsa_mutex = G_STATIC_MUTEX_INIT;

struct snd_format {
	guint rate;
	guint channels;
	snd_pcm_format_t format;
	AFormat xmms_format;
	gint sample_bits;
	gint bps;
};

static struct snd_format *inputf = NULL;
static struct snd_format *outputf = NULL;

static gint alsa_setup(struct snd_format *f);
static void alsa_write_audio(gchar *data, gint length);
static void alsa_cleanup_mixer(void);
static gint get_thread_buffer_filled(void);

static struct snd_format * snd_format_from_xmms(AFormat fmt, gint rate, gint channels);

static const struct {
	AFormat xmms;
	snd_pcm_format_t alsa;
} format_table[] =
{
#if 0
/* i don't know if we will ever do this --nenolod */
 {FMT_S32_LE, SND_PCM_FORMAT_S32_LE},
 {FMT_S32_BE, SND_PCM_FORMAT_S32_BE},
 {FMT_S32_NE, SND_PCM_FORMAT_S32},
#endif
 {FMT_S24_LE, SND_PCM_FORMAT_S24_LE},
 {FMT_S24_BE, SND_PCM_FORMAT_S24_BE},
 {FMT_S24_NE, SND_PCM_FORMAT_S24},
 {FMT_U24_LE, SND_PCM_FORMAT_U24_LE},
 {FMT_U24_BE, SND_PCM_FORMAT_U24_BE},
 {FMT_U24_NE, SND_PCM_FORMAT_U24},
 {FMT_S16_LE, SND_PCM_FORMAT_S16_LE},
 {FMT_S16_BE, SND_PCM_FORMAT_S16_BE},
 {FMT_S16_NE, SND_PCM_FORMAT_S16},
 {FMT_U16_LE, SND_PCM_FORMAT_U16_LE},
 {FMT_U16_BE, SND_PCM_FORMAT_U16_BE},
 {FMT_U16_NE, SND_PCM_FORMAT_U16},
 {FMT_U8, SND_PCM_FORMAT_U8},
 {FMT_S8, SND_PCM_FORMAT_S8},
};


static void debug(gchar *str, ...) G_GNUC_PRINTF(1, 2);

static void debug(gchar *str, ...)
{
	va_list args;

	if (alsa_cfg.debug)
	{
		va_start(args, str);
		g_logv(NULL, G_LOG_LEVEL_MESSAGE, str, args);
		va_end(args);
	}
}

int alsa_hardware_present(void)
{
	gint card = -1;

        if ((snd_card_next(&card)) != 0)
                return 0;

        return 1;
}

int alsa_playing(void)
{
	gint ret;

	g_static_mutex_lock(&alsa_mutex);

	if (!going || paused || prebuffer || alsa_pcm == NULL)
		ret = FALSE;
	else 
		ret = snd_pcm_state(alsa_pcm) == SND_PCM_STATE_RUNNING ||
		      get_thread_buffer_filled();

	g_static_mutex_unlock(&alsa_mutex);

	return ret;
}

/* update and get the available space on h/w buffer */
static gint alsa_get_avail(void)
{
	snd_pcm_sframes_t ret;

	if (alsa_pcm == NULL)
		return 0;

	while ((ret = snd_pcm_avail_update(alsa_pcm)) < 0)
	{
		debug("snd_pcm_avail_update returned %d", ret);
		ret = snd_pcm_recover(alsa_pcm, ret, !alsa_cfg.debug);
		if (ret < 0)
		{
			g_warning("alsa_get_avail(): snd_pcm_avail_update() failed: %s",
				  snd_strerror(ret));
			return 0;
		}
	}
	return snd_pcm_frames_to_bytes(alsa_pcm, ret);
}

/* get the free space on buffer */
int alsa_free(void)
{
	gint ret;

	g_static_mutex_lock(&alsa_mutex);

	if (remove_prebuffer && prebuffer)
	{
		prebuffer = FALSE;
		remove_prebuffer = FALSE;
	}
	if (prebuffer)
		remove_prebuffer = TRUE;
	
	ret = thread_buffer_size - get_thread_buffer_filled();

	g_static_mutex_unlock(&alsa_mutex);

	return ret;
}

/* do pause operation */
static void alsa_do_pause(gboolean p)
{
	if (paused == p)
		return;

	if (alsa_pcm)
	{
		if (alsa_can_pause)
			snd_pcm_pause(alsa_pcm, p);
		else if (p)
		{
			snd_pcm_drop(alsa_pcm);
			snd_pcm_prepare(alsa_pcm);
		}
	}
	paused = p;
}

void alsa_pause(short p)
{
	debug("alsa_pause");
	pause_request = p;
}

/* close PCM and release associated resources */
static void alsa_close_pcm(void)
{
	if (alsa_pcm)
	{
		int err;
		snd_pcm_drop(alsa_pcm);
		if ((err = snd_pcm_close(alsa_pcm)) < 0)
			g_warning("alsa_pcm_close() failed: %s",
				  snd_strerror(err));
		alsa_pcm = NULL;
	}
}

void alsa_close(void)
{
	if (!going)
		return;

	debug("Closing device");

	going = 0;

	g_thread_join(audio_thread);

	alsa_cleanup_mixer();

	g_free(inputf);
	inputf = NULL;
	g_free(outputf);
	outputf = NULL;

	alsa_save_config();

	if (alsa_cfg.debug)
		snd_output_close(logs);
	debug("Device closed");
}

/* reopen ALSA PCM */
#if 0
static gint alsa_reopen(struct snd_format *f)
{
	/* remember the current position */
	output_time_offset += (alsa_hw_written * 1000) / outputf->bps;
	alsa_hw_written = 0;

	alsa_close_pcm();

	return alsa_setup(f);
}
#endif

/* do flush (drop) operation */
static void alsa_do_flush(gint time)
{
	if (alsa_pcm)
	{
		snd_pcm_drop(alsa_pcm);
		snd_pcm_prepare(alsa_pcm);
	}
	/* correct the offset */
	output_time_offset = time;
	alsa_total_written = (guint64) time * inputf->bps / 1000;
	rd_index = wr_index = alsa_hw_written = 0;
	buffer_empty = 1;
}

void alsa_flush(gint time)
{
	flush_request = time;
	while ((flush_request != -1) && (going))
		g_usleep(10000);
}

static void parse_mixer_name(gchar *str, gchar **name, gint *index)
{
	gchar *end;

	while (isspace(*str))
		str++;

	if ((end = strchr(str, ',')) != NULL)
	{
		*name = g_strndup(str, end - str);
		end++;
		*index = atoi(end);
	}
	else
	{
		*name = g_strdup(str);
		*index = 0;
	}
}

gint alsa_get_mixer(snd_mixer_t **mixer, gint card)
{
	gchar *dev;
	gint err;

	debug("alsa_get_mixer");

	if ((err = snd_mixer_open(mixer, 0)) < 0)
	{
		g_warning("alsa_get_mixer(): Failed to open empty mixer: %s",
			  snd_strerror(err));
		mixer = NULL;
		return -1;
	}

	dev = g_strdup_printf("hw:%i", card);
	if ((err = snd_mixer_attach(*mixer, dev)) < 0)
	{
		g_warning("alsa_get_mixer(): Attaching to mixer %s failed: %s",
			  dev, snd_strerror(err));
		g_free(dev);
		return -1;
	}
	g_free(dev);

	if ((err = snd_mixer_selem_register(*mixer, NULL, NULL)) < 0)
	{
		g_warning("alsa_get_mixer(): Failed to register mixer: %s",
			  snd_strerror(err));
		return -1;
	}
	if ((err = snd_mixer_load(*mixer)) < 0)
	{
		g_warning("alsa_get_mixer(): Failed to load mixer: %s",
			  snd_strerror(err));
		return -1;
	}

	return (*mixer != NULL);
}


static snd_mixer_elem_t* alsa_get_mixer_elem(snd_mixer_t *mixer, gchar *name, gint index)
{
	snd_mixer_selem_id_t *selem_id;
	snd_mixer_elem_t* elem;
	snd_mixer_selem_id_alloca(&selem_id);

	if (index != -1)
		snd_mixer_selem_id_set_index(selem_id, index);
	if (name != NULL)
		snd_mixer_selem_id_set_name(selem_id, name);

	elem = snd_mixer_find_selem(mixer, selem_id);

	return elem;
}

static int alsa_setup_mixer(void)
{
	gchar *name;
	glong a, b;
	glong alsa_min_vol, alsa_max_vol;
	gint err, index;

	debug("alsa_setup_mixer");

	if ((err = alsa_get_mixer(&mixer, alsa_cfg.mixer_card)) < 0)
		return err;

	parse_mixer_name(alsa_cfg.mixer_device, &name, &index);

	pcm_element = alsa_get_mixer_elem(mixer, name, index);

	g_free(name);

	if (!pcm_element)
	{
		g_warning("alsa_setup_mixer(): Failed to find mixer element: %s",
			  alsa_cfg.mixer_device);
		return -1;
	}

	/*
	 * Work around a bug in alsa-lib up to 1.0.0rc2 where the
	 * new range don't take effect until the volume is changed.
	 * This hack should be removed once we depend on Alsa 1.0.0.
	 */
	snd_mixer_selem_get_playback_volume(pcm_element,
					    SND_MIXER_SCHN_FRONT_LEFT, &a);
	snd_mixer_selem_get_playback_volume(pcm_element,
					    SND_MIXER_SCHN_FRONT_RIGHT, &b);

	snd_mixer_selem_get_playback_volume_range(pcm_element,
						  &alsa_min_vol, &alsa_max_vol);
	snd_mixer_selem_set_playback_volume_range(pcm_element, 0, 100);

	if (alsa_max_vol == 0)
	{
		pcm_element = NULL;
		return -1;
	}

	alsa_set_volume(a * 100 / alsa_max_vol, b * 100 / alsa_max_vol);

	debug("alsa_setup_mixer: end");

	return 0;
}

static void alsa_cleanup_mixer(void)
{
	pcm_element = NULL;
	if (mixer)
	{
		snd_mixer_close(mixer);
		mixer = NULL;
	}
}

void alsa_get_volume(gint *l, gint *r)
{
	glong ll = *l, lr = *r;

	if (mixer_start)
	{
		alsa_setup_mixer();
		mixer_start = FALSE;
	}

	if (!pcm_element)
		return;

	snd_mixer_handle_events(mixer);

	snd_mixer_selem_get_playback_volume(pcm_element,
					    SND_MIXER_SCHN_FRONT_LEFT,
					    &ll);
	snd_mixer_selem_get_playback_volume(pcm_element,
					    SND_MIXER_SCHN_FRONT_RIGHT,
					    &lr);
	*l = ll;
	*r = lr;
}


void alsa_set_volume(gint l, gint r)
{
	if (!pcm_element)
		return;

	if (snd_mixer_selem_is_playback_mono(pcm_element))
	{
		if (l > r)
			snd_mixer_selem_set_playback_volume(pcm_element,
							    SND_MIXER_SCHN_MONO, l);
		else
			snd_mixer_selem_set_playback_volume(pcm_element,
							    SND_MIXER_SCHN_MONO, r);
	}
	else
	{
		snd_mixer_selem_set_playback_volume(pcm_element,
						    SND_MIXER_SCHN_FRONT_LEFT, l);
		snd_mixer_selem_set_playback_volume(pcm_element,
						    SND_MIXER_SCHN_FRONT_RIGHT, r);
	}

	if (snd_mixer_selem_has_playback_switch(pcm_element) && !snd_mixer_selem_has_playback_switch_joined(pcm_element))
	{
		snd_mixer_selem_set_playback_switch(pcm_element,
			SND_MIXER_SCHN_FRONT_LEFT, l != 0);
		snd_mixer_selem_set_playback_switch(pcm_element,
			SND_MIXER_SCHN_FRONT_RIGHT, r != 0);
	}
}


/*
 * audio stuff
 */

/* return the size of audio data filled in the audio thread buffer */
static int get_thread_buffer_filled(void)
{
	if (buffer_empty)
		return 0;

	if (wr_index > rd_index)
		return wr_index - rd_index;

	return thread_buffer_size - (rd_index - wr_index);
}

gint alsa_get_output_time(void)
{
	gint ret = 0;
	snd_pcm_sframes_t delay;
	guint64 bytes = alsa_hw_written;

	g_static_mutex_lock(&alsa_mutex);

	if (going && alsa_pcm != NULL)
	{
		if (!snd_pcm_delay(alsa_pcm, &delay))
		{
			unsigned int d = snd_pcm_frames_to_bytes(alsa_pcm, delay);
			if (bytes < d)
				bytes = 0;
			else
				bytes -= d;
		}
		ret = output_time_offset + (bytes * 1000) / outputf->bps;
	}

	g_static_mutex_unlock(&alsa_mutex);

	return ret;
}

gint alsa_get_written_time(void)
{
	gint ret = 0;

	g_static_mutex_lock(&alsa_mutex);

	if (going)
		ret = (alsa_total_written * 1000) / inputf->bps;

	g_static_mutex_unlock(&alsa_mutex);

	return ret;
}

/* transfer data to audio h/w; length is given in bytes
 *
 * data can be modified via effect plugin, rate conversion or
 * software volume before passed to audio h/w
 */
static void alsa_do_write(gpointer data, int length)
{
	if (paused)
		return;

	alsa_write_audio(data, length);
}

/* write callback */
void alsa_write(gpointer data, gint length)
{
	gint cnt;
	gchar *src = (gchar *)data;
	
	g_static_mutex_lock(&alsa_mutex);

	remove_prebuffer = FALSE;
	
	alsa_total_written += length;
	while (length > 0)
	{
		int wr;
		cnt = MIN(length, thread_buffer_size - wr_index);
		memcpy(thread_buffer + wr_index, src, cnt);
		wr = (wr_index + cnt) % thread_buffer_size;
		wr_index = wr;
		buffer_empty = 0;
		length -= cnt;
		src += cnt;
	}
	g_static_mutex_unlock(&alsa_mutex);
}

/* transfer data to audio h/w via normal write */
static void alsa_write_audio(gchar *data, gint length)
{
	snd_pcm_sframes_t written_frames;

	while (length > 0)
	{
		int frames = snd_pcm_bytes_to_frames(alsa_pcm, length);
		written_frames = snd_pcm_writei(alsa_pcm, data, frames);

		if (written_frames > 0)
		{
			int written = snd_pcm_frames_to_bytes(alsa_pcm,
							      written_frames);
			length -= written;
			data += written;
			alsa_hw_written += written;
		}
		else
		{
			int err = snd_pcm_recover(alsa_pcm, written_frames, !alsa_cfg.debug);
			if (err < 0)
			{
				g_warning("alsa_write_audio(): write error: %s",
					  snd_strerror(err));
				break;
			}
		}
	}
}

/* transfer audio data from thread buffer to h/w */
static void alsa_write_out_thread_data(gint avail)
{
	gint length, cnt;

	length = MIN(avail, get_thread_buffer_filled());
	while (length > 0)
	{
		int rd;
		cnt = MIN(length, thread_buffer_size - rd_index);
		alsa_do_write(thread_buffer + rd_index, cnt);
		rd = (rd_index + cnt) % thread_buffer_size;
		rd_index = rd;
		if (rd_index == wr_index)
			buffer_empty = 1;
		length -= cnt;
	}
}

/* audio thread loop */
/* FIXME: proper lock? */
static void *alsa_loop(void *arg)
{
	struct pollfd *pfd;
	gint npfds, err, avail;

	g_static_mutex_lock(&alsa_mutex);

	npfds = snd_pcm_poll_descriptors_count(alsa_pcm);
	if (npfds <= 0)
		goto _error;

	pfd = alloca(sizeof(*pfd) * npfds);
	err = snd_pcm_poll_descriptors(alsa_pcm, pfd, npfds);
	if (err != npfds)
                goto _error;

	while (going && alsa_pcm)
	{
		if (get_thread_buffer_filled() > prebuffer_size)
			prebuffer = FALSE;
		if (!paused && !prebuffer && get_thread_buffer_filled())
		{
			avail = alsa_get_avail();

			while (avail == 0) {
				g_static_mutex_unlock(&alsa_mutex);
				err = poll(pfd, npfds, alsa_cfg.period_time);
				g_static_mutex_lock(&alsa_mutex);

				if (err < 0 && errno != EINTR)
					goto _error;

				avail = alsa_get_avail();
			}

			alsa_write_out_thread_data(avail);
		}
		else
		{
			g_static_mutex_unlock(&alsa_mutex);
			g_usleep(10000);
			g_static_mutex_lock(&alsa_mutex);
		}

		if (pause_request != paused)
			alsa_do_pause(pause_request);

		if (flush_request != -1)
		{
			alsa_do_flush(flush_request);
			flush_request = -1;
			prebuffer = TRUE;
		}
	}

 _error:
	g_static_mutex_unlock(&alsa_mutex);
	alsa_close_pcm();
	g_free(thread_buffer);
	thread_buffer = NULL;
	g_thread_exit(NULL);
	return(NULL);
}

/* open callback */
gint alsa_open(AFormat fmt, gint rate, gint nch)
{
	debug("Opening device");
	if((inputf = snd_format_from_xmms(fmt, rate, nch)) == NULL) return 0;

	if (alsa_cfg.debug)
		snd_output_stdio_attach(&logs, stdout, 0);

	if (alsa_setup(inputf) < 0)
	{
		alsa_close();
		return 0;
	}

	if (!mixer)
		alsa_setup_mixer();

	output_time_offset = 0;
	alsa_total_written = alsa_hw_written = 0;
	going = TRUE;
	paused = FALSE;
	prebuffer = TRUE;
	remove_prebuffer = FALSE;

	thread_buffer_size = (guint64)aud_cfg->output_buffer_size * inputf->bps / 1000;
	if (thread_buffer_size < hw_buffer_size)
		thread_buffer_size = hw_buffer_size * 2;
	if (thread_buffer_size < 8192)
		thread_buffer_size = 8192;
	prebuffer_size = thread_buffer_size / 2;
	if (prebuffer_size < 8192)
		prebuffer_size = 8192;
	thread_buffer_size += hw_buffer_size;
	thread_buffer_size -= thread_buffer_size % hw_period_size;
	thread_buffer = g_malloc0(thread_buffer_size);
	wr_index = rd_index = 0;
	buffer_empty = 1;
	pause_request = FALSE;
	flush_request = -1;

	audio_thread = g_thread_create((GThreadFunc)alsa_loop, NULL, TRUE, NULL);
	return 1;
}

static struct snd_format * snd_format_from_xmms(AFormat fmt, gint rate, gint channels)
{
	struct snd_format *f = g_malloc(sizeof(struct snd_format));
	size_t i;
    gint found = 0;

	f->xmms_format = fmt;
	f->format = SND_PCM_FORMAT_UNKNOWN;

	for (i = 0; i < sizeof(format_table) / sizeof(format_table[0]); i++)
		if (format_table[i].xmms == fmt)
		{
			f->format = format_table[i].alsa;
                        found = 1;
			break;
		}

        if(!found) {
            g_free(f);
            return NULL;
        }

	/* Get rid of _NE */
	for (i = 0; i < sizeof(format_table) / sizeof(format_table[0]); i++)
		if (format_table[i].alsa == f->format)
		{
			f->xmms_format = format_table[i].xmms;
			break;
		}


	f->rate = rate;
	f->channels = channels;
	f->sample_bits = snd_pcm_format_physical_width(f->format);
	f->bps = (rate * f->sample_bits * channels) >> 3;

	return f;
}

static gint alsa_setup(struct snd_format *f)
{
	gint err;
	snd_pcm_hw_params_t *hwparams = NULL;
	snd_pcm_sw_params_t *swparams = NULL;
	guint alsa_buffer_time, alsa_period_time;
	snd_pcm_uframes_t alsa_buffer_size, alsa_period_size;

	debug("alsa_setup");

	g_free(outputf);
	outputf = snd_format_from_xmms(f->xmms_format, f->rate, f->channels);
        if(outputf == NULL) return -1;

	debug("Opening device: %s", alsa_cfg.pcm_device);
	/* FIXME: Can snd_pcm_open() return EAGAIN? */
	if ((err = snd_pcm_open(&alsa_pcm, alsa_cfg.pcm_device,
				SND_PCM_STREAM_PLAYBACK, 0)) < 0)
	{
		g_warning("alsa_setup(): Failed to open pcm device (%s): %s",
			  alsa_cfg.pcm_device, snd_strerror(err));
		alsa_pcm = NULL;
		g_free(outputf);
		outputf = NULL;
		return -1;
	}

	/* doesn't care about non-blocking */
	/* snd_pcm_nonblock(alsa_pcm, 0); */

	if (alsa_cfg.debug)
	{
		snd_pcm_info_t *info = NULL;
		int alsa_card, alsa_device, alsa_subdevice;

		snd_pcm_info_alloca(&info);
		snd_pcm_info(alsa_pcm, info);
		alsa_card = snd_pcm_info_get_card(info);
		alsa_device = snd_pcm_info_get_device(info);
		alsa_subdevice = snd_pcm_info_get_subdevice(info);
		printf("Card %i, Device %i, Subdevice %i\n",
		       alsa_card, alsa_device, alsa_subdevice);
	}

	snd_pcm_hw_params_alloca(&hwparams);

	if ((err = snd_pcm_hw_params_any(alsa_pcm, hwparams)) < 0)
	{
		g_warning("alsa_setup(): No configuration available for "
			  "playback: %s", snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(alsa_pcm, hwparams,
						SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		g_warning("alsa_setup(): Cannot set direct write mode: %s",
			  snd_strerror(err));
		return -1;
	}

	/* XXX: there is no down-dithering for 24bit yet --nenolod */
	if ((err = snd_pcm_hw_params_set_format(alsa_pcm, hwparams, outputf->format)) < 0)
	{
		g_warning("alsa_setup(): Sample format not "
			  "available for playback: %s",
			  snd_strerror(err));
		return -1;
	}

	snd_pcm_hw_params_set_channels_near(alsa_pcm, hwparams, &outputf->channels);
	if (outputf->channels != f->channels)
		return -1;

	snd_pcm_hw_params_set_rate_near(alsa_pcm, hwparams, &outputf->rate, 0);
	if (outputf->rate == 0)
	{
		g_warning("alsa_setup(): No usable samplerate available.");
		return -1;
	}
	if (outputf->rate != f->rate)
		return -1;

	outputf->sample_bits = snd_pcm_format_physical_width(outputf->format);
	outputf->bps = (outputf->rate * outputf->sample_bits * outputf->channels) >> 3;

	alsa_buffer_time = alsa_cfg.buffer_time * 1000;
	if ((err = snd_pcm_hw_params_set_buffer_time_near(alsa_pcm, hwparams,
							  &alsa_buffer_time, 0)) < 0)
	{
		g_warning("alsa_setup(): Set buffer time failed: %s.",
			  snd_strerror(err));
		return -1;
	}

	alsa_period_time = alsa_cfg.period_time * 1000;
	if ((err = snd_pcm_hw_params_set_period_time_near(alsa_pcm, hwparams,
							  &alsa_period_time, 0)) < 0)
	{
		g_warning("alsa_setup(): Set period time failed: %s.",
			  snd_strerror(err));
		return -1;
	}

	if (snd_pcm_hw_params(alsa_pcm, hwparams) < 0)
	{
		if (alsa_cfg.debug)
			snd_pcm_hw_params_dump(hwparams, logs);
		g_warning("alsa_setup(): Unable to install hw params");
		return -1;
	}

	if ((err = snd_pcm_hw_params_get_buffer_size(hwparams, &alsa_buffer_size)) < 0)
	{
		g_warning("alsa_setup(): snd_pcm_hw_params_get_buffer_size() "
			  "failed: %s",
			  snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_get_period_size(hwparams, &alsa_period_size, 0)) < 0)
	{
		g_warning("alsa_setup(): snd_pcm_hw_params_get_period_size() "
			  "failed: %s",
			  snd_strerror(err));
		return -1;
	}

	alsa_can_pause = snd_pcm_hw_params_can_pause(hwparams);
	debug("can pause: %d", alsa_can_pause);

	snd_pcm_sw_params_alloca(&swparams);
	snd_pcm_sw_params_current(alsa_pcm, swparams);

	if ((err = snd_pcm_sw_params_set_start_threshold(alsa_pcm,
			swparams, alsa_buffer_size - alsa_period_size) < 0))
		g_warning("alsa_setup(): setting start "
			  "threshold failed: %s", snd_strerror(err));
	if (snd_pcm_sw_params(alsa_pcm, swparams) < 0)
	{
		g_warning("alsa_setup(): Unable to install sw params");
		return -1;
	}

	if (alsa_cfg.debug)
	{
		snd_pcm_sw_params_dump(swparams, logs);
		snd_pcm_dump(alsa_pcm, logs);
	}

	hw_buffer_size = snd_pcm_frames_to_bytes(alsa_pcm, alsa_buffer_size);
	hw_period_size = snd_pcm_frames_to_bytes(alsa_pcm, alsa_period_size);
	if (inputf->bps != outputf->bps)
	{
		int align = (inputf->sample_bits * inputf->channels) / 8;
		hw_buffer_size_in = ((guint64)hw_buffer_size * inputf->bps +
				     outputf->bps/2) / outputf->bps;
		hw_period_size_in = ((guint64)hw_period_size * inputf->bps +
				     outputf->bps/2) / outputf->bps;
		hw_buffer_size_in -= hw_buffer_size_in % align;
		hw_period_size_in -= hw_period_size_in % align;
	}
	else
	{
		hw_buffer_size_in = hw_buffer_size;
		hw_period_size_in = hw_period_size;
	}

	debug("Device setup: buffer time: %i, size: %i.", alsa_buffer_time,
	      hw_buffer_size);
	debug("Device setup: period time: %i, size: %i.", alsa_period_time,
	      hw_period_size);
	debug("bits per sample: %i; frame size: %i; Bps: %i",
	      snd_pcm_format_physical_width(outputf->format),
	      (int)snd_pcm_frames_to_bytes(alsa_pcm, 1), outputf->bps);

	return 0;
}

void alsa_tell(AFormat * fmt, gint * rate, gint * nch)
{
	(*fmt) = inputf->xmms_format;
	(*rate) = inputf->rate;
	(*nch) = inputf->channels;
}
