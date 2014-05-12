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

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

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

    return NULL;
}

static int input_channels, output_channels;

void mixer_start (int * channels, int * rate)
{
    input_channels = * channels;
    output_channels = aud_get_int ("mixer", "channels");

    if (input_channels == output_channels)
        return;

    if (! get_converter (input_channels, output_channels))
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

    Converter converter = get_converter (input_channels, output_channels);
    if (converter)
        converter (data, samples);
}

static const char * const mixer_defaults[] = {
 "channels", "2",
  NULL};

static bool_t mixer_init (void)
{
    aud_config_set_defaults ("mixer", mixer_defaults);
    return TRUE;
}

static void mixer_cleanup (void)
{
    g_free (mixer_buf);
    mixer_buf = 0;
}

static const char mixer_about[] =
 N_("Channel Mixer Plugin for Audacious\n"
    "Copyright 2011-2012 John Lindgren and Michał Lipski");

static const PreferencesWidget mixer_widgets[] = {
    WidgetLabel (N_("<b>Channel Mixer</b>")),
    WidgetSpin (N_("Output channels:"),
        {VALUE_INT, 0, "mixer", "channels"},
        {1, AUD_MAX_CHANNELS, 1})
};

static const PluginPreferences mixer_prefs = {
    mixer_widgets,
    ARRAY_LEN (mixer_widgets)
};

#define AUD_PLUGIN_NAME        N_("Channel Mixer")
#define AUD_PLUGIN_ABOUT       mixer_about
#define AUD_PLUGIN_PREFS       & mixer_prefs
#define AUD_PLUGIN_INIT        mixer_init
#define AUD_PLUGIN_CLEANUP     mixer_cleanup
#define AUD_EFFECT_START       mixer_start
#define AUD_EFFECT_PROCESS     mixer_process
#define AUD_EFFECT_FINISH      mixer_process
#define AUD_EFFECT_ORDER       2  /* must be before crossfade */

#define AUD_DECLARE_EFFECT
#include <libaudcore/plugin-declare.h>
