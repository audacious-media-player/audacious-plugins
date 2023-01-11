/*
 * Background music (equal loudness) engine.
 * Copyright 2023 Michel Fleur
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

#include "BackgroundMusicEngine.h"
#include <cmath>

static constexpr unsigned PEAK_INTEGRATOR = 0;
static constexpr unsigned SHORT_INTEGRATOR = 1;
static constexpr unsigned LONG_INTEGRATOR = 2;

/*
 * The standard "center" volume is set to 0.25 while the dynamic range is set to
 * zero, meaning all songs will be equally loud according to the detected
 * loudness, that is.
 */
static constexpr const char * const background_music_defaults[] = {
    conf_target_level.variable,
    conf_target_level.default_string,
    conf_maximum_amplification.variable,
    conf_maximum_amplification.default_string,
    conf_slow_measurement.variable,
    conf_slow_measurement.default_string,
    conf_fast_measurement.variable,
    conf_fast_measurement.default_string,
    conf_perceived_slow_balance.variable,
    conf_perceived_slow_balance.default_string,
    nullptr};

#define plugin_version "0.1.0"

static constexpr const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(N_(conf_target_level.name),
               WidgetFloat(BACKGROUND_MUSIC_CONFIG, conf_target_level.variable),
               {conf_target_level.value_min, conf_target_level.value_max,
                1.0}), // Limited until limiter feature arrives
    WidgetSpin(N_(conf_maximum_amplification.name),
               WidgetFloat(BACKGROUND_MUSIC_CONFIG,
                           conf_maximum_amplification.variable),
               {conf_maximum_amplification.value_min,
                conf_maximum_amplification.value_max, 1.0}),
    WidgetSeparator(),
    WidgetLabel(N_("<b>Advanced</b>")),
    WidgetSpin(
        N_(conf_slow_measurement.name),
        WidgetFloat(BACKGROUND_MUSIC_CONFIG, conf_slow_measurement.variable),
        {conf_slow_measurement.value_min, conf_slow_measurement.value_max,
         0.1}),
    WidgetSpin(
        N_(conf_fast_measurement.name),
        WidgetFloat(BACKGROUND_MUSIC_CONFIG, conf_fast_measurement.variable),
        {conf_fast_measurement.value_min, conf_fast_measurement.value_max,
         0.1}),
    WidgetSpin(N_(conf_perceived_slow_balance.name),
               WidgetFloat(BACKGROUND_MUSIC_CONFIG,
                           conf_perceived_slow_balance.variable),
               {conf_perceived_slow_balance.value_min,
                conf_perceived_slow_balance.value_max, 0.1}),
    WidgetLabel(N_("(-1: fast, radio like: 1: slow, peak issues)")),
};

static constexpr const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static constexpr const char background_music_about[] =
    N_("Background music\nEqual loudness plugin for Audacious \n"
       "version " plugin_version "\n"
       "Copyright 2023 Michel Fleur");

static constexpr PluginInfo background_music_info = {
    N_("Background music (equal loudness) "), PACKAGE, background_music_about,
    &background_music_preferences};

static void set_defaults(bool on_init) {
    int version = conf_version.get_value();
    bool legacy = version < config_version;
    bool set_defaults = on_init || legacy;
    if (set_defaults) {
        aud_config_set_defaults(BACKGROUND_MUSIC_CONFIG, background_music_defaults);
    }
    if (legacy) {
        conf_version.set_value(config_version);
    }
}

BackgroundMusicEngine::BackgroundMusicEngine(int order)
    : FrameBasedPlugin(background_music_info, order), perceptive_integrator(12)
{
}

const char * BackgroundMusicEngine::name() const
{
    return BackgroundMusicEngine::info.name;
}

