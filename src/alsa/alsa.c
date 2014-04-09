/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2012 John Lindgren
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

/*
 * Because ALSA is not thread-safe (despite claims to the contrary) we use non-
 * blocking output in the pump thread with the mutex locked, then unlock the
 * mutex and wait for more room in the buffer with poll() while other threads
 * lock the mutex and read the output time.  We poll a pipe of our own as well
 * as the ALSA file descriptors so that we can wake up the pump thread when
 * needed.
 *
 * When paused, or when it comes to the end of the data given it, the pump will
 * wait on alsa_cond for the signal to continue.  When it has more data waiting,
 * however, it will be sitting in poll() waiting for ALSA's signal that more
 * data can be written.
 *
 * * After adding more data to the buffer, and after resuming from pause,
 *   signal on alsa_cond to wake the pump.  (There is no need to signal when
 *   entering pause.)
 * * After setting the pump_quit flag, signal on alsa_cond AND the poll_pipe
 *   before joining the thread.
 */

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>

#include <alsa/asoundlib.h>

#include <libaudcore/runtime.h>
#include <libaudcore/runtime.h>
#include <audacious/plugin.h>

#include "alsa.h"

#define CHECK_VAL_RECOVER(value, function, ...) \
do { \
    (value) = function (__VA_ARGS__); \
    if ((value) < 0) { \
        CHECK (snd_pcm_recover, alsa_handle, (value), 0); \
        CHECK_VAL ((value), function, __VA_ARGS__); \
    } \
} while (0)

#define CHECK_RECOVER(function, ...) \
do { \
    int CHECK_RECOVER_error; \
    CHECK_VAL_RECOVER (CHECK_RECOVER_error, function, __VA_ARGS__); \
} while (0)

static snd_pcm_t * alsa_handle;
static pthread_mutex_t alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t alsa_cond = PTHREAD_COND_INITIALIZER;

static snd_pcm_format_t alsa_format;
static int alsa_channels, alsa_rate;

static void * alsa_buffer;
static int alsa_buffer_length, alsa_buffer_data_start, alsa_buffer_data_length;
static int alsa_period; /* milliseconds */

static int64_t alsa_written; /* frames */
static char alsa_prebuffer, alsa_paused;
static int alsa_paused_delay; /* frames */

static int poll_pipe[2];
static int poll_count;
static struct pollfd * poll_handles;

static char pump_quit;
static pthread_t pump_thread;

static snd_mixer_t * alsa_mixer;
static snd_mixer_elem_t * alsa_mixer_element;

static char poll_setup (void)
{
    if (pipe (poll_pipe))
    {
        ERROR ("Failed to create pipe: %s.\n", strerror (errno));
        return 0;
    }

    if (fcntl (poll_pipe[0], F_SETFL, O_NONBLOCK))
    {
        ERROR ("Failed to set O_NONBLOCK on pipe: %s.\n", strerror (errno));
        close (poll_pipe[0]);
        close (poll_pipe[1]);
        return 0;
    }

    poll_count = 1 + snd_pcm_poll_descriptors_count (alsa_handle);
    poll_handles = g_new (struct pollfd, poll_count);
    poll_handles[0].fd = poll_pipe[0];
    poll_handles[0].events = POLLIN;
    poll_count = 1 + snd_pcm_poll_descriptors (alsa_handle, poll_handles + 1,
     poll_count - 1);

    return 1;
}

static void poll_sleep (void)
{
    if (poll (poll_handles, poll_count, -1) < 0)
    {
        ERROR ("Failed to poll: %s.\n", strerror (errno));
        return;
    }

    if (poll_handles[0].revents & POLLIN)
    {
        char c;
        while (read (poll_pipe[0], & c, 1) == 1)
            ;
    }
}

static void poll_wake (void)
{
    const char c = 0;
    if (write (poll_pipe[1], & c, 1) < 0)
        ERROR ("Failed to write to pipe: %s.\n", strerror (errno));
}

static void poll_cleanup (void)
{
    close (poll_pipe[0]);
    close (poll_pipe[1]);
    g_free (poll_handles);
}

