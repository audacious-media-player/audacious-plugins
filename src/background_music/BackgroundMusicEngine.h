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

static constexpr auto conf_target_level = ConfigDouble(BACKGROUND_MUSIC_CONFIG)
                                              .withName("Target level (dB):")
                                              .withVariable("target_level")
                                              .withMinimum(-30.0)
                                              .withDefault(-12, "-12.0")
                                              .withMaximum(-6);

static constexpr auto conf_maximum_amplification =
    ConfigDouble(BACKGROUND_MUSIC_CONFIG)
        .withName("Maximum amplification (dB):")
        .withVariable("maximum_amplification")
        .withMinimum(0.0)
        .withDefault(10, "10.0")
        .withMaximum(40.0);

static constexpr auto conf_slow_measurement =
    ConfigDouble(BACKGROUND_MUSIC_CONFIG)
        .withName("Slow seconds:")
        .withVariable("slow_seconds")
        .withMinimum(1.0)
        .withDefault(6.3, "6.3")
        .withMaximum(10.0);

static constexpr auto conf_fast_measurement =
    ConfigDouble(BACKGROUND_MUSIC_CONFIG)
        .withName("Fast seconds:")
        .withVariable("fast_seconds")
        .withMinimum(0.2)
        .withDefault(0.8, "0.8")
        .withMaximum(1.0);

static constexpr auto conf_perceived_slow_balance = ConfigDouble(BACKGROUND_MUSIC_CONFIG)
                                                  .withName("Perception - slow balance")
                                                  .withVariable("perceived_slow_balance")
                                                  .withMinimum(-1)
                                                  .withDefault(0.3, "0.3")
                                                  .withMaximum(1);

class BackgroundMusicEngine : public FrameBasedPlugin
{
    double target_level;
    double maximum_amplification;
    double perceived_weight;
    double slow_weight;
    double slow_seconds;
    double fast_seconds;
    double minimum_detection;

    size_t processed_frames = 0;
    Index<float> frame_in;
    Index<float> frame_out;

    PerceptiveRMS perceptive_integrator;
    ScaledIntegrator<double> release;
    ScaledIntegrator<double> slow;

    CircularBuffer<float> buffer;
    Index<float> output;
    size_t total_latency = 0;

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
