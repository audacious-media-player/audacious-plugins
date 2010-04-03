/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2010 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include "alsa.h"

#define LEAST_BUFFER 100 /* milliseconds */

GMutex * alsa_mutex;
static snd_pcm_t * alsa_handle;
static GCond * alsa_cond;
static gboolean initted;

static snd_pcm_format_t alsa_format;
static gint alsa_channels, alsa_rate;

static void * alsa_buffer;
static gint alsa_buffer_length, alsa_buffer_data_start, alsa_buffer_data_length;
static gboolean read_locked;

static gint64 alsa_time; /* microseconds */
static gboolean alsa_paused;
static gint alsa_paused_time;

static gboolean pump_quit;
static GThread * pump_thread;

static snd_mixer_t * alsa_mixer;
static snd_mixer_elem_t * alsa_mixer_element;

static void * pump (void * unused)
{
    snd_pcm_status_t * status;
    gint length;

    g_mutex_lock (alsa_mutex);
    read_locked = FALSE;
    g_cond_signal (alsa_cond);

    snd_pcm_status_alloca (& status);

    while (! pump_quit)
    {
        if (alsa_paused)
        {
            g_cond_wait (alsa_cond, alsa_mutex);
            continue;
        }

        length = snd_pcm_frames_to_bytes (alsa_handle, LEAST_BUFFER / 2 *
         alsa_rate / 1000);
        length = MIN (length, alsa_buffer_data_length);
        length = MIN (length, alsa_buffer_length - alsa_buffer_data_start);

        if (length == 0)
        {
            g_cond_wait (alsa_cond, alsa_mutex);
            continue;
        }

        read_locked = TRUE;
        g_mutex_unlock (alsa_mutex);

        length = snd_pcm_writei (alsa_handle, (gchar *) alsa_buffer +
         alsa_buffer_data_start, snd_pcm_bytes_to_frames (alsa_handle, length));

        g_mutex_lock (alsa_mutex);

        if (length < 0)
        {
            if (! pump_quit && ! alsa_paused) /* ignore errors caused by drop */
                CHECK (snd_pcm_recover, alsa_handle, length, 0);

        FAILED:
            length = 0;
        }

        length = snd_pcm_frames_to_bytes (alsa_handle, length);

        alsa_buffer_data_start = (alsa_buffer_data_start + length) %
         alsa_buffer_length;
        alsa_buffer_data_length -= length;

        read_locked = FALSE;
        g_cond_signal (alsa_cond);
    }

    g_mutex_unlock (alsa_mutex);
    return NULL;
}

static void start_playback (void)
{
    AUDDBG ("Starting playback.\n");

    if (snd_pcm_state (alsa_handle) == SND_PCM_STATE_PAUSED)
        CHECK (snd_pcm_pause, alsa_handle, 0);
    else
        CHECK (snd_pcm_prepare, alsa_handle);

FAILED:
    alsa_paused = FALSE;
    g_cond_signal (alsa_cond);
}

static gint real_output_time (void)
{
    snd_pcm_status_t * status;
    gint time = 0;

    snd_pcm_status_alloca (& status);
    CHECK (snd_pcm_status, alsa_handle, status);
    time = (alsa_time - (gint64) (snd_pcm_bytes_to_frames (alsa_handle,
     alsa_buffer_data_length) + snd_pcm_status_get_delay (status)) * 1000000 /
     alsa_rate) / 1000;

FAILED:
    return time;
}

OutputPluginInitStatus alsa_init (void)
{
    alsa_mutex = g_mutex_new ();
    alsa_handle = NULL;
    alsa_cond = g_cond_new ();
    initted = FALSE;

    return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}

void alsa_soft_init (void)
{
    if (! initted)
    {
        AUDDBG ("Initialize.\n");
        alsa_config_load ();
        alsa_open_mixer ();
        initted = TRUE;
    }
}

void alsa_cleanup (void)
{
    if (initted)
    {
        AUDDBG ("Cleanup.\n");
        alsa_close_mixer ();
        alsa_config_save ();
    }

    g_mutex_free (alsa_mutex);
    g_cond_free (alsa_cond);
}

static snd_pcm_format_t convert_aud_format (AFormat aud_format)
{
    const struct
    {
        AFormat aud_format;
        snd_pcm_format_t format;
    }
    table[] =
    {
        {FMT_FLOAT, SND_PCM_FORMAT_FLOAT},
        {FMT_S8, SND_PCM_FORMAT_S8},
        {FMT_U8, SND_PCM_FORMAT_U8},
        {FMT_S16_LE, SND_PCM_FORMAT_S16_LE},
        {FMT_S16_BE, SND_PCM_FORMAT_S16_BE},
        {FMT_U16_LE, SND_PCM_FORMAT_U16_LE},
        {FMT_U16_BE, SND_PCM_FORMAT_U16_BE},
        {FMT_S24_LE, SND_PCM_FORMAT_S24_LE},
        {FMT_S24_BE, SND_PCM_FORMAT_S24_BE},
        {FMT_U24_LE, SND_PCM_FORMAT_U24_LE},
        {FMT_U24_BE, SND_PCM_FORMAT_U24_BE},
        {FMT_S32_LE, SND_PCM_FORMAT_S32_LE},
        {FMT_S32_BE, SND_PCM_FORMAT_S32_BE},
        {FMT_U32_LE, SND_PCM_FORMAT_U32_LE},
        {FMT_U32_BE, SND_PCM_FORMAT_U32_BE},
    };

    gint count;

    for (count = 0; count < G_N_ELEMENTS (table); count ++)
    {
         if (table[count].aud_format == aud_format)
             return table[count].format;
    }

    return SND_PCM_FORMAT_UNKNOWN;
}