void BackgroundMusicEngine::update_config(bool is_processing)
{
    set_defaults(false);
    // dB
    target_level = exp10(conf_target_level.get_value() / 20);
    double balance = conf_perceived_slow_balance.get_value();
    if (balance < 0)
    {
        perceived_weight = 1.0;
        slow_weight = std::clamp(1.0 + balance, 0.0, 1.0);
        slow_weight *= slow_weight;
    }
    else
    {
        slow_weight = 1.0;
        perceived_weight = std::clamp(1.0 - balance, 0.0, 1.0);
        perceived_weight *= perceived_weight;
    }
    // dB
    maximum_amplification = exp10(conf_maximum_amplification.get_value() / 20);

    slow_seconds = conf_slow_measurement.get_value();
    fast_seconds = conf_fast_measurement.get_value();
    minimum_detection = target_level / maximum_amplification;

    if (is_processing || rate() > 0)
    {
        release = {fast_seconds, rate()};
        // Integrate then squared effectively doubles release time
        slow = {0.5 * slow_seconds, rate()};
    }

    if constexpr (enabled_print_debug)
    {
        static int count = 0;
        count++;
        if ((count % 100) == 0)
        {
            print_debug("%s: target=%lf; max-amp=%lf; slow=%lf; fast=%lf; "
                        "perceived-weight=%lf; slow-weight=%lf\n",
                        name(), target_level, maximum_amplification,
                        slow_seconds, fast_seconds, sqrt(perceived_weight),
                        sqrt(slow_weight));
            count = 0;
        }
    }
}

int BackgroundMusicEngine::latency() const
{
    return total_latency / channels();
}

bool BackgroundMusicEngine::on_flush(bool force)
{
    buffer.set_size(0, 0);
    return true;
}

bool BackgroundMusicEngine::on_init()
{
    set_defaults(true);
    update_config(false);
    return true;
}

bool BackgroundMusicEngine::after_finished(bool end_of_playlist)
{
    if (end_of_playlist)
    {
        buffer.set_size(0, 0);
    }
    return true;
}

void BackgroundMusicEngine::on_start(int previous_channels, int previous_rate)
{
    processed_frames = 0;

    perceptive_integrator.set_rate_and_value(rate(),
                                             target_level * target_level);
    size_t perceptive_latency = perceptive_integrator.latency();

    total_latency = perceptive_latency;

    print_debug("Total latency=%-zu\n", total_latency);

    buffer.set_size(total_latency * channels(), 0);
}

void BackgroundMusicEngine::on_cleanup(bool was_enabled) { output.clear(); }

bool BackgroundMusicEngine::offer_frame_return_if_output(
    const Index<float> & in, Index<float> & out)
{
    double square_sum = 0.0;
    for (int channel = 0; channel < channels(); channel++)
    {
        float sample = buffer.get_and_set(in[channel]);
        square_sum += (sample * sample);
        out[channel] = sample;
    }

    double perceptive_mean_square =
        perceptive_integrator.get_mean_squared(square_sum);
    slow.integrate(square_sum);
    double slow_measured = slow.integrated();

    double weighted_square = std::max(
        slow_weight * slow_measured, perceived_weight * perceptive_mean_square);

    double weighted_scaled_rms = sqrt(weighted_square) * 3;
    if (weighted_scaled_rms > release.integrated())
    {
        release.set_value(weighted_scaled_rms);
    }
    else
    {
        release.integrate(weighted_scaled_rms);
    }

    double detection = std::max(minimum_detection, release.integrated());
    double amplify = target_level / detection;

    if constexpr (enabled_print_debug)
    {
        static double old_amplify = 0;

        if (fabs(old_amplify - amplify) / (old_amplify + amplify) > 0.1)
        {
            print_debug("%s amp=%lf\n", name(), amplify);
            old_amplify = amplify;
        }
    }
    if (processed_frames >= total_latency)
    {
        for (float & sample : out)
        {
            sample *= (float)amplify;
        }
        return true;
    }
    else
    {
        processed_frames++;
    }

    return false;
}
