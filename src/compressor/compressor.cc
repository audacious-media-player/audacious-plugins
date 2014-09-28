/*
 * Dynamic Range Compression Plugin for Audacious
 * Copyright 2010-2012 John Lindgren
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

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

/* Response time adjustments.  Maybe this should be adjustable? */
#define CHUNK_TIME 0.2f /* seconds */
#define CHUNKS 5
#define DECAY 0.3f

/* What is a "normal" volume?  Replay Gain stuff claims to use 89 dB, but what
 * does that translate to in our PCM range? */
static const char * const compressor_defaults[] = {
    "center", "0.5",
    "range", "0.5",
     nullptr
};

static const PreferencesWidget compressor_widgets[] = {
    WidgetLabel (N_("<b>Compression</b>")),
    WidgetSpin (N_("Center volume:"),
        WidgetFloat ("compressor", "center"),
        {0.1, 1, 0.1}),
    WidgetSpin (N_("Dynamic range:"),
        WidgetFloat ("compressor", "range"),
        {0.0, 3.0, 0.1})
};

static const PluginPreferences compressor_prefs = {{compressor_widgets}};

static const char compressor_about[] =
 N_("Dynamic Range Compression Plugin for Audacious\n"
    "Copyright 2010-2012 John Lindgren");

class Compressor : public EffectPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Dynamic Range Compressor"),
        PACKAGE,
        compressor_about,
        & compressor_prefs
    };

    constexpr Compressor () : EffectPlugin (info, 0, true) {}

    bool init ();
    void cleanup ();

    void start (int * channels, int * rate);
    void process (float * * data, int * samples);
    void flush ();
    void finish (float * * data, int * samples);
    int adjust_delay (int delay);
};

EXPORT Compressor aud_plugin_instance;

static float * buffer, * output, * peaks;
static int output_size;
static int chunk_size, buffer_size;
static int ring_at, buffer_filled, peaks_filled;
static float current_peak;
static int output_filled;
static int current_channels, current_rate;

static void buffer_append (float * * data, int * length)
{
    int offset = (chunk_size * ring_at + buffer_filled) % buffer_size;
    int writable = aud::min (* length, buffer_size - buffer_filled);

    if (writable <= buffer_size - offset)
        memcpy (buffer + offset, * data, sizeof (float) * writable);
    else
    {
        int part = buffer_size - offset;

        memcpy (buffer + offset, * data, part);
        memcpy (buffer, (* data) + part, writable - part);
    }

    buffer_filled += writable;
    * data += writable;
    * length -= writable;
}

/* I used to find the maximum sample and take that as the peak, but that doesn't
 * work well on badly clipped tracks.  Now, I use the highly sophisticated
 * method of averaging the absolute value of the samples and multiplying by 6, a
 * number proved by experiment (on exactly three files) to best approximate the
 * actual peak. */

static float calc_peak (float * data, int length)
{
    if (! length)
        return 0;

    float sum = 0;

    float * end = data + length;
    while (data < end)
        sum += fabsf (* data ++);

    return sum / length * 6;
}

static void do_ramp (float * data, int length, float peak_a, float peak_b)
{
    float center = aud_get_double ("compressor", "center");
    float range = aud_get_double ("compressor", "range");
    float a = powf (peak_a / center, range - 1);
    float b = powf (peak_b / center, range - 1);

    for (int count = 0; count < length; count ++)
    {
        * data = (* data) * (a * (length - count) + b * count) / length;
        data ++;
    }
}

static void output_append (float * data, int length)
{
    if (output_size < output_filled + length)
    {
        output_size = output_filled + length;
        output = g_renew (float, output, output_size);
    }

    memcpy (output + output_filled, data, sizeof (float) * length);
    output_filled += length;
}

static void reset ()
{
    ring_at = 0;
    buffer_filled = 0;
    peaks_filled = 0;
    current_peak = 0.0;
}

#define IN_RING(i) ((ring_at + i) % CHUNKS)
#define GET_PEAK(i) peaks[IN_RING (i)]

static void do_compress (float * * data, int * samples, bool finish)
{
    output_filled = 0;

    while (1)
    {
        buffer_append (data, samples);

        if (buffer_filled < buffer_size)
            break;

        for (; peaks_filled < CHUNKS; peaks_filled ++)
            GET_PEAK (peaks_filled) = calc_peak (buffer + chunk_size * IN_RING
             (peaks_filled), chunk_size);

        if (current_peak == 0.0f)
            current_peak = aud::max (0.01f, calc_peak (peaks, CHUNKS));

        float new_peak = aud::max (0.01f, GET_PEAK (0));
        new_peak = aud::max (new_peak, current_peak * (1.0f - DECAY));

        for (int count = 1; count < CHUNKS; count ++)
            new_peak = aud::max (new_peak, current_peak + (GET_PEAK (count) -
             current_peak) / count);

        do_ramp (buffer + chunk_size * ring_at, chunk_size, current_peak, new_peak);

        output_append (buffer + chunk_size * ring_at, chunk_size);

        ring_at = IN_RING (1);
        buffer_filled -= chunk_size;
        peaks_filled --;
        current_peak = new_peak;
    }

    if (finish)
    {
        int offset = chunk_size * ring_at;
        int first = aud::min (buffer_filled, buffer_size - offset);
        int second = buffer_filled - first;

        if (current_peak == 0.0f)
        {
            current_peak = aud::max (0.01f, calc_peak (buffer + offset, first));
            current_peak = aud::max (current_peak, calc_peak (buffer, second));
        }

        do_ramp (buffer + offset, first, current_peak, current_peak);
        do_ramp (buffer, second, current_peak, current_peak);

        output_append (buffer + offset, first);
        output_append (buffer, second);

        reset ();
    }

    * data = output;
    * samples = output_filled;
}

bool Compressor::init ()
{
    aud_config_set_defaults ("compressor", compressor_defaults);

    buffer = nullptr;
    output = nullptr;
    output_size = 0;
    peaks = nullptr;

    return true;
}

void Compressor::cleanup ()
{
    g_free (buffer);
    g_free (output);
    g_free (peaks);
}

void Compressor::start (int * channels, int * rate)
{
    chunk_size = (* channels) * (int) ((* rate) * CHUNK_TIME);
    buffer_size = chunk_size * CHUNKS;
    buffer = g_renew (float, buffer, buffer_size);
    peaks = g_renew (float, peaks, CHUNKS);

    current_channels = * channels;
    current_rate = * rate;

    reset ();
}

void Compressor::process (float * * data, int * samples)
{
    do_compress (data, samples, false);
}

void Compressor::flush ()
{
    reset ();
}

void Compressor::finish (float * * data, int * samples)
{
    do_compress (data, samples, true);
}

int Compressor::adjust_delay (int delay)
{
    return delay + aud::rescale<int64_t> (buffer_filled / current_channels, current_rate, 1000);
}