static void * pump (void * unused)
{
    pthread_mutex_lock (& alsa_mutex);
    pthread_cond_broadcast (& alsa_cond); /* signal thread started */

    char failed = 0;
    char workaround = 0;
    int slept = 0;

    while (! pump_quit)
    {
        if (alsa_prebuffer || alsa_paused || ! snd_pcm_bytes_to_frames
         (alsa_handle, alsa_buffer_data_length))
        {
            pthread_cond_wait (& alsa_cond, & alsa_mutex);
            continue;
        }

        int length;
        CHECK_VAL_RECOVER (length, snd_pcm_avail_update, alsa_handle);

        if (! length)
            goto WAIT;

        slept = 0;

        length = snd_pcm_frames_to_bytes (alsa_handle, length);
        length = MIN (length, alsa_buffer_data_length);
        length = MIN (length, alsa_buffer_length - alsa_buffer_data_start);
        length = snd_pcm_bytes_to_frames (alsa_handle, length);

        int written;
        CHECK_VAL_RECOVER (written, snd_pcm_writei, alsa_handle, (char *)
         alsa_buffer + alsa_buffer_data_start, length);

        failed = 0;

        written = snd_pcm_frames_to_bytes (alsa_handle, written);
        alsa_buffer_data_start += written;
        alsa_buffer_data_length -= written;

        pthread_cond_broadcast (& alsa_cond); /* signal write complete */

        if (alsa_buffer_data_start == alsa_buffer_length)
        {
            alsa_buffer_data_start = 0;
            continue;
        }

        if (! snd_pcm_bytes_to_frames (alsa_handle, alsa_buffer_data_length))
            continue;

    WAIT:
        pthread_mutex_unlock (& alsa_mutex);

        if (slept > 4)
        {
            AUDDBG ("Activating timer workaround.\n");
            workaround = 1;
        }

        if (workaround && slept)
        {
            const struct timespec delay = {.tv_sec = 0, .tv_nsec = 600000 *
             alsa_period};
            nanosleep (& delay, NULL);
        }
        else
        {
            poll_sleep ();
            slept ++;
        }

        pthread_mutex_lock (& alsa_mutex);
        continue;

    FAILED:
        if (failed)
            break;

        failed = 1;
        CHECK (snd_pcm_prepare, alsa_handle);
    }

    pthread_mutex_unlock (& alsa_mutex);
    return NULL;
}

static void pump_start (void)
{
    AUDDBG ("Starting pump.\n");
    pthread_create (& pump_thread, NULL, pump, NULL);
    pthread_cond_wait (& alsa_cond, & alsa_mutex);
}

static void pump_stop (void)
{
    AUDDBG ("Stopping pump.\n");
    pump_quit = 1;
    pthread_cond_broadcast (& alsa_cond);
    poll_wake ();
    pthread_mutex_unlock (& alsa_mutex);
    pthread_join (pump_thread, NULL);
    pthread_mutex_lock (& alsa_mutex);
    pump_quit = 0;
}

static void start_playback (void)
{
    AUDDBG ("Starting playback.\n");
    CHECK (snd_pcm_prepare, alsa_handle);

FAILED:
    alsa_prebuffer = 0;
    pthread_cond_broadcast (& alsa_cond);
}

static int get_delay (void)
{
    snd_pcm_sframes_t delay = 0;

    CHECK_RECOVER (snd_pcm_delay, alsa_handle, & delay);

FAILED:
    return delay;
}

int alsa_init (void)
{
    AUDDBG ("Initialize.\n");
    alsa_config_load ();
    alsa_open_mixer ();
    return 1;
}

void alsa_cleanup (void)
{
    AUDDBG ("Cleanup.\n");
    alsa_close_mixer ();
    alsa_config_save ();
}

