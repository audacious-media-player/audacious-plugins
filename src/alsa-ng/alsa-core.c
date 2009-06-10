/*
 * Audacious ALSA Plugin (-ng)
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#define ALSA_DEBUG
#include "alsa-stdinc.h"

static snd_pcm_t *pcm_handle = NULL;
static alsaplug_ringbuf_t pcm_ringbuf;
static gboolean pcm_going = FALSE;
static GThread *audio_thread = NULL;
static gint bps;

static gsize wr_total = 0;
static gsize wr_hwframes = 0;

static gint flush_request, paused;

static GMutex *pcm_pause_mutex, *pcm_state_mutex;
static GCond *pcm_pause_cond, *pcm_state_cond, *pcm_flush_cond;

/********************************************************************************
 * ALSA Mixer setting functions.                                                *
 ********************************************************************************/

static snd_mixer_t *amixer = NULL;
static gboolean mixer_ready = FALSE;

static snd_mixer_elem_t *
alsaplug_get_mixer_elem_by_name(snd_mixer_t *mixer, gchar *name)
{
    snd_mixer_selem_id_t *selem_id;
    snd_mixer_elem_t *elem;

    g_return_val_if_fail(mixer != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    snd_mixer_selem_id_alloca(&selem_id);
    snd_mixer_selem_id_set_name(selem_id, name);

    elem = snd_mixer_find_selem(mixer, selem_id);
    if (elem == NULL) {
        _ERROR("Requested mixer element %p/%s not found", mixer, name);
        return NULL;
    }

    snd_mixer_selem_set_playback_volume_range(elem, 0, 100);

    return elem;
}

/* try to determine the best choice... may need tweaking. --nenolod */
static snd_mixer_elem_t *
alsaplug_guess_mixer_elem(snd_mixer_t *mixer)
{
    gchar *elem_names[] = { "Wave", "PCM", "Front", "Master" };
    gint i;
    snd_mixer_elem_t *elem;

    for (i = 0; i < G_N_ELEMENTS(elem_names); i++)
    {
        elem = alsaplug_get_mixer_elem_by_name(mixer, elem_names[i]);
        if (elem != NULL)
            return elem;
    }

    return NULL;
}

static gint
alsaplug_mixer_new(snd_mixer_t **mixer)
{
    gint ret;

    _ERROR("setting up mixer");

    ret = snd_mixer_open(mixer, 0);
    if (ret < 0)
    {
        _ERROR("mixer initialization failed: %s", snd_strerror(ret));
        return ret;
    }

    ret = snd_mixer_attach(*mixer, "default");
    if (ret < 0)
    {
        snd_mixer_close(*mixer);
        _ERROR("failed to attach to hardware mixer: %s", snd_strerror(ret));
        return ret;
    }

    ret = snd_mixer_selem_register(*mixer, NULL, NULL);
    if (ret < 0)
    {
        snd_mixer_detach(*mixer, "default");
        snd_mixer_close(*mixer);
        _ERROR("failed to register hardware mixer: %s", snd_strerror(ret));
        return ret;
    }

    ret = snd_mixer_load(*mixer);
    if (ret < 0)
    {
        snd_mixer_detach(*mixer, "default");
        snd_mixer_close(*mixer);
        _ERROR("failed to load hardware mixer controls: %s", snd_strerror(ret));
        return ret;
    }    

    return 0;
}

static void
alsaplug_set_volume(gint l, gint r)
{
    snd_mixer_elem_t *elem = alsaplug_guess_mixer_elem(amixer);

    if (elem == NULL)
        return;

    if (snd_mixer_selem_is_playback_mono(elem))
    {
        gint vol = (l > r) ? l : r;

        snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_MONO, vol);

        if (snd_mixer_selem_has_playback_switch(elem))
            snd_mixer_selem_set_playback_switch(elem, SND_MIXER_SCHN_MONO, vol != 0);
    }
    else
    {
        snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, l);
        snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, r);

        if (snd_mixer_selem_has_playback_switch(elem) && !snd_mixer_selem_has_playback_switch_joined(elem))
        {
            snd_mixer_selem_set_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, l != 0);
            snd_mixer_selem_set_playback_switch(elem, SND_MIXER_SCHN_FRONT_RIGHT, r != 0);
        }
    }    

    snd_mixer_handle_events(amixer);
}

