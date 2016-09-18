/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2014 John Lindgren
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

#include <alsa/asoundlib.h>
#include <libaudcore/ringbuf.h>

#include "alsa.h"

EXPORT ALSAPlugin aud_plugin_instance;

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

static RingBuf<char> alsa_buffer;
static int alsa_period; /* milliseconds */

static bool alsa_prebuffer, alsa_paused;
static int alsa_paused_delay; /* milliseconds */

static int poll_pipe[2];
static int poll_count;
static pollfd * poll_handles;

static bool pump_quit;
static pthread_t pump_thread;

static snd_mixer_t * alsa_mixer;
static snd_mixer_elem_t * alsa_mixer_element;

static bool poll_setup ()
{
    if (pipe (poll_pipe))
    {
        AUDERR ("Failed to create pipe: %s.\n", strerror (errno));
        return false;
    }

    if (fcntl (poll_pipe[0], F_SETFL, O_NONBLOCK))
    {
        AUDERR ("Failed to set O_NONBLOCK on pipe: %s.\n", strerror (errno));
        close (poll_pipe[0]);
        close (poll_pipe[1]);
        return false;
    }

    poll_count = 1 + snd_pcm_poll_descriptors_count (alsa_handle);
    poll_handles = new pollfd[poll_count];
    poll_handles[0].fd = poll_pipe[0];
    poll_handles[0].events = POLLIN;
    poll_count = 1 + snd_pcm_poll_descriptors (alsa_handle, poll_handles + 1,
     poll_count - 1);

    return true;
}

static void poll_sleep ()
{
    if (poll (poll_handles, poll_count, -1) < 0)
    {
        AUDERR ("Failed to poll: %s.\n", strerror (errno));
        return;
    }

    if (poll_handles[0].revents & POLLIN)
    {
        char c;
        while (read (poll_pipe[0], & c, 1) == 1)
            ;
    }
}

static void poll_wake ()
{
    const char c = 0;
    if (write (poll_pipe[1], & c, 1) < 0)
        AUDERR ("Failed to write to pipe: %s.\n", strerror (errno));
}

static void poll_cleanup ()
{
    close (poll_pipe[0]);
    close (poll_pipe[1]);
    delete[] poll_handles;
}

static void * pump (void *)
{
    pthread_mutex_lock (& alsa_mutex);
    pthread_cond_broadcast (& alsa_cond); /* signal thread started */

    bool failed_once = false;
    bool use_timed_wait = false;
    int wakeups_since_write = 0;

    while (! pump_quit)
    {
        int writable = snd_pcm_bytes_to_frames (alsa_handle, alsa_buffer.linear ());

        if (alsa_prebuffer || alsa_paused || ! writable)
        {
            pthread_cond_wait (& alsa_cond, & alsa_mutex);
            continue;
        }

        int avail;
        CHECK_VAL_RECOVER (avail, snd_pcm_avail_update, alsa_handle);

        if (avail)
        {
            wakeups_since_write = 0;

            int written;
            CHECK_VAL_RECOVER (written, snd_pcm_writei, alsa_handle,
             & alsa_buffer[0], aud::min (writable, avail));

            failed_once = false;

            alsa_buffer.discard (snd_pcm_frames_to_bytes (alsa_handle, written));

            pthread_cond_broadcast (& alsa_cond); /* signal write complete */

            if (writable < avail)
                continue;
        }

        pthread_mutex_unlock (& alsa_mutex);

        if (wakeups_since_write > 4)
        {
            AUDDBG ("Activating timer workaround.\n");
            use_timed_wait = true;
        }

        if (use_timed_wait && wakeups_since_write)
        {
            timespec delay = {0, 600000 * alsa_period};
            nanosleep (& delay, nullptr);
        }
        else
        {
            poll_sleep ();
            wakeups_since_write ++;
        }

        pthread_mutex_lock (& alsa_mutex);
        continue;

    FAILED:
        if (failed_once)
            break;

        failed_once = true;
        CHECK (snd_pcm_prepare, alsa_handle);
    }

    pthread_mutex_unlock (& alsa_mutex);
    return nullptr;
}

static void pump_start ()
{
    AUDDBG ("Starting pump.\n");
    pthread_create (& pump_thread, nullptr, pump, nullptr);
    pthread_cond_wait (& alsa_cond, & alsa_mutex);
}

static void pump_stop ()
{
    AUDDBG ("Stopping pump.\n");
    pump_quit = true;
    pthread_cond_broadcast (& alsa_cond);
    poll_wake ();
    pthread_mutex_unlock (& alsa_mutex);
    pthread_join (pump_thread, nullptr);
    pthread_mutex_lock (& alsa_mutex);
    pump_quit = false;
}

static void start_playback ()
{
    AUDDBG ("Starting playback.\n");
    CHECK (snd_pcm_prepare, alsa_handle);

FAILED:
    alsa_prebuffer = false;
    pthread_cond_broadcast (& alsa_cond);
}

static int get_delay_locked ()
{
    snd_pcm_sframes_t delay = 0;
    CHECK_RECOVER (snd_pcm_delay, alsa_handle, & delay);

FAILED:
    return aud::rescale ((int) delay, alsa_rate, 1000);
}

