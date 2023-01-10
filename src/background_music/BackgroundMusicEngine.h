#ifndef AUDACIOUS_PLUGINS_BACKGROUND_MUSIC_PLUGIN_H
#define AUDACIOUS_PLUGINS_BACKGROUND_MUSIC_PLUGIN_H
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

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/ringbuf.h>

#include "FrameBasedPlugin.h"
#include "Integrator.h"
#include "Loudness.h"
#include "utils.h"

static constexpr const char * const BACKGROUND_MUSIC_CONFIG =
    "background_music";

static constexpr auto conf_dynamic_range = ConfigDouble(BACKGROUND_MUSIC_CONFIG)
                                               .withName("Dynamic range")
                                               .withVariable("dynamic_range")
                                               .withMinimum(0.0)
                                               .withDefault(0.0, "0.0")
                                               .withMaximum(1.0);

static constexpr auto conf_target_level = ConfigDouble(BACKGROUND_MUSIC_CONFIG)
                                              .withName("Target level (dB):")
                                              .withVariable("target_level")
                                              .withMinimum(-30.0)
                                              .withDefault(-12, "-12.0")
                                              .withMaximum(-6);

static constexpr auto conf_maximum_amplification =
    ConfigDouble(BACKGROUND_MUSIC_CONFIG)
        .withName("Maximum amplification:")
        .withVariable("maximum_amplification")
        .withMinimum(1.0)
        .withDefault(10, "10.0")
        .withMaximum(100.0);

static constexpr double SMOOTHER_INTEGRATION_SECONDS = 0.0015;

static constexpr double SHORT_INTEGRATION_SECONDS = 0.8;
static constexpr double SHORT_INTEGRATION_WEIGHT = 0.5;

static constexpr double LONG_INTEGRATION_SECONDS = 3.2;

class BackgroundMultiIntegrator : public MultiIntegrator<double>
{
public:
    BackgroundMultiIntegrator(size_t number_of_integrators)
        : MultiIntegrator(number_of_integrators)
    {
    }
    [[maybe_unused]] void set_hold_integrator(const Integrator<double> &);
    [[maybe_unused]] void set_short_hold_integrator(const Integrator<double> & v);

protected:
    double on_integration() override;
    void on_set_value(double value) override;
private:
    ScaledIntegrator<double> peak_hold_integrator;
    ScaledIntegrator<double> short_hold_integrator;
};

class BackgroundMusicEngine : public FrameBasedPlugin
{
    double target_level = 0.3;
    double range = 0;
    double maximum_amplification = 10;
    int processed_frames = 0;
    Index<float> frame_in;
    Index<float> frame_out;

    PerceptiveRMS multi_integrator;
    ScaledIntegrator<double> release;
    ScaledIntegrator<double> slow;
    DoubleIntegrator<double> smooth;

    RingBuf<float> read_ahead_buffer;
    Index<float> output;
    int read_ahead = 0;

public:
    explicit BackgroundMusicEngine(int order);

    [[nodiscard]] bool
    offer_frame_return_if_output(const Index<float> & in,
                                 Index<float> & out) override;
    bool on_flush(bool force) override;
    [[nodiscard]] const char * name() const override;
    void update_config(bool is_processing) override;
    [[nodiscard]] int latency() const override;
    bool on_init() override;
    void on_start(int channels, int rate) override;
    void on_cleanup(bool was_enabled) override;
    bool after_finished(bool end_of_playlist) override;
    ;
};

#endif // AUDACIOUS_PLUGINS_BACKGROUND_MUSIC_PLUGIN_H