gint alsa_open_audio (AFormat aud_format, gint rate, gint channels)
{
    snd_pcm_format_t format = convert_aud_format (aud_format);
    snd_pcm_hw_params_t * params;
    guint useconds;
    snd_pcm_uframes_t frames, period;
    gint hard_buffer, soft_buffer;

    g_mutex_lock (alsa_mutex);
    alsa_soft_init ();

    AUDDBG ("Opening PCM device %s for %s, %d channels, %d Hz.\n",
     alsa_config_pcm, snd_pcm_format_name (format), channels, rate);
    CHECK_NOISY (snd_pcm_open, & alsa_handle, alsa_config_pcm,
     SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_alloca (& params);
    CHECK_NOISY (snd_pcm_hw_params_any, alsa_handle, params);
    CHECK_NOISY (snd_pcm_hw_params_set_access, alsa_handle, params,
     SND_PCM_ACCESS_RW_INTERLEAVED);
    CHECK_NOISY (snd_pcm_hw_params_set_format, alsa_handle, params, format);
    CHECK_NOISY (snd_pcm_hw_params_set_channels, alsa_handle, params, channels);
    CHECK_NOISY (snd_pcm_hw_params_set_rate, alsa_handle, params, rate, 0);
    useconds = 1000 * LEAST_BUFFER;
    CHECK_NOISY (snd_pcm_hw_params_set_buffer_time_min, alsa_handle, params,
     & useconds, 0);
    useconds = 1000 * MAX (LEAST_BUFFER * 11 / 10, aud_cfg->output_buffer_size /
     2);
    CHECK_NOISY (snd_pcm_hw_params_set_buffer_time_max, alsa_handle, params,
     & useconds, 0);
    CHECK_NOISY (snd_pcm_hw_params, alsa_handle, params);

    alsa_format = format;
    alsa_channels = channels;
    alsa_rate = rate;

    CHECK_NOISY (snd_pcm_get_params, alsa_handle, & frames, & period);
    hard_buffer = (gint64) frames * 1000 / rate;
    soft_buffer = MAX (LEAST_BUFFER, aud_cfg->output_buffer_size - hard_buffer);
    AUDDBG ("Hardware buffer %d ms, software buffer %d ms.\n", hard_buffer,
     soft_buffer);

    alsa_buffer_length = snd_pcm_frames_to_bytes (alsa_handle, (gint64)
     soft_buffer * rate / 1000);
    alsa_buffer = g_malloc (alsa_buffer_length);
    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    alsa_time = 0;
    alsa_paused = TRUE; /* for buffering */
    alsa_paused_time = 0;

    pump_quit = FALSE;
    pump_thread = g_thread_create (pump, NULL, TRUE, NULL);
    g_cond_wait (alsa_cond, alsa_mutex);

    g_mutex_unlock (alsa_mutex);
    return 1;

FAILED:
    if (alsa_handle != NULL)
    {
        snd_pcm_close (alsa_handle);
        alsa_handle = NULL;
    }

    g_mutex_unlock (alsa_mutex);
    return 0;
}

void alsa_close_audio (void)
{
    AUDDBG ("Closing audio.\n");
    g_mutex_lock (alsa_mutex);
    pump_quit = TRUE;

    if (! alsa_config_drop_workaround)
        CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    g_cond_signal (alsa_cond);
    g_mutex_unlock (alsa_mutex);
    g_thread_join (pump_thread);
    g_mutex_lock (alsa_mutex);

    g_free (alsa_buffer);
    snd_pcm_close (alsa_handle);
    alsa_handle = NULL;
    g_mutex_unlock (alsa_mutex);
}

void alsa_write_audio (void * data, gint length)
{
    g_mutex_lock (alsa_mutex);

    while (1)
    {
        gint writable = MIN (alsa_buffer_length - alsa_buffer_data_length,
         length);
        gint start = (alsa_buffer_data_start + alsa_buffer_data_length) %
         alsa_buffer_length;

        if (writable > alsa_buffer_length - start)
        {
            gint part = alsa_buffer_length - start;

            memcpy ((gint8 *) alsa_buffer + start, data, part);
            memcpy (alsa_buffer, (gint8 *) data + part, writable - part);
        }
        else
            memcpy ((gint8 *) alsa_buffer + start, data, writable);

        data = (gint8 *) data + writable;
        length -= writable;

        alsa_buffer_data_length += writable;
        alsa_time += (gint64) snd_pcm_bytes_to_frames (alsa_handle, writable) *
         1000000 / alsa_rate;

        if (! length)
            break;

        if (alsa_paused) /* buffering completed */
            start_playback ();

        g_cond_signal (alsa_cond);
        g_cond_wait (alsa_cond, alsa_mutex);
    }

    g_mutex_unlock (alsa_mutex);
}

void alsa_drain (void)
{
    AUDDBG ("Drain.\n");
    g_mutex_lock (alsa_mutex);

    while (alsa_buffer_data_length > 0)
    {
        if (alsa_paused) /* buffering completed */
            start_playback ();

        g_cond_wait (alsa_cond, alsa_mutex);
    }

    g_mutex_unlock (alsa_mutex);

    CHECK (snd_pcm_drain, alsa_handle);

FAILED:
    return;
}

void alsa_set_written_time (gint time)
{
    AUDDBG ("Setting time counter to %d.\n", time);
    g_mutex_lock (alsa_mutex);
    alsa_time = 1000 * (gint64) time;
    g_mutex_unlock (alsa_mutex);
}

gint alsa_written_time (void)
{
    gint time;

    g_mutex_lock (alsa_mutex);
    time = alsa_time / 1000;
    g_mutex_unlock (alsa_mutex);
    return time;
}

gint alsa_output_time (void)
{
    gint time = 0;

    g_mutex_lock (alsa_mutex);

    if (alsa_paused)
        time = alsa_paused_time;
    else
        time = real_output_time ();

    g_mutex_unlock (alsa_mutex);
    return time;
}

void alsa_flush (gint time)
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    g_mutex_lock (alsa_mutex);

    alsa_time = (gint64) time * 1000;
    alsa_paused = TRUE; /* for buffering */
    alsa_paused_time = time;

    if (! alsa_config_drop_workaround)
        CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    while (read_locked)
        g_cond_wait (alsa_cond, alsa_mutex);

    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    g_cond_signal (alsa_cond);
    g_mutex_unlock (alsa_mutex);
}

