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
#include <soxr.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#define MIN_RATE 8000
#define MAX_RATE 192000
#define RATE_STEP 50

class SoXResampler : public EffectPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("SoX Resampler"),
        PACKAGE,
        about,
        & prefs
    };

    /* order #2: must be before crossfade */
    constexpr SoXResampler () : EffectPlugin (info, 2, false) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
    bool flush (bool force);
};

EXPORT SoXResampler aud_plugin_instance;

const char * const SoXResampler::defaults[] = {
    "quality", aud::numeric_string<SOXR_HQ>::str,
    "rate", "44100",
    nullptr
};

static soxr_t soxr;
static soxr_error_t error;
static int stored_channels;
static double ratio;
static Index<float> buffer;

bool SoXResampler::init ()
{
    aud_config_set_defaults ("soxr", defaults);
    return true;
}

void SoXResampler::cleanup ()
{
    soxr_delete (soxr);
    soxr = 0;
    buffer.clear ();
}

void SoXResampler::start (int & channels, int & rate)
{
    soxr_delete (soxr);
    soxr = 0;

    int new_rate = aud_get_int ("soxr", "rate");
    new_rate = aud::clamp (new_rate, MIN_RATE, MAX_RATE);

    if (new_rate == rate)
        return;

    soxr_quality_spec_t q = soxr_quality_spec (aud_get_int ("soxr", "quality"), 0);

    soxr = soxr_create (rate, new_rate, channels, & error, nullptr, & q, nullptr);

    if (error)
    {
        AUDERR ("%s\n", error);
        return;
    }

    stored_channels = channels;
    ratio = (double) new_rate / rate;
    rate = new_rate;
}

Index<float> & SoXResampler::process (Index<float> & data)
{
    if (! soxr)
         return data;

    buffer.resize ((int) (data.len () * ratio) + 256);

    size_t samples_done;
    error = soxr_process (soxr, data.begin (), data.len () / stored_channels,
     nullptr, buffer.begin (), buffer.len () / stored_channels, & samples_done);

    if (error)
    {
        AUDERR ("%s\n", error);
        return data;
    }

    buffer.resize (samples_done * stored_channels);

    return buffer;
}

bool SoXResampler::flush (bool force)
{
    if (soxr && (error = soxr_process (soxr, nullptr, 0, nullptr, nullptr, 0, nullptr)))
        AUDERR ("%s\n", error);

    return true;
}

const char SoXResampler::about[] =
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

const PreferencesWidget SoXResampler::widgets[] = {
    WidgetCombo (N_("Quality:"),
        WidgetInt ("soxr", "quality"),
        {{method_list}}),
    WidgetSpin (N_("Rate:"),
        WidgetInt ("soxr", "rate"),
        {MIN_RATE, MAX_RATE, RATE_STEP, N_("Hz")})
};

const PluginPreferences SoXResampler::prefs = {{widgets}};
