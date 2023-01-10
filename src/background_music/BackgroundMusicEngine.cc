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
    : FrameBasedPlugin(background_music_info, order), multi_integrator(10)
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
    return read_ahead / channels() - 1;
}

bool BackgroundMusicEngine::on_flush(bool force)
{
    read_ahead_buffer.discard();
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
        read_ahead_buffer.discard();
    }
    return true;
}

void BackgroundMusicEngine::on_start(int previous_channels, int previous_rate)
{
    processed_frames = 0;

    multi_integrator.set_rate_and_value(rate(), target_level * target_level);
    read_ahead = multi_integrator.latency();

    // As data is added, then fetched, we need 1 extra frame in the buffer.
    int alloc_size = channels() * (read_ahead + 1);
    if (read_ahead_buffer.size() < alloc_size)
    {
        read_ahead_buffer.alloc(alloc_size);
    }
    read_ahead_buffer.discard();
}

void BackgroundMusicEngine::on_cleanup(bool was_enabled)
{
    read_ahead_buffer.destroy();
    output.clear();
}

bool BackgroundMusicEngine::offer_frame_return_if_output(
    const Index<float> & in, Index<float> & out)
{

    /**
     * Add samples to the delay buffer so that the detection can be
     * applied ion a predictive fashion.
     */
    read_ahead_buffer.copy_in(in.begin(), channels());

    /*
     * Detection is based on a kind-of RMS value. The sum of the squared sample
     * values in each frame is integrated over two time periods. The square root
     * of those integrated values is a pretty nice indication of loudness
     * for a first implementation.
     * The detection does nothing fancy and just uses a weighted sum of the
     * square roots of both integrations. In the future it will use multiple
     * frequency bands, actual RMS, ear-loudness curves and actual perceived
     * weighting of RMS windows.
     */
    double square_sum = 0.0;
    for (float sample : in)
    {
        square_sum += (sample * sample);
    }
    double mean_square = multi_integrator.get_mean_squared(square_sum);

    double detection = sqrt(mean_square) * 2;
    double ratio = detection / target_level;
    double amplify =
        aud::clamp(pow(ratio, range - 1), 0.0, maximum_amplification);

    if (processed_frames >= read_ahead)
    {
        read_ahead_buffer.move_out(out.begin(), channels());
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

double BackgroundMultiIntegrator::on_integration()
{
    double peak = do_integrate_from_buffer(PEAK_INTEGRATOR);

    if (peak > peak_hold_integrator.integrated())
    {
        peak_hold_integrator.set_value(peak);
    }
    else
    {
        peak_hold_integrator.integrate(peak);
    }

    double short_looked_ahead =
        get_buffered_value_to_integrate(SHORT_INTEGRATOR);
    double short_value = integrate_value(
        SHORT_INTEGRATOR,
        std::max(peak_hold_integrator.integrated(), short_looked_ahead));

    if (short_value > short_hold_integrator.integrated())
    {
        short_hold_integrator.set_value(short_value);
    }
    else
    {
        short_hold_integrator.integrate(short_value);
    }
    double long_looked_ahead = get_buffered_value_to_integrate(LONG_INTEGRATOR);
    double short_integrated_value = short_hold_integrator.integrated();
    double long_value = integrate_value(
        LONG_INTEGRATOR, std::max(short_integrated_value, long_looked_ahead));
    return std::max(short_integrated_value, long_value);
}

void BackgroundMultiIntegrator::on_set_value(double value)
{
    peak_hold_integrator.set_value(value);
    short_hold_integrator.set_value(value);
}

[[maybe_unused]] void
BackgroundMultiIntegrator::set_hold_integrator(const Integrator<double> & v)
{
    peak_hold_integrator = v;
}

[[maybe_unused]] void BackgroundMultiIntegrator::set_short_hold_integrator(
    const Integrator<double> & v)
{
    short_hold_integrator = v;
}