bool ALSAPlugin::init ()
{
    AUDDBG ("Initialize.\n");
    init_config ();
    open_mixer ();
    return true;
}

void ALSAPlugin::cleanup ()
{
    AUDDBG ("Cleanup.\n");
    close_mixer ();
}

static snd_pcm_format_t convert_aud_format (int aud_format)
{
    static const struct
    {
        int aud_format;
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
        {FMT_S24_3LE, SND_PCM_FORMAT_S24_3LE},
        {FMT_S24_3BE, SND_PCM_FORMAT_S24_3BE},
        {FMT_U24_3LE, SND_PCM_FORMAT_U24_3LE},
        {FMT_U24_3BE, SND_PCM_FORMAT_U24_3BE},
    };

    for (auto & conv : table)
    {
        if (conv.aud_format == aud_format)
            return conv.format;
    }

    return SND_PCM_FORMAT_UNKNOWN;
}

bool ALSAPlugin::open_audio (int aud_format, int rate, int channels, String & error)
{
    int total_buffer, hard_buffer, soft_buffer, buffer_frames;
    unsigned useconds;
    int direction;

    pthread_mutex_lock (& alsa_mutex);

    assert (! alsa_handle);

    String pcm = aud_get_str ("alsa", "pcm");
    snd_pcm_format_t format = convert_aud_format (aud_format);

    if (format == SND_PCM_FORMAT_UNKNOWN)
    {
        error = String ("Unsupported audio format");
        goto FAILED;
    }

    AUDINFO ("Opening PCM device %s for %s, %d channels, %d Hz.\n",
     (const char *) pcm, snd_pcm_format_name (format), channels, rate);
    CHECK_STR (error, snd_pcm_open, & alsa_handle, pcm, SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_t * params;
    snd_pcm_hw_params_alloca (& params);
    CHECK_STR (error, snd_pcm_hw_params_any, alsa_handle, params);
    CHECK_STR (error, snd_pcm_hw_params_set_access, alsa_handle, params,
     SND_PCM_ACCESS_RW_INTERLEAVED);

    CHECK_STR (error, snd_pcm_hw_params_set_format, alsa_handle, params, format);
    CHECK_STR (error, snd_pcm_hw_params_set_channels, alsa_handle, params, channels);
    CHECK_STR (error, snd_pcm_hw_params_set_rate, alsa_handle, params, rate, 0);

    alsa_format = format;
    alsa_channels = channels;
    alsa_rate = rate;

    total_buffer = aud_get_int (nullptr, "output_buffer_size");
    useconds = 1000 * aud::min (1000, total_buffer / 2);
    direction = 0;
    CHECK_STR (error, snd_pcm_hw_params_set_buffer_time_near, alsa_handle,
     params, & useconds, & direction);
    hard_buffer = useconds / 1000;

    useconds = 1000 * (hard_buffer / 4);
    direction = 0;
    CHECK_STR (error, snd_pcm_hw_params_set_period_time_near, alsa_handle,
     params, & useconds, & direction);
    alsa_period = useconds / 1000;

    CHECK_STR (error, snd_pcm_hw_params, alsa_handle, params);

    soft_buffer = aud::max (total_buffer / 2, total_buffer - hard_buffer);
    AUDINFO ("Buffer: hardware %d ms, software %d ms, period %d ms.\n",
     hard_buffer, soft_buffer, alsa_period);

    buffer_frames = aud::rescale<int64_t> (soft_buffer, 1000, rate);
    alsa_buffer.alloc (snd_pcm_frames_to_bytes (alsa_handle, buffer_frames));

    alsa_prebuffer = true;
    alsa_paused = false;
    alsa_paused_delay = 0;

    if (! poll_setup ())
        goto FAILED;

    pump_start ();

    pthread_mutex_unlock (& alsa_mutex);
    return true;

FAILED:
    if (alsa_handle)
    {
        snd_pcm_close (alsa_handle);
        alsa_handle = nullptr;
    }

    pthread_mutex_unlock (& alsa_mutex);
    return false;
}

void ALSAPlugin::close_audio ()
{
    AUDDBG ("Closing audio.\n");
    pthread_mutex_lock (& alsa_mutex);

    assert (alsa_handle);

    pump_stop ();
    CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    alsa_buffer.destroy ();
    poll_cleanup ();
    snd_pcm_close (alsa_handle);
    alsa_handle = nullptr;

    pthread_mutex_unlock (& alsa_mutex);
}

int ALSAPlugin::write_audio (const void * data, int length)
{
    pthread_mutex_lock (& alsa_mutex);

    length = aud::min (length, alsa_buffer.space ());
    alsa_buffer.copy_in ((const char *) data, length);

    if (! alsa_paused)
        pthread_cond_broadcast (& alsa_cond);

    pthread_mutex_unlock (& alsa_mutex);
    return length;
}

void ALSAPlugin::period_wait ()
{
    pthread_mutex_lock (& alsa_mutex);

    while (! alsa_buffer.space ())
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

void ALSAPlugin::drain ()
{
    AUDDBG ("Drain.\n");
    pthread_mutex_lock (& alsa_mutex);

    assert (! alsa_paused);

    if (alsa_prebuffer)
        start_playback ();

    while (snd_pcm_bytes_to_frames (alsa_handle, alsa_buffer.len ()))
        pthread_cond_wait (& alsa_cond, & alsa_mutex);

    pump_stop ();

    int d = get_delay_locked ();
    timespec delay = {d / 1000, d % 1000 * 1000000};

    pthread_mutex_unlock (& alsa_mutex);
    nanosleep (& delay, nullptr);
    pthread_mutex_lock (& alsa_mutex);

    pump_start ();

    pthread_mutex_unlock (& alsa_mutex);
}

int ALSAPlugin::get_delay ()
{
    pthread_mutex_lock (& alsa_mutex);

    int buffered = snd_pcm_bytes_to_frames (alsa_handle, alsa_buffer.len ());
    int delay = aud::rescale (buffered, alsa_rate, 1000);

    if (alsa_prebuffer || alsa_paused)
        delay += alsa_paused_delay;
    else
        delay += get_delay_locked ();

    pthread_mutex_unlock (& alsa_mutex);
    return delay;
}

void ALSAPlugin::flush ()
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& alsa_mutex);

    pump_stop ();
    CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    alsa_buffer.discard ();

    alsa_prebuffer = true;
    alsa_paused_delay = 0;

    pthread_cond_broadcast (& alsa_cond); /* interrupt period wait */

    pump_start ();

    pthread_mutex_unlock (& alsa_mutex);
}

