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
    conf_dynamic_range.variable,
    conf_dynamic_range.default_string,
    conf_maximum_amplification.variable,
    conf_maximum_amplification.default_string,
    nullptr};

static constexpr const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(N_(conf_target_level.name),
               WidgetFloat(BACKGROUND_MUSIC_CONFIG, conf_target_level.variable),
               {conf_target_level.value_min, conf_target_level.value_max,
                conf_target_level
                    .value_default}), // Limited until limiter feature arrives
    WidgetSpin(
        N_(conf_dynamic_range.name),
        WidgetFloat(BACKGROUND_MUSIC_CONFIG, conf_dynamic_range.variable),
        {conf_dynamic_range.value_min, conf_dynamic_range.value_max,
         conf_dynamic_range.value_default}),
    WidgetSpin(N_(conf_maximum_amplification.name),
               WidgetFloat(BACKGROUND_MUSIC_CONFIG,
                           conf_maximum_amplification.variable),
               {conf_maximum_amplification.value_min,
                conf_maximum_amplification.value_max,
                conf_maximum_amplification.value_default})};

static constexpr const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static constexpr const char background_music_about[] =
    N_("Background music (equal loudness) Plugin for Audacious\n"
       "Copyright 2023 Michel Fleur");

static constexpr PluginInfo background_music_info = {
    N_("Background music (equal loudness)"), PACKAGE, background_music_about,
    &background_music_preferences};

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
    target_level = exp10(conf_target_level.get_value() / 20);
    range = conf_dynamic_range.get_value();
    maximum_amplification = conf_maximum_amplification.get_value();
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
    aud_config_set_defaults(BACKGROUND_MUSIC_CONFIG, background_music_defaults);
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

    slow = {LONG_INTEGRATION_SECONDS, rate()};
    release = {SHORT_INTEGRATION_SECONDS, rate()};

    perceptive_integrator.set_rate_and_value(rate(), target_level * target_level);
    size_t perceptive_latency = perceptive_integrator.latency();

    total_latency = perceptive_latency;

    print_debug("Total latency=%-zu\n", total_latency);

    buffer.set_size(total_latency * channels(), 0);
}

void BackgroundMusicEngine::on_cleanup(bool was_enabled)
{
    output.clear();
}

bool BackgroundMusicEngine::offer_frame_return_if_output(
    const Index<float> & in, Index<float> & out)
{
    double square_sum = 0.0;
    for (int channel = 0; channel < channels(); channel++) {
        float sample = buffer.get_and_set(in[channel]);
        square_sum += (sample * sample);
        out[channel] = sample;
    }

    double perceptive_mean_square = perceptive_integrator.get_mean_squared(square_sum);
    slow.integrate(perceptive_mean_square);
    double slow_measured = slow.integrated();

    double weighted_square = std::max(slow_measured, SHORT_INTEGRATION_WEIGHT * perceptive_mean_square);

    double weighted_scaled_rms = sqrt(weighted_square) * 3;
    if (weighted_scaled_rms > release.integrated()) {
        release.set_value(weighted_scaled_rms);
    }
    else {
        release.integrate(weighted_scaled_rms);
    }

    double detection = release.integrated();
    double ratio = detection / target_level;
    double amplify =
        aud::clamp(pow(ratio, range - 1), 0.0, maximum_amplification);

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
