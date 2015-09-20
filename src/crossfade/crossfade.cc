/*
 * Crossfade Plugin for Audacious
 * Copyright 2010-2014 John Lindgren
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

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

enum
{
    STATE_OFF,
    STATE_FADEIN,
    STATE_RUNNING,
    STATE_FINISHED,
    STATE_FLUSHED
};

static const char * const crossfade_defaults[] = {
    "automatic", "TRUE",
    "length", "5",
    "manual", "TRUE",
    "manual_length", "0.2",
    nullptr
};

static const char crossfade_about[] =
 N_("Crossfade Plugin for Audacious\n"
    "Copyright 2010-2014 John Lindgren");

static const PreferencesWidget crossfade_widgets[] = {
    WidgetLabel (N_("<b>Crossfade</b>")),
    WidgetCheck (N_("On automatic song change"),
        WidgetBool ("crossfade", "automatic")),
    WidgetSpin (N_("Overlap:"),
        WidgetFloat ("crossfade", "length"),
        {1, 15, 0.5, N_("seconds")},
        WIDGET_CHILD),
    WidgetCheck (N_("On seek or manual song change"),
        WidgetBool ("crossfade", "manual")),
    WidgetSpin (N_("Overlap:"),
        WidgetFloat ("crossfade", "manual_length"),
        {0.1, 3.0, 0.1, N_("seconds")},
        WIDGET_CHILD),
    WidgetLabel (N_("<b>Tip</b>")),
    WidgetLabel (N_("For better crossfading, enable\n"
                    "the Silence Removal effect."))
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
    constexpr Crossfade () : EffectPlugin (info, 5, true) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
    bool flush (bool force);
    Index<float> & finish (Index<float> & data, bool end_of_playlist);
    int adjust_delay (int delay);
};

EXPORT Crossfade aud_plugin_instance;

static char state = STATE_OFF;
static int current_channels, current_rate;
static Index<float> buffer, output;
static int fadein_point;

bool Crossfade::init ()
{
    aud_config_set_defaults ("crossfade", crossfade_defaults);
    return true;
}

void Crossfade::cleanup ()
{
    state = STATE_OFF;
    buffer.clear ();
    output.clear ();
}

static void do_ramp (float * data, int length, float a, float b)
{
    for (int i = 0; i < length; i ++)
    {
        * data = (* data) * (a * (length - i) + b * i) / length;
        data ++;
    }
}

static void mix (float * data, float * add, int length)
{
    while (length --)
        (* data ++) += (* add ++);
}

/* stupid simple resampling/rechanneling algorithm */
static void reformat (int channels, int rate)
{
    if (channels == current_channels && rate == current_rate)
        return;

    int old_frames = buffer.len () / current_channels;
    int new_frames = (int64_t) old_frames * rate / current_rate;

    int map[AUD_MAX_CHANNELS];
    for (int c = 0; c < channels; c ++)
        map[c] = c * current_channels / channels;

    Index<float> new_buffer;
    new_buffer.resize (new_frames * channels);

    for (int f = 0; f < new_frames; f ++)
    {
        int f0 = (int64_t) f * current_rate / rate;
        int s0 = f0 * current_channels;
        int s = f * channels;

        for (int c = 0; c < channels; c ++)
            new_buffer[s + c] = buffer[s0 + map[c]];
    }

    buffer = std::move (new_buffer);
}

static int buffer_needed_for_state ()
{
    double overlap = 0;

    if (state != STATE_FLUSHED && aud_get_bool ("crossfade", "automatic"))
        overlap = aud_get_double ("crossfade", "length");

    if (state != STATE_FINISHED && aud_get_bool ("crossfade", "manual"))
        overlap = aud::max (overlap, aud_get_double ("crossfade", "manual_length"));

    return current_channels * (int) (current_rate * overlap);
}

static void output_data_as_ready (int buffer_needed, bool exact)
{
    int copy = buffer.len () - buffer_needed;

    /* if allowed, wait until we have at least 1/2 second ready to output */
    if (exact ? (copy > 0) : (copy >= current_channels * (current_rate / 2)))
        output.move_from (buffer, 0, -1, copy, true, true);
}

void Crossfade::start (int & channels, int & rate)
{
    if (state != STATE_OFF)
        reformat (channels, rate);

    current_channels = channels;
    current_rate = rate;

    if (state == STATE_OFF)
    {
        if (aud_get_bool ("crossfade", "manual"))
        {
            state = STATE_FLUSHED;
            buffer.insert (0, buffer_needed_for_state ());
        }
        else
            state = STATE_RUNNING;
    }
}

static void run_fadeout ()
{
    do_ramp (buffer.begin (), buffer.len (), 1.0, 0.0);

    state = STATE_FADEIN;
    fadein_point = 0;
}

static void run_fadein (Index<float> & data)
{
    int length = buffer.len ();

    if (fadein_point < length)
    {
        int copy = aud::min (data.len (), length - fadein_point);
        float a = (float) fadein_point / length;
        float b = (float) (fadein_point + copy) / length;

        do_ramp (data.begin (), copy, a, b);
        mix (& buffer[fadein_point], data.begin (), copy);
        data.remove (0, copy);

        fadein_point += copy;
    }

    if (fadein_point == length)
        state = STATE_RUNNING;
}

Index<float> & Crossfade::process (Index<float> & data)
{
    if (state == STATE_OFF)
        return data;

    output.resize (0);

    if (state == STATE_FINISHED || state == STATE_FLUSHED)
        run_fadeout ();

    if (state == STATE_FADEIN)
        run_fadein (data);

    if (state == STATE_RUNNING)
    {
        buffer.insert (data.begin (), -1, data.len ());
        output_data_as_ready (buffer_needed_for_state (), false);
    }

    return output;
}

bool Crossfade::flush (bool force)
{
    if (state == STATE_OFF)
        return true;

    if (! force && aud_get_bool ("crossfade", "manual"))
    {
        state = STATE_FLUSHED;
        int buffer_needed = buffer_needed_for_state ();
        if (buffer.len () > buffer_needed)
            buffer.remove (buffer_needed, -1);

        return false;
    }

    state = STATE_RUNNING;
    buffer.resize (0);

    return true;
}

Index<float> & Crossfade::finish (Index<float> & data, bool end_of_playlist)
{
    if (state == STATE_OFF)
        return data;

    output.resize (0);

    if (state == STATE_FADEIN)
        run_fadein (data);

    if (state == STATE_RUNNING || state == STATE_FINISHED || state == STATE_FLUSHED)
    {
        buffer.insert (data.begin (), -1, data.len ());
        output_data_as_ready (buffer_needed_for_state (), state != STATE_RUNNING);
    }

    if (state == STATE_FADEIN || state == STATE_RUNNING)
    {
        if (aud_get_bool ("crossfade", "automatic"))
        {
            state = STATE_FINISHED;
            output_data_as_ready (buffer_needed_for_state (), true);
        }
        else
        {
            state = STATE_OFF;
            output_data_as_ready (0, true);
        }
    }

    if (end_of_playlist && (state == STATE_FINISHED || state == STATE_FLUSHED))
    {
        do_ramp (buffer.begin (), buffer.len (), 1.0, 0.0);

        state = STATE_OFF;
        output_data_as_ready (0, true);
    }

    return output;
}

int Crossfade::adjust_delay (int delay)
{
    return delay + aud::rescale<int64_t> (buffer.len () / current_channels, current_rate, 1000);
}