static int convert_aud_format (int aud_format)
{
    const struct
    {
        int aud_format, format;
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

    for (int count = 0; count < ARRAY_LEN (table); count ++)
    {
        if (table[count].aud_format == aud_format)
            return table[count].format;
    }

    return SND_PCM_FORMAT_UNKNOWN;
}

int alsa_open_audio (int aud_format, int rate, int channels)
{
    pthread_mutex_lock (& alsa_mutex);

    assert (alsa_handle == NULL);

    int format = convert_aud_format (aud_format);
    AUDDBG ("Opening PCM device %s for %s, %d channels, %d Hz.\n",
     alsa_config_pcm, snd_pcm_format_name (format), channels, rate);
    CHECK_NOISY (snd_pcm_open, & alsa_handle, alsa_config_pcm,
     SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_t * params;
    snd_pcm_hw_params_alloca (& params);
    CHECK_NOISY (snd_pcm_hw_params_any, alsa_handle, params);
    CHECK_NOISY (snd_pcm_hw_params_set_access, alsa_handle, params,
     SND_PCM_ACCESS_RW_INTERLEAVED);

    CHECK_NOISY (snd_pcm_hw_params_set_format, alsa_handle, params, format);
    CHECK_NOISY (snd_pcm_hw_params_set_channels, alsa_handle, params, channels);
    CHECK_NOISY (snd_pcm_hw_params_set_rate, alsa_handle, params, rate, 0);

    alsa_format = format;
    alsa_channels = channels;
    alsa_rate = rate;

    int total_buffer = aud_get_int (NULL, "output_buffer_size");
    unsigned int useconds = 1000 * MIN (1000, total_buffer / 2);
    int direction = 0;
    CHECK_NOISY (snd_pcm_hw_params_set_buffer_time_near, alsa_handle, params,
     & useconds, & direction);
    int hard_buffer = useconds / 1000;

    useconds = 1000 * (hard_buffer / 4);
    direction = 0;
    CHECK_NOISY (snd_pcm_hw_params_set_period_time_near, alsa_handle, params,
     & useconds, & direction);
    alsa_period = useconds / 1000;

    CHECK_NOISY (snd_pcm_hw_params, alsa_handle, params);

    int soft_buffer = MAX (total_buffer / 2, total_buffer - hard_buffer);
    AUDDBG ("Buffer: hardware %d ms, software %d ms, period %d ms.\n",
     hard_buffer, soft_buffer, alsa_period);

    alsa_buffer_length = snd_pcm_frames_to_bytes (alsa_handle, (int64_t)
     soft_buffer * rate / 1000);
    alsa_buffer = g_malloc (alsa_buffer_length);
    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    alsa_written = 0;
    alsa_prebuffer = 1;
    alsa_paused = 0;
    alsa_paused_delay = 0;

    if (! poll_setup ())
        goto FAILED;

    pump_start ();

    pthread_mutex_unlock (& alsa_mutex);
    return 1;

FAILED:
    if (alsa_handle != NULL)
    {
        snd_pcm_close (alsa_handle);
        alsa_handle = NULL;
    }

    pthread_mutex_unlock (& alsa_mutex);
    return 0;
}

void alsa_close_audio (void)
{
    AUDDBG ("Closing audio.\n");
    pthread_mutex_lock (& alsa_mutex);

    assert (alsa_handle != NULL);

    pump_stop ();
    CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    g_free (alsa_buffer);
    poll_cleanup ();
    snd_pcm_close (alsa_handle);
    alsa_handle = NULL;

    pthread_mutex_unlock (& alsa_mutex);
}

int alsa_buffer_free (void)
{
    pthread_mutex_lock (& alsa_mutex);
    int avail = alsa_buffer_length - alsa_buffer_data_length;
    pthread_mutex_unlock (& alsa_mutex);
    return avail;
}

void alsa_write_audio (void * data, int length)
{
    pthread_mutex_lock (& alsa_mutex);

    int start = (alsa_buffer_data_start + alsa_buffer_data_length) %
     alsa_buffer_length;

    assert (length <= alsa_buffer_length - alsa_buffer_data_length);

    if (length > alsa_buffer_length - start)
    {
        int part = alsa_buffer_length - start;

        memcpy ((char *) alsa_buffer + start, data, part);
        memcpy (alsa_buffer, (char *) data + part, length - part);
    }
    else
        memcpy ((char *) alsa_buffer + start, data, length);

    alsa_buffer_data_length += length;
    alsa_written += snd_pcm_bytes_to_frames (alsa_handle, length);

    if (! alsa_paused)
        pthread_cond_broadcast (& alsa_cond);

    pthread_mutex_unlock (& alsa_mutex);
}

void alsa_period_wait (void)
{
    pthread_mutex_lock (& alsa_mutex);

    while (alsa_buffer_data_length == alsa_buffer_length)
    {
        if (! alsa_paused)
        {
            if (alsa_prebuffer)
                start_playback ();
            else
                pthread_cond_broadcast (& alsa_cond);
        }

        pthread_cond_wait (& alsa_cond, & alsa_mutex);
    }

    pthread_mutex_unlock (& alsa_mutex);
}

void alsa_drain (void)
{
    AUDDBG ("Drain.\n");
    pthread_mutex_lock (& alsa_mutex);

    assert (! alsa_paused);

    if (alsa_prebuffer)
        start_playback ();

    while (snd_pcm_bytes_to_frames (alsa_handle, alsa_buffer_data_length))
        pthread_cond_wait (& alsa_cond, & alsa_mutex);

    pump_stop ();

    if (alsa_config_drain_workaround)
    {
        int d = get_delay () * 1000 / alsa_rate;
        struct timespec delay = {.tv_sec = d / 1000, .tv_nsec = d % 1000 *
         1000000};

        pthread_mutex_unlock (& alsa_mutex);
        nanosleep (& delay, NULL);
        pthread_mutex_lock (& alsa_mutex);
    }
    else
    {
        while (1)
        {
            int state;
            CHECK_VAL (state, snd_pcm_state, alsa_handle);

            if (state != SND_PCM_STATE_RUNNING && state !=
             SND_PCM_STATE_DRAINING)
                break;

            pthread_mutex_unlock (& alsa_mutex);
            poll_sleep ();
            pthread_mutex_lock (& alsa_mutex);
        }
    }

    pump_start ();

FAILED:
    pthread_mutex_unlock (& alsa_mutex);
    return;
}

int alsa_output_time (void)
{
    pthread_mutex_lock (& alsa_mutex);

    int64_t frames = alsa_written - snd_pcm_bytes_to_frames (alsa_handle,
     alsa_buffer_data_length);

    if (alsa_prebuffer || alsa_paused)
        frames -= alsa_paused_delay;
    else
        frames -= get_delay ();

    int time = frames * 1000 / alsa_rate;

    pthread_mutex_unlock (& alsa_mutex);
    return time;
}

void alsa_flush (int time)
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& alsa_mutex);

    pump_stop ();
    CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    alsa_written = (int64_t) time * alsa_rate / 1000;
    alsa_prebuffer = 1;
    alsa_paused_delay = 0;

    pthread_cond_broadcast (& alsa_cond); /* interrupt period wait */

    pump_start ();

    pthread_mutex_unlock (& alsa_mutex);
}

