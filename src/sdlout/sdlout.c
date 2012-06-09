/*
 * SDL Output Plugin for Audacious
 * Copyright 2010 John Lindgren
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

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <SDL.h>
#include <SDL_audio.h>

#include <audacious/debug.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "sdlout.h"

#define VOLUME_RANGE 40 /* decibels */

#define sdlout_error(...) do { \
    SPRINTF (sdlout_error_buf, "SDL error: " __VA_ARGS__); \
    aud_interface_show_error (sdlout_error_buf); \
} while (0)

static pthread_mutex_t sdlout_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sdlout_cond = PTHREAD_COND_INITIALIZER;

static volatile int vol_left = 100, vol_right = 100;

static int sdlout_chan, sdlout_rate;

static unsigned char * buffer;
static int buffer_size, buffer_data_start, buffer_data_len;

static int64_t frames_written;
static char prebuffer_flag, paused_flag;

static int block_delay;
static struct timeval block_time;

int sdlout_init (void)
{
    if (SDL_Init (SDL_INIT_AUDIO) < 0)
    {
        fprintf (stderr, "Failed to init SDL: %s.\n", SDL_GetError ());
        return 0;
    }

    return 1;
}

void sdlout_cleanup (void)
{
    SDL_Quit ();
}

void sdlout_get_volume (int * left, int * right)
{
    * left = vol_left;
    * right = vol_right;
}

void sdlout_set_volume (int left, int right)
{
    vol_left = left;
    vol_right = right;
}

static void apply_mono_volume (unsigned char * data, int len)
{
    int vol = MAX (vol_left, vol_right);
    int factor = (vol == 0) ? 0 : powf (10, (float) VOLUME_RANGE * (vol - 100)
     / 100 / 20) * 65536;

    int16_t * i = (int16_t *) data;
    int16_t * end = (int16_t *) (data + len);

    while (i < end)
    {
        * i = ((int) * i * factor) >> 16;
        i ++;
    }
}

static void apply_stereo_volume (unsigned char * data, int len)
{
    int factor_left = (vol_left == 0) ? 0 : powf (10, (float) VOLUME_RANGE *
     (vol_left - 100) / 100 / 20) * 65536;
    int factor_right = (vol_right == 0) ? 0 : powf (10, (float) VOLUME_RANGE *
     (vol_right - 100) / 100 / 20) * 65536;

    int16_t * i = (int16_t *) data;
    int16_t * end = (int16_t *) (data + len);

    while (i < end)
    {
        * i = ((int) * i * factor_left) >> 16;
        i ++;
        * i = ((int) * i * factor_right) >> 16;
        i ++;
    }
}

static void callback (void * user, unsigned char * buf, int len)
{
    pthread_mutex_lock (& sdlout_mutex);

    int copy = MIN (len, buffer_data_len);
    int part = buffer_size - buffer_data_start;

    if (copy <= part)
    {
        memcpy (buf, buffer + buffer_data_start, copy);
        buffer_data_start += copy;
    }
    else
    {
        memcpy (buf, buffer + buffer_data_start, part);
        memcpy (buf + part, buffer, copy - part);
        buffer_data_start = copy - part;
    }

    buffer_data_len -= copy;

    if (sdlout_chan == 2)
        apply_stereo_volume (buf, copy);
    else
        apply_mono_volume (buf, copy);

    if (copy < len)
        memset (buf + copy, 0, len - copy);

    /* At this moment, we know that there is a delay of (at least) the block of
     * data just written.  We save the block size and the current time for
     * estimating the delay later on. */
    block_delay = copy / (2 * sdlout_chan) * 1000 / sdlout_rate;
    gettimeofday (& block_time, NULL);

    pthread_cond_broadcast (& sdlout_cond);
    pthread_mutex_unlock (& sdlout_mutex);
}

