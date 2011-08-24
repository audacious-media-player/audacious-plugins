/*
 * Crossfade Plugin for Audacious
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

#include <glib.h>
#include <string.h>

#include <audacious/debug.h>
#include <audacious/plugin.h>

#include "crossfade.h"

enum
{
    STATE_OFF,
    STATE_PREBUFFER,
    STATE_RUNNING,
    STATE_BETWEEN,
    STATE_STOPPING,
};

int crossfade_length; /* seconds */

static char state = STATE_OFF;
static int current_channels = 0, current_rate = 0;
static float * buffer = NULL;
static int buffer_size = 0, buffer_filled = 0;
static int prebuffer_filled = 0;
static float * output = NULL;
static int output_size = 0;

static void reset (void)
{
    AUDDBG ("Reset.\n");
    state = STATE_OFF;
    current_channels = 0;
    current_rate = 0;
    g_free (buffer);
    buffer = NULL;
    buffer_size = 0;
    buffer_filled = 0;
    prebuffer_filled = 0;
    g_free (output);
    output = NULL;
    output_size = 0;
}

int crossfade_init (void)
{
    AUDDBG ("Init.\n");
    crossfade_config_load ();
    return 1;
}

void crossfade_cleanup (void)
{
    AUDDBG ("Cleanup.\n");
    crossfade_config_save ();
    reset ();
}

static int message_cb (void * data)
{
    void (* message) (void) = data;

    message ();
    return 0;
}

void crossfade_start (int * channels, int * rate)
{
    AUDDBG ("Start (state was %d).\n", state);

    if (state != STATE_BETWEEN)
        reset ();
    else if (* channels != current_channels)
    {
        g_timeout_add (0, message_cb, crossfade_show_channels_message);
        reset ();
    }
    else if (* rate != current_rate)
    {
        g_timeout_add (0, message_cb, crossfade_show_rate_message);
        reset ();
    }

    state = STATE_PREBUFFER;
    current_channels = * channels;
    current_rate = * rate;
    prebuffer_filled = 0;
}

static void do_ramp (float * data, int length, float a, float b)
{
    int count;

    for (count = 0; count < length; count ++)
    {
        * data = (* data) * (a * (length - count) + b * count) / length;
        data ++;
    }
}

static void mix (float * data, float * new, int length)
{
    while (length --)
        (* data ++) += (* new ++);
}

static void enlarge_buffer (int length)
{
    if (length > buffer_size)
    {
        buffer = g_realloc (buffer, sizeof (float) * length);
        buffer_size = length;
    }
}

static void add_data (float * data, int length)
{
    if (state == STATE_PREBUFFER)
    {
        int full = current_channels * current_rate * crossfade_length;

        if (prebuffer_filled < full)
        {
            int copy = MIN (length, full - prebuffer_filled);
            float a = (float) prebuffer_filled / full;
            float b = (float) (prebuffer_filled + copy) / full;

            if (prebuffer_filled + copy > buffer_filled)
            {
                enlarge_buffer (prebuffer_filled + copy);
                memset (buffer + buffer_filled, 0, sizeof (float) *
                 (prebuffer_filled + copy - buffer_filled));
                buffer_filled = prebuffer_filled + copy;
            }

            do_ramp (data, copy, a, b);
            mix (buffer + prebuffer_filled, data, copy);
            prebuffer_filled += copy;
            data += copy;
            length -= copy;
        }

        if (prebuffer_filled < full)
            return;

        if (prebuffer_filled < buffer_filled)
        {
            int copy = MIN (length, buffer_filled - prebuffer_filled);

            mix (buffer + prebuffer_filled, data, copy);
            prebuffer_filled += copy;
            data += copy;
            length -= copy;
        }

        if (prebuffer_filled < buffer_filled)
            return;

        AUDDBG ("Prebuffer complete.\n");
        state = STATE_RUNNING;
    }

    if (state != STATE_RUNNING)
        return;

    enlarge_buffer (buffer_filled + length);
    memcpy (buffer + buffer_filled, data, sizeof (float) * length);
    buffer_filled += length;
}

static void enlarge_output (int length)
{
    if (length > output_size)
    {
        output = g_realloc (output, sizeof (float) * length);
        output_size = length;
    }
}

static void return_data (float * * data, int * length)
{
    int full = current_channels * current_rate * crossfade_length;
    int copy = buffer_filled - full;

    /* only return if we have at least 1/2 second -- this reduces memmove's */
    if (state != STATE_RUNNING || copy < current_channels * (current_rate / 2))
    {
        * data = NULL;
        * length = 0;
        return;
    }

    enlarge_output (copy);
    memcpy (output, buffer, sizeof (float) * copy);
    buffer_filled -= copy;
    memmove (buffer, buffer + copy, sizeof (float) * buffer_filled);
    * data = output;
    * length = copy;
}

void crossfade_process (float * * data, int * samples)
{
    add_data (* data, * samples);
    return_data (data, samples);
}

void crossfade_flush (void)
{
    AUDDBG ("Flush.\n");

    if (state == STATE_PREBUFFER || state == STATE_RUNNING)
    {
        state = STATE_RUNNING;
        buffer_filled = 0;
    }
}

void crossfade_finish (float * * data, int * samples)
{
    if (state == STATE_BETWEEN) /* second call, end of last song */
    {
        AUDDBG ("End of last song.\n");
        enlarge_output (buffer_filled);
        memcpy (output, buffer, sizeof (float) * buffer_filled);
        * data = output;
        * samples = buffer_filled;
        buffer_filled = 0;
        state = STATE_OFF;
        return;
    }

    add_data (* data, * samples);
    return_data (data, samples);

    if (state == STATE_PREBUFFER || state == STATE_RUNNING)
    {
        AUDDBG ("Fade out.\n");
        do_ramp (buffer, buffer_filled, 1.0, 0.0);
        state = STATE_BETWEEN;
    }
}

int crossfade_decoder_to_output_time (int time)
{
    return time;
}

int crossfade_output_to_decoder_time (int time)
{
    return time;
}
