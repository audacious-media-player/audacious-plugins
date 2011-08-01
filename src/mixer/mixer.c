/*
 * Channel Mixer Plugin for Audacious
 * Copyright 2011 John Lindgren
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

/* TODO: implement surround converters */

#include <stdio.h>
#include <stdlib.h>

#include "mixer.h"

typedef void (* Converter) (float * * data, int * samples);

static void mono_to_stereo (float * * data, int * samples)
{
    int frames = * samples;
    float * get = * data;
    float * set = mixer_buf = realloc (mixer_buf, sizeof (float) * 2 * frames);

    * data = mixer_buf;
    * samples = 2 * frames;

    while (frames --)
    {
        float val = * get ++;
        * set ++ = val;
        * set ++ = val;
    }
}

static void stereo_to_mono (float * * data, int * samples)
{
    int frames = * samples / 2;
    float * get = * data;
    float * set = mixer_buf = realloc (mixer_buf, sizeof (float) * frames);

    * data = mixer_buf;
    * samples = frames;

    while (frames --)
    {
        float val = * get ++;
        val += * get ++;
        * set ++ = val / 2;
    }
}

static const Converter converters[MAX_CHANNELS + 1][MAX_CHANNELS + 1] = {
 [1][2] = mono_to_stereo,
 [2][1] = stereo_to_mono};

static int input_channels, output_channels;

void mixer_start (int * channels, int * rate)
{
    input_channels = * channels;
    output_channels = mixer_channels;

    if (input_channels == output_channels)
        return;

    if (input_channels < 1 || input_channels > MAX_CHANNELS ||
     ! converters[input_channels][output_channels])
    {
        fprintf (stderr, "Converting %d to %d channels is not implemented.\n",
         input_channels, output_channels);
        return;
    }

    * channels = output_channels;
}

void mixer_process (float * * data, int * samples)
{
    if (input_channels == output_channels)
        return;

    if (input_channels < 1 || input_channels > MAX_CHANNELS ||
     ! converters[input_channels][output_channels])
        return;

    converters[input_channels][output_channels] (data, samples);
}

void mixer_flush (void)
{
}
