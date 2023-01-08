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
#include <libaudcore/runtime.h>

static constexpr const char * const CONFIG_SECTION_BACKGROUND_MUSIC =
    "background_music";
static constexpr const char * const CONFIG_VALUE_TARGET_LEVEL = "target_level";
static constexpr const char * const CONFIG_VALUE_DYNAMIC_RANGE =
    "dynamic_range";
static constexpr const char * const CONFIG_VALUE_MAXIMUM_AMPLIFICATION =
    "maximum_amplification";

static constexpr double SMOOTHER_INTEGRATION_SECONDS = 0.010;

static constexpr unsigned PEAK_INTEGRATOR = 0;
static constexpr double PEAK_INTEGRATION_SECONDS = 0.020;
static constexpr double PEAK_INTEGRATION_WEIGHT = 0.25;

static constexpr unsigned SHORT_INTEGRATOR = 1;
static constexpr double SHORT_INTEGRATION_SECONDS = 0.2;//;0.200;
static constexpr double SHORT_INTEGRATION_WEIGHT = 0.5;

static constexpr unsigned LONG_INTEGRATOR = 2;
static constexpr double LONG_INTEGRATION_SECONDS = 3.2;
static constexpr double LONG_INTEGRATION_WEIGHT = 1.0;

/*
 * The standard "center" volume is set to 0.25 while the dynamic range is set to
 * zero, meaning all songs will be equally loud according to the detected
 * loudness, that is.
 */
static constexpr const char * const background_music_defaults[] = {
    CONFIG_VALUE_TARGET_LEVEL,
    "-10",
    CONFIG_VALUE_DYNAMIC_RANGE,
    "0.0",
    CONFIG_VALUE_MAXIMUM_AMPLIFICATION,
    "10",
    nullptr};

static constexpr const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(
        N_("Target level (dB):"),
        WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC, CONFIG_VALUE_TARGET_LEVEL),
        {-20.0, -6.0, -12.0}), // Limited until limiter feature arrives
    WidgetSpin(N_("Dynamic range:"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONFIG_VALUE_DYNAMIC_RANGE),
               {0.0, 1.0, 0.1}),
    WidgetSpin(N_("Maximum amplification:"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONFIG_VALUE_MAXIMUM_AMPLIFICATION),
               {1.0, 100.0, 10.0})};

static constexpr const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static constexpr const char background_music_about[] =
    N_("Background music (equal loudness) Plugin for Audacious\n"
       "Copyright 2023 Michel Fleur");

static constexpr PluginInfo background_music_info = {
    N_("Background music (equal loudness)"), PACKAGE, background_music_about,
    &background_music_preferences};

/**
 * Determines how fast the step response for the square root of the integrator
 * reaches 1 - 1/e, the usual characteristic "RC" value for an integrator.
 * @param integrator The integrator
 * @param maximum_samples After how many samples to bail out.
 * @return The sample count needed to reach teh desired value.
 */
static int
get_root_integrate_square_rc_time_samples(const Integrator & integrator,
                                          const Integrator & smoother,
                                          int maximum_samples)
{
    double rc_value = 1.0 - 1.0 / M_E;
    double sqr_value = rc_value * rc_value;
    return Integrator::get_samples_until_threshold_step(
        integrator, smoother, sqr_value, maximum_samples);
}

BackgroundMusicEngine::BackgroundMusicEngine(int order)
    : FrameBasedPlugin(background_music_info, order)
{
    multi_integrator.set_scale(PEAK_INTEGRATOR, PEAK_INTEGRATION_WEIGHT);
    multi_integrator.set_scale(SHORT_INTEGRATOR, SHORT_INTEGRATION_WEIGHT);
    multi_integrator.set_scale(LONG_INTEGRATOR, LONG_INTEGRATION_WEIGHT);
}

const char * BackgroundMusicEngine::name() const
{
    return BackgroundMusicEngine::info.name;
}

void BackgroundMusicEngine::update_config(bool is_processing)
{
    double target_level_in_db = aud_get_double(CONFIG_SECTION_BACKGROUND_MUSIC,
                                               CONFIG_VALUE_TARGET_LEVEL);
    target_level = exp10(aud::clamp(target_level_in_db, -20.0, 0.0) / 10);

    range = aud_get_double(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONFIG_VALUE_DYNAMIC_RANGE);
    maximum_amplification = aud_get_double(CONFIG_SECTION_BACKGROUND_MUSIC,
                                           CONFIG_VALUE_MAXIMUM_AMPLIFICATION);
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
    aud_config_set_defaults(CONFIG_SECTION_BACKGROUND_MUSIC,
                            background_music_defaults);
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

    // Configure the integrators
    multi_integrator.set_value(target_level * target_level);
    multi_integrator.set_multipliers(PEAK_INTEGRATOR, {PEAK_INTEGRATION_SECONDS, rate()});
    multi_integrator.set_multipliers(SHORT_INTEGRATOR, {SHORT_INTEGRATION_SECONDS, rate()});
    multi_integrator.set_multipliers(LONG_INTEGRATOR, {LONG_INTEGRATION_SECONDS, rate()});
    multi_integrator.set_smoothing({SMOOTHER_INTEGRATION_SECONDS, rate()});

    // Determine how much we must read ahead for the short integration period to
    // track signals quickly enough.
    read_ahead = multi_integrator.prepare_lookahead(SHORT_INTEGRATOR, target_level * target_level);
    const int * la = multi_integrator.look_ahead();
    const int * rla = multi_integrator.reduced_look_ahead();

    if constexpr (enabled_print_debug) {
        for (size_t i = 0; i < multi_integrator.number_of_integrators(); i++) {
            printf("%s: [%i] read-ahead=%i; reduced read_ahead=%i; scale=%lf\n", name(), (int)i, la[i], rla[i], multi_integrator.scale(i));
        }
    }

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
    multi_integrator.integrate(square_sum);

    double detection = sqrt(multi_integrator.integrated()) * M_SQRT2;
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