void ALSAPlugin::pause (bool pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    pthread_mutex_lock (& alsa_mutex);

    alsa_paused = pause;

    if (! alsa_prebuffer)
    {
        if (pause)
            alsa_paused_delay = get_delay_locked ();

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

void ALSAPlugin::open_mixer ()
{
    alsa_mixer = nullptr;

    String mixer = aud_get_str ("alsa", "mixer");
    String mixer_element = aud_get_str ("alsa", "mixer-element");

    if (! mixer_element[0])
        goto FAILED;

    AUDDBG ("Opening mixer card %s.\n", (const char *) mixer);
    CHECK (snd_mixer_open, & alsa_mixer, 0);
    CHECK (snd_mixer_attach, alsa_mixer, mixer);
    CHECK (snd_mixer_selem_register, alsa_mixer, nullptr, nullptr);
    CHECK (snd_mixer_load, alsa_mixer);

    snd_mixer_selem_id_t * selem_id;
    snd_mixer_selem_id_alloca (& selem_id);
    snd_mixer_selem_id_set_name (selem_id, mixer_element);
    alsa_mixer_element = snd_mixer_find_selem (alsa_mixer, selem_id);

    if (! alsa_mixer_element)
    {
        AUDERR ("snd_mixer_find_selem failed.\n");
        goto FAILED;
    }

    CHECK (snd_mixer_selem_set_playback_volume_range, alsa_mixer_element, 0, 100);
    return;

FAILED:
    if (alsa_mixer)
    {
        snd_mixer_close (alsa_mixer);
        alsa_mixer = nullptr;
    }
}

void ALSAPlugin::close_mixer ()
{
    if (alsa_mixer)
        snd_mixer_close (alsa_mixer);
}

StereoVolume ALSAPlugin::get_volume ()
{
    pthread_mutex_lock (& alsa_mutex);

    long left = 0, right = 0;

    if (! alsa_mixer)
        goto FAILED;

    CHECK (snd_mixer_handle_events, alsa_mixer);

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_element))
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_MONO, & left);
        right = left;

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
        {
            int on = 0;
            CHECK (snd_mixer_selem_get_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_MONO, & on);

            if (! on)
                left = right = 0;
        }
    }
    else
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_LEFT, & left);
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_RIGHT, & right);

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
        {
            int left_on = 0, right_on = 0;
            CHECK (snd_mixer_selem_get_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_FRONT_LEFT, & left_on);
            CHECK (snd_mixer_selem_get_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_FRONT_RIGHT, & right_on);

            if (! left_on)
                left = 0;
            if (! right_on)
                right = 0;
        }
    }

FAILED:
    pthread_mutex_unlock (& alsa_mutex);
    return {(int) left, (int) right};
}

void ALSAPlugin::set_volume (StereoVolume v)
{
    pthread_mutex_lock (& alsa_mutex);

    if (! alsa_mixer)
        goto FAILED;

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_element))
    {
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_MONO, aud::max (v.left, v.right));

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
            CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_MONO, aud::max (v.left, v.right) != 0);
    }
    else
    {
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_LEFT, v.left);
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_RIGHT, v.right);

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
        {
            if (snd_mixer_selem_has_playback_switch_joined (alsa_mixer_element))
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_LEFT, aud::max (v.left, v.right) != 0);
            else
            {
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_LEFT, v.left != 0);
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_RIGHT, v.right != 0);
            }
        }
    }

    CHECK (snd_mixer_handle_events, alsa_mixer);

FAILED:
    pthread_mutex_unlock (& alsa_mutex);
}
