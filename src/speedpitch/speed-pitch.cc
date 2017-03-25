/*
 * Speed and Pitch effect plugin for Audacious
 * Copyright 2012 John Lindgren
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
#include <samplerate.h>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

/* The general idea of the speed change algorithm is to divide the input signal
 * into pieces, spaced at a time interval A, using a cosine-shaped window
 * function.  The pieces are then reassembled by adding them together again,
 * spaced at another time interval B.  By varying the ratio A:B, we change the
 * speed of the audio. */

#define FREQ    10
#define OVERLAP  3

#define CFGSECT "speed-pitch"
#define MINSPEED 0.5
#define MAXSPEED 2.0
#define MINPITCH 0.5
#define MAXPITCH 2.0
#define MINSEMITONES -12.0
#define MAXSEMITONES 12.0

class SpeedPitch : public EffectPlugin
{
public:
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Speed and Pitch"),
        PACKAGE,
        nullptr,
        & prefs
    };

    constexpr SpeedPitch () : EffectPlugin (info, 0, true) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    bool flush (bool force);
    int adjust_delay (int delay);

    Index<float> & process (Index<float> & samples)
        { return process (samples, false); }
    Index<float> & finish (Index<float> & samples, bool end_of_playlist)
        { return process (samples, true); }

private:
    Index<float> & process (Index<float> & samples, bool ending);
};

EXPORT SpeedPitch aud_plugin_instance;

static double semitones;
static int curchans, currate;
static SRC_STATE * srcstate;
static int outstep, width;
static Index<float> cosine;
static Index<float> in, out;
static int src, dst;

static void add_data (Index<float> & b, Index<float> & data, float ratio)
{
    int oldlen = b.len ();
    int inframes = data.len () / curchans;
    int maxframes = (int) (inframes * ratio) + 256;
    b.resize (oldlen + maxframes * curchans);

    SRC_DATA d = SRC_DATA ();

    d.data_in = data.begin ();
    d.input_frames = inframes;
    d.data_out = & b[oldlen];
    d.output_frames = maxframes;
    d.src_ratio = ratio;

    src_process (srcstate, & d);
    b.resize (oldlen + d.output_frames_gen * curchans);
}

bool SpeedPitch::flush (bool force)
{
    src_reset (srcstate);

    in.resize (0);
    out.resize (0);

    /* The source and destination pointers give the center of the next cosine
     * window to be copied, relative to the current input and output buffers. */
    src = dst = 0;

    /* The output buffer always extends right of the destination pointer by half
     * the width of a cosine window. */
    out.insert (0, width / 2);

    return true;
}

void SpeedPitch::start (int & chans, int & rate)
{
    curchans = chans;
    currate = rate;

    if (srcstate)
        src_delete (srcstate);

    srcstate = src_new (SRC_LINEAR, curchans, nullptr);

    /* Calculate the width of the cosine window and the spacing interval for
     * output.  Make them both even numbers for convenience.  Note that the
     * cosine window is applied without deinterleaving the audio samples. */
    outstep = ((currate / FREQ) & ~1) * curchans;
    width = outstep * OVERLAP;

    /* Generate the cosine window, scaled vertically to compensate for the
     * overlap of the reassembled pieces of audio. */
    cosine.resize (width);
    for (int i = 0; i < width; i ++)
        cosine[i] = (1.0 - cos (2.0 * M_PI * i / width)) / OVERLAP;

    flush (true);
}

