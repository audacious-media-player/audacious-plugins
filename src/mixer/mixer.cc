/*
 * Channel Mixer Plugin for Audacious
 * Copyright 2011-2012 John Lindgren and Michał Lipski
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

/* TODO: implement more surround converters */

#include <stdlib.h>

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

class ChannelMixer : public EffectPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Channel Mixer"),
        PACKAGE,
        about,
        & prefs
    };

    /* order #2: must be before crossfade */
    constexpr ChannelMixer () : EffectPlugin (info, 2, false) {}

    bool init ();
    void cleanup ();

    void start (int * channels, int * rate);
    void process (float * * data, int * samples);
};

EXPORT ChannelMixer aud_plugin_instance;

typedef void (* Converter) (float * * data, int * samples);

static float * mixer_buf;

static void mono_to_stereo (float * * data, int * samples)
{
    int frames = * samples;
    float * get = * data;
    float * set = mixer_buf = g_renew (float, mixer_buf, 2 * frames);

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
    float * set = mixer_buf = g_renew (float, mixer_buf, frames);

    * data = mixer_buf;
    * samples = frames;

    while (frames --)
    {
        float val = * get ++;
        val += * get ++;
        * set ++ = val / 2;
    }
}

static void quadro_to_stereo (float * * data, int * samples)
{
    int frames = * samples / 4;
    float * get = * data;
    float * set = mixer_buf = g_renew (float, mixer_buf, 2 * frames);

    * data = mixer_buf;
    * samples = 2 * frames;

    while (frames --)
    {
        float front_left  = * get ++;
        float front_right = * get ++;
        float back_left   = * get ++;
        float back_right  = * get ++;
        * set ++ = front_left + (back_left * 0.7);
        * set ++ = front_right + (back_right * 0.7);
    }
}

static void surround_5p1_to_stereo (float * * data, int * samples)
{
    int frames = * samples / 6;
    float * get = * data;
    float * set = mixer_buf = g_renew (float, mixer_buf, 2 * frames);

    * data = mixer_buf;
    * samples = 2 * frames;

    while (frames --)
    {
        float front_left  = * get ++;
        float front_right = * get ++;
        float center = * get ++;
        float lfe    = * get ++;
        float rear_left   = * get ++;
        float rear_right  = * get ++;
        * set ++ = front_left + (center * 0.5) + (lfe * 0.5) + (rear_left * 0.5);
        * set ++ = front_right + (center * 0.5) + (lfe * 0.5) + (rear_right * 0.5);
    }
}

static Converter get_converter (int in, int out)
{
    if (in == 1 && out == 2)
        return mono_to_stereo;
    if (in == 2 && out == 1)
        return stereo_to_mono;
    if (in == 4 && out == 2)
        return quadro_to_stereo;
    if (in == 6 && out == 2)
        return surround_5p1_to_stereo;

    return nullptr;
}

static int input_channels, output_channels;

void ChannelMixer::start (int * channels, int * rate)
{
    input_channels = * channels;
    output_channels = aud_get_int ("mixer", "channels");

    if (input_channels == output_channels)
        return;

    if (! get_converter (input_channels, output_channels))
    {
        AUDERR ("Converting %d to %d channels is not implemented.\n",
         input_channels, output_channels);
        return;
    }

    * channels = output_channels;
}

void ChannelMixer::process (float * * data, int * samples)
{
    if (input_channels == output_channels)
        return;

    Converter converter = get_converter (input_channels, output_channels);
    if (converter)
        converter (data, samples);
}

const char * const ChannelMixer::defaults[] = {
 "channels", "2",
  nullptr};

bool ChannelMixer::init ()
{
    aud_config_set_defaults ("mixer", defaults);
    return true;
}

void ChannelMixer::cleanup ()
{
    g_free (mixer_buf);
    mixer_buf = 0;
}

const char ChannelMixer::about[] =
 N_("Channel Mixer Plugin for Audacious\n"
    "Copyright 2011-2012 John Lindgren and Michał Lipski");

const PreferencesWidget ChannelMixer::widgets[] = {
    WidgetLabel (N_("<b>Channel Mixer</b>")),
    WidgetSpin (N_("Output channels:"),
        WidgetInt ("mixer", "channels"),
        {1, AUD_MAX_CHANNELS, 1})
};

const PluginPreferences ChannelMixer::prefs = {{ChannelMixer::widgets}};