void alsa_pause (int pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    pthread_mutex_lock (& alsa_mutex);

    alsa_paused = pause;

    if (! alsa_prebuffer)
    {
        if (pause)
            alsa_paused_delay = get_delay ();

        CHECK (snd_pcm_pause, alsa_handle, pause);
    }

DONE:
    if (! pause)
        pthread_cond_broadcast (& alsa_cond);

    pthread_mutex_unlock (& alsa_mutex);
    return;

FAILED:
    AUDDBG ("Trying to work around broken pause.\n");

    if (pause)
        snd_pcm_drop (alsa_handle);
    else
        snd_pcm_prepare (alsa_handle);

    goto DONE;
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

void alsa_get_volume (int * left, int * right)
{
    pthread_mutex_lock (& alsa_mutex);

    long left_l = 0, right_l = 0;

    if (alsa_mixer == NULL)
        goto FAILED;

    CHECK (snd_mixer_handle_events, alsa_mixer);

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_element))
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_MONO, & left_l);
        right_l = left_l;

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
        {
            int on = 0;
            CHECK (snd_mixer_selem_get_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_MONO, & on);

            if (! on)
                left_l = right_l = 0;
        }
    }
    else
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_LEFT, & left_l);
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_RIGHT, & right_l);

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
        {
            int left_on = 0, right_on = 0;
            CHECK (snd_mixer_selem_get_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_FRONT_LEFT, & left_on);
            CHECK (snd_mixer_selem_get_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_FRONT_RIGHT, & right_on);

            if (! left_on)
                left_l = 0;
            if (! right_on)
                right_l = 0;
        }
    }

FAILED:
    pthread_mutex_unlock (& alsa_mutex);

    * left = left_l;
    * right = right_l;
}

void alsa_set_volume (int left, int right)
{
    pthread_mutex_lock (& alsa_mutex);

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
    pthread_mutex_unlock (& alsa_mutex);
}