/********************************************************************************
 * ALSA PCM I/O functions.                                                      *
 ********************************************************************************/

static void
alsaplug_write_buffer(gpointer data, gint length)
{
    snd_pcm_sframes_t wr_frames;

    while (length > 0)
    {
        gint frames = snd_pcm_bytes_to_frames(pcm_handle, length);
        wr_frames = snd_pcm_writei(pcm_handle, data, frames);

        if (wr_frames > 0)
        {
            gint written = snd_pcm_frames_to_bytes(pcm_handle, wr_frames);
            length -= written;
            data += written;
        }
        else
        {
            gint err = snd_pcm_recover(pcm_handle, wr_frames, 1);
            if (err < 0)
                _ERROR("(write) snd_pcm_recover: %s", snd_strerror(err));

            return;
        }
    }
}

static gpointer
alsaplug_loop(gpointer unused)
{
    gchar buf[2048];

    while (pcm_going)
    {
        if (flush_request != -1)
        {
            snd_pcm_drop(pcm_handle);
            snd_pcm_prepare(pcm_handle);
            wr_total = flush_request * (bps / 1000);
            flush_request = -1;

            g_cond_broadcast(pcm_flush_cond);
        }

        if (alsaplug_ringbuffer_read(&pcm_ringbuf, buf, 2048) == -1)
        {
            /* less than 2048 bytes to go...? */
            gint remain = alsaplug_ringbuffer_used(&pcm_ringbuf);
            if (remain <= 2048 && remain > 0)
            {
                alsaplug_ringbuffer_read(&pcm_ringbuf, buf, remain);
                alsaplug_write_buffer(buf, remain);
            }
            else
            {
                g_mutex_lock(pcm_state_mutex);
                g_cond_wait(pcm_state_cond, pcm_state_mutex);
                g_mutex_unlock(pcm_state_mutex);
            }

            continue;
        }

        alsaplug_write_buffer(buf, 2048);
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    pcm_handle = NULL;
    audio_thread = NULL;
    alsaplug_ringbuffer_destroy(&pcm_ringbuf);

    return NULL;
}

/********************************************************************************
 * Output Plugin API implementation.                                            *
 ********************************************************************************/

static OutputPluginInitStatus
alsaplug_init(void)
{
    gint card = -1;

    pcm_pause_mutex = g_mutex_new();
    pcm_pause_cond = g_cond_new();

    pcm_state_mutex = g_mutex_new();
    pcm_state_cond = g_cond_new();
    pcm_flush_cond = g_cond_new();

    if (snd_card_next(&card) != 0)
        return OUTPUT_PLUGIN_INIT_NO_DEVICES;

    if (!alsaplug_mixer_new(&amixer))
        mixer_ready = TRUE;

    return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}

static gint
alsaplug_open_audio(AFormat fmt, gint rate, gint nch)
{
    gint err, bitwidth, ringbuf_size;
    snd_pcm_format_t afmt;
    snd_pcm_hw_params_t *hwparams = NULL;

    afmt = alsaplug_format_convert(fmt);

    if ((err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        _ERROR("snd_pcm_open: %s", snd_strerror(err));
        pcm_handle = NULL;
        return -1;
    }

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_hw_params_any(pcm_handle, hwparams);
    snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, hwparams, afmt);
    snd_pcm_hw_params_set_channels(pcm_handle, hwparams, nch);
    snd_pcm_hw_params_set_rate(pcm_handle, hwparams, rate, 0);

    err = snd_pcm_hw_params(pcm_handle, hwparams);
    if (err < 0)
    {
        _ERROR("snd_pcm_hw_params failed: %s", snd_strerror(err));
        return -1;
    }

    bitwidth = snd_pcm_format_physical_width(afmt);
    bps = (rate * bitwidth * nch) >> 3;
    ringbuf_size = aud_cfg->output_buffer_size * bps / 1000;
    alsaplug_ringbuffer_init(&pcm_ringbuf, ringbuf_size);
    pcm_going = TRUE;
    flush_request = -1;

    audio_thread = g_thread_create(alsaplug_loop, NULL, TRUE, NULL);
    return 1;
}

static void
alsaplug_close_audio(void)
{
    g_mutex_lock(pcm_state_mutex);

    pcm_going = FALSE;
    wr_total = 0;
    wr_hwframes = 0;
    bps = 0;

    g_mutex_unlock(pcm_state_mutex);
    g_cond_broadcast(pcm_state_cond);

    if (audio_thread != NULL)
        g_thread_join(audio_thread);

    audio_thread = NULL;
}

static void
alsaplug_write_audio(gpointer data, gint length)
{
    /* software pause... snd_pcm_pause() is not safe. --nenolod */
    if (paused)
    {
        g_mutex_lock(pcm_pause_mutex);
        g_cond_wait(pcm_pause_cond, pcm_pause_mutex);
        g_mutex_unlock(pcm_pause_mutex);
    }

    g_mutex_lock(pcm_state_mutex);
    wr_total += length;
    alsaplug_ringbuffer_write(&pcm_ringbuf, data, length);
    g_mutex_unlock(pcm_state_mutex);
    g_cond_broadcast(pcm_state_cond);
}

static gint
alsaplug_output_time(void)
{
    gint ret = 0;
    snd_pcm_sframes_t delay;
    gsize bytes = wr_total;

    g_mutex_lock(pcm_state_mutex);

    if (pcm_going && pcm_handle != NULL)
    {
        if (!snd_pcm_delay(pcm_handle, &delay))
        {
            guint d = snd_pcm_frames_to_bytes(pcm_handle, delay);
            if (bytes < d)
                bytes = 0;
            else
                bytes -= d;
        }

        ret = (bytes * 1000) / bps;
    }

    g_mutex_unlock(pcm_state_mutex);

    return ret;
}

static gint
alsaplug_written_time(void)
{
    gint ret = 0;

    g_mutex_lock(pcm_state_mutex);

    if (pcm_going)
        ret = (wr_total * 1000) / bps;

    g_mutex_unlock(pcm_state_mutex);

    return ret;
}

static gint
alsaplug_buffer_free(void)
{
    gint ret;

    g_mutex_lock(pcm_state_mutex);

    if (pcm_going == FALSE)
        ret = 0;
    else
        ret = alsaplug_ringbuffer_free(&pcm_ringbuf);

    g_mutex_unlock(pcm_state_mutex);

    return ret;
}

static void
alsaplug_flush(gint time)
{
    /* make the request... */
    g_mutex_lock(pcm_state_mutex);
    flush_request = time;
    g_cond_broadcast(pcm_state_cond);

    /* ...then wait for the transaction to complete. */
    g_cond_wait(pcm_flush_cond, pcm_state_mutex);
    g_mutex_unlock(pcm_state_mutex);
}

static gint
alsaplug_buffer_playing(void)
{
    gint ret;

    g_mutex_lock(pcm_state_mutex);

    if (pcm_going == FALSE)
        ret = 0;
    else 
        ret = alsaplug_ringbuffer_used(&pcm_ringbuf) != 0;

    g_mutex_unlock(pcm_state_mutex);

    return ret;
}

static void
alsaplug_pause(short p)
{
    g_mutex_lock(pcm_pause_mutex);
    paused = p;
    g_mutex_unlock(pcm_pause_mutex);
    g_cond_broadcast(pcm_pause_cond);
}

/********************************************************************************
 * Plugin glue.                                                                 *
 ********************************************************************************/

static OutputPlugin alsa_op = {
    .description = "ALSA Output Plugin (-ng)",
    .probe_priority = 1,
    .init = alsaplug_init,
    .open_audio = alsaplug_open_audio,
    .close_audio = alsaplug_close_audio,
    .write_audio = alsaplug_write_audio,
    .output_time = alsaplug_output_time,
    .written_time = alsaplug_written_time,
    .buffer_free = alsaplug_buffer_free,
    .buffer_playing = alsaplug_buffer_playing,
    .flush = alsaplug_flush,
    .pause = alsaplug_pause,
    .set_volume = alsaplug_set_volume,
};

OutputPlugin *alsa_oplist[] = { &alsa_op, NULL };
SIMPLE_OUTPUT_PLUGIN(alsa, alsa_oplist);