void alsa_pause (gshort pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    g_mutex_lock (alsa_mutex);

    if (pause)
    {
        alsa_paused = TRUE;
        alsa_paused_time = real_output_time ();

        CHECK (snd_pcm_pause, alsa_handle, pause);
    }

FAILED:
    g_cond_signal (alsa_cond);
    g_mutex_unlock (alsa_mutex);
}

void alsa_open_mixer (void)
{
    snd_mixer_selem_id_t * selem_id;

    alsa_mixer = NULL;

    if (alsa_config_mixer_element == NULL)
        goto FAILED;

    AUDDBG ("Opening mixer card %s.\n", alsa_config_mixer);
    CHECK_NOISY (snd_mixer_open, & alsa_mixer, 0);
    CHECK_NOISY (snd_mixer_attach, alsa_mixer, alsa_config_mixer);
    CHECK_NOISY (snd_mixer_selem_register, alsa_mixer, NULL, NULL);
    CHECK_NOISY (snd_mixer_load, alsa_mixer);

    snd_mixer_selem_id_alloca (& selem_id);
    snd_mixer_selem_id_set_name (selem_id, alsa_config_mixer_element);
    alsa_mixer_element = snd_mixer_find_selem (alsa_mixer, selem_id);

    if (alsa_mixer_element == NULL)
    {
        ERROR_NOISY ("snd_mixer_find_selem failed.\n");
        goto FAILED;
    }

    CHECK (snd_mixer_selem_set_playback_volume_range, alsa_mixer_element, 0, 100);
    return;

FAILED:
    if (alsa_mixer != NULL)
    {
        snd_mixer_close (alsa_mixer);
        alsa_mixer = NULL;
    }
}

void alsa_close_mixer (void)
{
    if (alsa_mixer != NULL)
        snd_mixer_close (alsa_mixer);
}

void alsa_get_volume (gint * left, gint * right)
{
    glong left_l = 0, right_l = 0;

    g_mutex_lock (alsa_mutex);
    alsa_soft_init ();

    if (alsa_mixer == NULL)
        goto FAILED;

    CHECK (snd_mixer_handle_events, alsa_mixer);

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_element))
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_MONO, & left_l);
        right_l = left_l;
    }
    else
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_LEFT, & left_l);
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_RIGHT, & right_l);
    }

FAILED:
    g_mutex_unlock (alsa_mutex);

    * left = left_l;
    * right = right_l;
}

void alsa_set_volume (gint left, gint right)
{
    g_mutex_lock (alsa_mutex);
    alsa_soft_init ();

    if (alsa_mixer == NULL)
        goto FAILED;

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_element))
    {
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_MONO, MAX (left, right));

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
            CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_MONO, MAX (left, right) != 0);
    }
    else
    {
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_LEFT, left);
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_RIGHT, right);

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
        {
            if (snd_mixer_selem_has_playback_switch_joined (alsa_mixer_element))
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_LEFT, MAX (left, right) != 0);
            else
            {
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_LEFT, left != 0);
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_RIGHT, right != 0);
            }
        }
    }

    CHECK (snd_mixer_handle_events, alsa_mixer);

FAILED:
    g_mutex_unlock (alsa_mutex);
}
