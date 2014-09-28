/*
 * Crossfade Plugin for Audacious
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

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

enum
{
    STATE_OFF,
    STATE_PREBUFFER,
    STATE_RUNNING,
    STATE_BETWEEN,
    STATE_STOPPING,
};

static const char * const crossfade_defaults[] = {
 "length", "3",
 nullptr};

static const char crossfade_about[] =
 N_("Crossfade Plugin for Audacious\n"
    "Copyright 2010-2012 John Lindgren");

static const PreferencesWidget crossfade_widgets[] = {
    WidgetLabel (N_("<b>Crossfade</b>")),
    WidgetSpin (N_("Overlap:"),
        WidgetInt ("crossfade", "length"),
        {1, 10, 1, N_("seconds")})
};

static const PluginPreferences crossfade_prefs = {{crossfade_widgets}};

class Crossfade : public EffectPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Crossfade"),
        PACKAGE,
        crossfade_about,
        & crossfade_prefs
    };

    /* order #5: must be after resample and mixer */
    Crossfade () : EffectPlugin (info, 5, true) {}

    bool init ();
    void cleanup ();

    void start (int * channels, int * rate);
    void process (float * * data, int * samples);
    void flush ();
    void finish (float * * data, int * samples);
    int adjust_delay (int delay);
};

static char state = STATE_OFF;
static int current_channels = 0, current_rate = 0;
static float * buffer = nullptr;
static int buffer_size = 0, buffer_filled = 0;
static int prebuffer_filled = 0;
static float * output = nullptr;
static int output_size = 0;

static void reset (void)
{
    state = STATE_OFF;
    current_channels = 0;
    current_rate = 0;
    g_free (buffer);
    buffer = nullptr;
    buffer_size = 0;
    buffer_filled = 0;
    prebuffer_filled = 0;
    g_free (output);
    output = nullptr;
    output_size = 0;
}

bool Crossfade::init (void)
{
    aud_config_set_defaults ("crossfade", crossfade_defaults);
    return true;
}

void Crossfade::cleanup (void)
{
    reset ();
}

void Crossfade::start (int * channels, int * rate)
{
    if (state != STATE_BETWEEN)
        reset ();
    else if (* channels != current_channels)
    {
        aud_ui_show_error (_("Crossfading failed because the songs had "
         "a different number of channels.  You can use the Channel Mixer to "
         "convert the songs to the same number of channels."));
        reset ();
    }
    else if (* rate != current_rate)
    {
        aud_ui_show_error (_("Crossfading failed because the songs had "
         "different sample rates.  You can use the Sample Rate Converter to "
         "convert the songs to the same sample rate."));
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

static void mix (float * data, float * add, int length)
{
    while (length --)
        (* data ++) += (* add ++);
}

static void enlarge_buffer (int length)
{
    if (length > buffer_size)
    {
        buffer = g_renew (float, buffer, length);
        buffer_size = length;
    }
}

static void add_data (float * data, int length)
{
    if (state == STATE_PREBUFFER)
    {
        int full = current_channels * current_rate * aud_get_int ("crossfade", "length");

        if (prebuffer_filled < full)
        {
            int copy = aud::min (length, full - prebuffer_filled);
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
            int copy = aud::min (length, buffer_filled - prebuffer_filled);

            mix (buffer + prebuffer_filled, data, copy);
            prebuffer_filled += copy;
            data += copy;
            length -= copy;
        }

        if (prebuffer_filled < buffer_filled)
            return;

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
        output = g_renew (float, output, length);
        output_size = length;
    }
}

static void return_data (float * * data, int * length)
{
    int full = current_channels * current_rate * aud_get_int ("crossfade", "length");
    int copy = buffer_filled - full;

    /* only return if we have at least 1/2 second -- this reduces memmove's */
    if (state != STATE_RUNNING || copy < current_channels * (current_rate / 2))
    {
        * data = nullptr;
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

void Crossfade::process (float * * data, int * samples)
{
    add_data (* data, * samples);
    return_data (data, samples);
}

void Crossfade::flush (void)
{
    if (state == STATE_PREBUFFER || state == STATE_RUNNING)
    {
        state = STATE_RUNNING;
        buffer_filled = 0;
    }
}

void Crossfade::finish (float * * data, int * samples)
{
    if (state == STATE_BETWEEN) /* second call, end of last song */
    {
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
        do_ramp (buffer, buffer_filled, 1.0, 0.0);
        state = STATE_BETWEEN;
    }
}

int Crossfade::adjust_delay (int delay)
{
    return delay + aud::rescale<int64_t> (buffer_filled / current_channels, current_rate, 1000);
}
