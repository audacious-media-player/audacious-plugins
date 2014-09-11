/*
 * SoX Resampler Plugin for Audacious
 * Copyright 2013 Michał Lipski
 *
 * Based on Sample Rate Converter Plugin:
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

#include <glib.h>

#include <soxr.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#define MIN_RATE 8000
#define MAX_RATE 192000
#define RATE_STEP 50

const char default_quality[] = {'0' + SOXR_HQ, 0};

static const char * const sox_resampler_defaults[] = {
 "quality", default_quality,
 "rate", "44100",
 nullptr};

static soxr_t soxr;
static soxr_error_t error;
static int stored_channels;
static double ratio;
static float * buffer;
static size_t buffer_samples;

bool sox_resampler_init (void)
{
    aud_config_set_defaults ("soxr", sox_resampler_defaults);
    return true;
}

void sox_resampler_cleanup (void)
{
    soxr_delete (soxr);
    soxr = 0;
    g_free (buffer);
    buffer = nullptr;
    buffer_samples = 0;
}

void sox_resampler_start (int * channels, int * rate)
{
    soxr_delete (soxr);
    soxr = 0;

    int new_rate = aud_get_int ("soxr", "rate");
    new_rate = aud::clamp (new_rate, MIN_RATE, MAX_RATE);

    if (new_rate == * rate)
        return;

    soxr_quality_spec_t q = soxr_quality_spec(aud_get_int ("soxr", "quality"), 0);

    soxr = soxr_create((double) * rate, (double) new_rate, * channels, & error, nullptr, & q, nullptr);

    if (error)
    {
        AUDERR (error);
        return;
    }

    stored_channels = * channels;
    ratio = (double) new_rate / * rate;
    * rate = new_rate;
}

void do_resample (float * * data, int * samples)
{
    if (! soxr)
         return;

    if (buffer_samples < (unsigned) (* samples * ratio) + 256)
    {
        buffer_samples = (unsigned) (* samples * ratio) + 256;
        buffer = g_renew (float, buffer, buffer_samples);
    }

    size_t samples_done;

    error = soxr_process(soxr, * data, * samples / stored_channels, nullptr,
        buffer, buffer_samples / stored_channels, & samples_done);

    if (error)
    {
        AUDERR (error);
        return;
    }

    * data = buffer;
    * samples = samples_done * stored_channels;
}

void sox_resampler_process (float * * data, int * samples)
{
    do_resample (data, samples);
}

void sox_resampler_flush (void)
{
    if (soxr && (error = soxr_process(soxr, nullptr, 0, nullptr, nullptr, 0, nullptr)))
        AUDERR (error);
}

void sox_resampler_finish (float * * data, int * samples)
{
    do_resample (data, samples);
}

static const char sox_resampler_about[] =
 N_("SoX Resampler Plugin for Audacious\n"
    "Copyright 2013 Michał Lipski\n\n"
    "Based on Sample Rate Converter Plugin:\n"
    "Copyright 2010-2012 John Lindgren");

static const ComboItem method_list[] = {
    ComboItem (N_("Quick"), SOXR_QQ),
    ComboItem (N_("Low"), SOXR_LQ),
    ComboItem (N_("Medium"), SOXR_MQ),
    ComboItem (N_("High"), SOXR_HQ),
    ComboItem (N_("Very High"), SOXR_VHQ)
};

static const PreferencesWidget sox_resampler_widgets[] = {
    WidgetCombo (N_("Quality:"),
        WidgetInt ("soxr", "quality"),
        {{method_list}}),
    WidgetSpin (N_("Rate:"),
        WidgetInt ("soxr", "rate"),
        {MIN_RATE, MAX_RATE, RATE_STEP, N_("Hz")})
};

static const PluginPreferences sox_resampler_prefs = {{sox_resampler_widgets}};

#define AUD_PLUGIN_NAME        N_("SoX Resampler")
#define AUD_PLUGIN_ABOUT       sox_resampler_about
#define AUD_PLUGIN_PREFS       & sox_resampler_prefs
#define AUD_PLUGIN_INIT        sox_resampler_init
#define AUD_PLUGIN_CLEANUP     sox_resampler_cleanup
#define AUD_EFFECT_START       sox_resampler_start
#define AUD_EFFECT_PROCESS     sox_resampler_process
#define AUD_EFFECT_FLUSH       sox_resampler_flush
#define AUD_EFFECT_FINISH      sox_resampler_finish
#define AUD_EFFECT_ORDER       2  /* must be before crossfade */

#define AUD_DECLARE_EFFECT
#include <libaudcore/plugin-declare.h>