Index<float> & SpeedPitch::process (Index<float> & data, bool ending)
{
    const float * cosine_center = & cosine[width / 2];
    float pitch = aud_get_double (CFGSECT, "pitch");
    float speed = aud_get_double (CFGSECT, "speed");

    /* Copy the passed audio to the input buffer, scaled to adjust pitch. */
    add_data (in, data, 1.0 / pitch);

    if (! aud_get_bool (CFGSECT, "decouple"))
    {
        data = std::move (in);
        return data;
    }

    /* Calculate the spacing interval for input. */
    int instep = (int) round ((outstep / curchans) * speed / pitch) * curchans;

    /* Stop copying half a window's width before the end of the input buffer (or
     * right up to the end of the buffer if the song is ending). */
    int stop = in.len () - (ending ? 0 : width / 2);

    while (src <= stop)
    {
        /* Truncate the window to avoid overflows if necessary. */
        int begin = aud::max (-(width / 2), aud::max (-src, -dst));
        int end = aud::min (width / 2, aud::min (in.len () - src, out.len () - dst));

        for (int i = begin; i < end; i ++)
            out[dst + i] += in[src + i] * cosine_center[i];

        src += instep;
        dst += outstep;

        out.insert (-1, outstep);
    }

    /* Discard input up to half a window's width before the source pointer (or
     * right up to the previous source pointer if the song is ending. */
    int seek = aud::clamp (0, src - (ending ? instep : width / 2), in.len ());
    in.remove (0, seek);
    src -= seek;

    data.resize (0);

    /* Return output up to half a window's width before the destination pointer
     * (or right up to the previous destination pointer if the song is ending). */
    int ret = aud::clamp (0, dst - (ending ? outstep : width / 2), out.len ());
    data.move_from (out, 0, 0, ret, true, true);
    dst -= ret;

    return data;
}

int SpeedPitch::adjust_delay (int delay)
{
    if (! aud_get_bool (CFGSECT, "decouple"))
        return delay;

    float samples_to_ms = 1000.0 / (curchans * currate);
    float speed = aud_get_double (CFGSECT, "speed");
    int in_samples = in.len () - src;
    int out_samples = dst;

    return (delay + in_samples * samples_to_ms) * speed + out_samples * samples_to_ms;
}

static void sync_speed ()
{
    if (! aud_get_bool (CFGSECT, "decouple"))
    {
        aud_set_double (CFGSECT, "speed", aud_get_double (CFGSECT, "pitch"));
        hook_call ("speed-pitch set speed", nullptr);
    }
}

static void pitch_changed ()
{
    semitones = 12 * log (aud_get_double (CFGSECT, "pitch")) / log (2);
    hook_call ("speed-pitch set semitones", nullptr);
    sync_speed ();
}

static void semitones_changed ()
{
    aud_set_double (CFGSECT, "pitch", pow (2, semitones / 12));
    hook_call ("speed-pitch set pitch", nullptr);
    sync_speed ();
}

const char * const SpeedPitch::defaults[] = {
 "decouple", "TRUE",
 "speed", "1",
 "pitch", "1",
 nullptr};

const PreferencesWidget SpeedPitch::widgets[] = {
    WidgetLabel (N_("<b>Speed</b>")),
    WidgetCheck (N_("Decouple from pitch"),
        WidgetBool (CFGSECT, "decouple", sync_speed)),
    WidgetSpin (N_("Multiplier:"),
        WidgetFloat (CFGSECT, "speed", nullptr, "speed-pitch set speed"),
        {MINSPEED, MAXSPEED, 0.05},
        WIDGET_CHILD),
    WidgetLabel (N_("<b>Pitch</b>")),
    WidgetSpin (nullptr,
        WidgetFloat (semitones, semitones_changed, "speed-pitch set semitones"),
        {MINSEMITONES, MAXSEMITONES, 0.05, N_("semitones")}),
    WidgetSpin (N_("Multiplier:"),
        WidgetFloat (CFGSECT, "pitch", pitch_changed, "speed-pitch set pitch"),
        {MINPITCH, MAXPITCH, 0.005},
        WIDGET_CHILD)
};

const PluginPreferences SpeedPitch::prefs = {{widgets}};

bool SpeedPitch::init ()
{
    aud_config_set_defaults (CFGSECT, defaults);
    pitch_changed ();
    return true;
}

void SpeedPitch::cleanup ()
{
    if (srcstate)
        src_delete (srcstate);

    srcstate = nullptr;

    cosine.clear ();
    in.clear ();
    out.clear ();
}