int sdlout_open_audio (int format, int rate, int chan)
{
    if (format != FMT_S16_NE)
    {
        sdlout_error ("Only signed 16-bit, native endian audio is supported.\n");
        return 0;
    }

    AUDDBG ("Opening audio for %d channels, %d Hz.\n", chan, rate);

    sdlout_chan = chan;
    sdlout_rate = rate;

    buffer_size = 2 * chan * (aud_get_int (NULL, "output_buffer_size") * rate / 1000);
    buffer = malloc (buffer_size);
    buffer_data_start = 0;
    buffer_data_len = 0;

    frames_written = 0;
    prebuffer_flag = 1;
    paused_flag = 0;

    SDL_AudioSpec spec = {
     .freq = rate,
     .format = AUDIO_S16,
     .channels = chan,
     .samples = 4096,
     .callback = callback,
     .userdata = NULL};

    if (SDL_OpenAudio (& spec, NULL) < 0)
    {
        sdlout_error ("Failed to open audio stream: %s.\n", SDL_GetError ());
        free (buffer);
        buffer = NULL;
        return 0;
    }

    return 1;
}

void sdlout_close_audio (void)
{
    AUDDBG ("Closing audio.\n");
    SDL_CloseAudio ();
    free (buffer);
    buffer = NULL;
}

int sdlout_buffer_free (void)
{
    pthread_mutex_lock (& sdlout_mutex);
    int space = buffer_size - buffer_data_len;
    pthread_mutex_unlock (& sdlout_mutex);
    return space;
}

static void check_started (void)
{
    if (! prebuffer_flag)
        return;

    AUDDBG ("Starting playback.\n");
    prebuffer_flag = 0;
    block_delay = 0;
    SDL_PauseAudio (0);
}

void sdlout_period_wait (void)
{
    pthread_mutex_lock (& sdlout_mutex);

    while (buffer_data_len == buffer_size)
    {
        if (! paused_flag)
            check_started ();

        pthread_cond_wait (& sdlout_cond, & sdlout_mutex);
    }

    pthread_mutex_unlock (& sdlout_mutex);
}

void sdlout_write_audio (void * data, int len)
{
    pthread_mutex_lock (& sdlout_mutex);

    assert (len <= buffer_size - buffer_data_len);

    int start = (buffer_data_start + buffer_data_len) % buffer_size;

    if (len <= buffer_size - start)
        memcpy (buffer + start, data, len);
    else
    {
        int part = buffer_size - start;
        memcpy (buffer + start, data, part);
        memcpy (buffer, (char *) data + part, len - part);
    }

    buffer_data_len += len;
    frames_written += len / (2 * sdlout_chan);

    pthread_mutex_unlock (& sdlout_mutex);
}

void sdlout_drain (void)
{
    AUDDBG ("Draining.\n");
    pthread_mutex_lock (& sdlout_mutex);

    check_started ();

    while (buffer_data_len)
        pthread_cond_wait (& sdlout_cond, & sdlout_mutex);

    pthread_mutex_unlock (& sdlout_mutex);
}

int sdlout_output_time (void)
{
    pthread_mutex_lock (& sdlout_mutex);

    int out = (int64_t) (frames_written - buffer_data_len / (2 * sdlout_chan))
     * 1000 / sdlout_rate;

    /* Estimate the additional delay of the last block written. */
    if (! prebuffer_flag && ! paused_flag && block_delay)
    {
        struct timeval cur;
        gettimeofday (& cur, NULL);

        int elapsed = 1000 * (cur.tv_sec - block_time.tv_sec) + (cur.tv_usec -
         block_time.tv_usec) / 1000;

        if (elapsed < block_delay)
            out -= block_delay - elapsed;
    }

    pthread_mutex_unlock (& sdlout_mutex);
    return out;
}

void sdlout_pause (int pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    pthread_mutex_lock (& sdlout_mutex);

    paused_flag = pause;

    if (! prebuffer_flag)
        SDL_PauseAudio (pause);

    pthread_cond_broadcast (& sdlout_cond); /* wake up period wait */
    pthread_mutex_unlock (& sdlout_mutex);
}

void sdlout_flush (int time)
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& sdlout_mutex);

    buffer_data_start = 0;
    buffer_data_len = 0;

    frames_written = (int64_t) time * sdlout_rate / 1000;
    prebuffer_flag = 1;

    pthread_cond_broadcast (& sdlout_cond); /* wake up period wait */
    pthread_mutex_unlock (& sdlout_mutex);
}
