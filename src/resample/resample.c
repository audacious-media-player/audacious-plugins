/*
 * Sample Rate Converter Plugin for Audacious
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

#include <stdlib.h>

#include "resample.h"

static SRC_STATE * state = NULL;
static int stored_channels;
static double ratio;
static float * buffer = NULL;
static int buffer_samples = 0;

int resample_init (void)
{
    resample_config_load ();
    return 1;
}

void resample_cleanup (void)
{
    if (state != NULL)
    {
        src_delete (state);
        state = NULL;
    }

    free (buffer);
    buffer = NULL;
    buffer_samples = 0;

    resample_config_save ();
}

void resample_start (int * channels, int * rate)
{
    int count, new_rate = fallback_rate, error;

    if (state != NULL)
    {
        src_delete (state);
        state = NULL;
    }

    for (count = 0; count < n_common_rates; count ++)
    {
        if (common_rates[count] == * rate)
        {
            new_rate = converted_rates[count];
            break;
        }
    }

    if (new_rate == * rate)
        return;

    if ((state = src_new (method, * channels, & error)) == NULL)
    {
        RESAMPLE_ERROR (error);
        return;
    }

    stored_channels = * channels;
    ratio = (double) new_rate / * rate;
    * rate = new_rate;
}

void do_resample (float * * data, int * samples, char finish)
{
    SRC_DATA d;
    int error;

    if (state == NULL || ! * samples)
        return;

    if (buffer_samples < (int) (* samples * ratio) + 256)
    {
        buffer_samples = (int) (* samples * ratio) + 256;
        buffer = realloc (buffer, sizeof (float) * buffer_samples);
    }

    d.data_in = * data;
    d.input_frames = * samples / stored_channels;
    d.data_out = buffer;
    d.output_frames = buffer_samples / stored_channels;
    d.src_ratio = ratio;
    d.end_of_input = finish;

    if ((error = src_process (state, & d)))
    {
        RESAMPLE_ERROR (error);
        return;
    }

    * data = buffer;
    * samples = stored_channels * d.output_frames_gen;
}

void resample_process (float * * data, int * samples)
{
    do_resample (data, samples, 0);
}

void resample_flush (void)
{
    int error;

    if (state != NULL && (error = src_reset (state)))
        RESAMPLE_ERROR (error);
}

void resample_finish (float * * data, int * samples)
{
    do_resample (data, samples, 1);
    resample_flush ();
}

int resample_decoder_to_output_time (int time)
{
    return time;
}

int resample_output_to_decoder_time (int time)
{
    return time;
}
