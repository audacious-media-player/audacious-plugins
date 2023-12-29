#ifndef AUDACIOUS_PLUGINS_BGM_DOUBLE_INTEGRATION_TIME_RMS_DETECTION_H
#define AUDACIOUS_PLUGINS_BGM_DOUBLE_INTEGRATION_TIME_RMS_DETECTION_H
/*
 * Background music (equal loudness) Plugin for Audacious
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
#include "Detection.h"
#include "Integrator.h"
#include "Loudness.h"
#include "basic_config.h"
#include "utils.h"
#include <cmath>

class DoubleIntegrationTimeRmsDetection : public Detection
{
    static constexpr float SHORT_INTEGRATION = 0.8;
    static constexpr float LONG_INTEGRATION = 6.3;
    static constexpr double FUDGE_FACTOR = 3;
    static constexpr double BALANCE = 0.3;

    Integrator<double> release_integration;
    Integrator<double> long_integration;
    PerceptiveRMS perceivedLoudness;
    double long_integrated = 0, release_integrated = 0;
    double slow_weight = 0, perceived_weight = 0;
    double target_level = 0.1;
    double maximum_amplification = 1;
    double minimum_detection = 1e-6;
    float amplify = 0;
    int read_ahead_ = 0;

public:
    [[nodiscard]] int read_ahead() const override { return read_ahead_; }

    DoubleIntegrationTimeRmsDetection() : perceivedLoudness(20) {}

    void init() override
    {
        update_config();
        long_integrated = target_level * target_level;
        release_integrated = long_integrated;
        minimum_detection = target_level / maximum_amplification;
    }

    void start(int channels, int rate) override
    {
        update_config();
        // Configure the integrators
        release_integration = {SHORT_INTEGRATION, rate};
        long_integration = {LONG_INTEGRATION / 2.0,
                            rate}; // Happens before sqrt
        perceivedLoudness.set_rate_and_value(rate, target_level);
        read_ahead_ = perceivedLoudness.latency();
        minimum_detection = target_level / maximum_amplification;
        double balance = BALANCE;
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
    }

    void update_config() override
    {
        static constexpr auto SECTION = CONFIG_SECTION_BACKGROUND_MUSIC;
        target_level = decibel_level_to_value(
            std::clamp(aud_get_double(SECTION, CONF_TARGET_LEVEL_VARIABLE),
                       CONF_TARGET_LEVEL_MIN, CONF_TARGET_LEVEL_MAX));
        maximum_amplification = decibel_level_to_value(
            std::clamp(aud_get_double(SECTION, CONF_MAX_AMPLIFICATION_VARIABLE),
                       CONF_MAX_AMPLIFICATION_MIN, CONF_MAX_AMPLIFICATION_MAX));
    }

    void detect(const Index<float> & frame_in) override
    {
        /*
         * Detection is based on a kind-of RMS value. The sum of the squared
         * sample values in each frame is integrated over two time periods. The
         * square root of those integrated values is a pretty nice indication of
         * loudness for a first implementation. The detection does nothing fancy
         * and just uses a weighted sum of the square roots of both
         * integrations. In the future it will use multiple frequency bands,
         * actual RMS, ear-loudness curves and actual perceived weighting of RMS
         * windows.
         */
        double square_sum = 0.0;
        for (float sample : frame_in)
        {
            square_sum += (sample * sample);
        }
        long_integration.integrate(long_integrated, square_sum);
        double perceived = perceivedLoudness.get_mean_squared(square_sum);
        double weighted = std::max(slow_weight * long_integrated,
                                   perceived_weight * perceived);
        double rms = sqrt(weighted) * FUDGE_FACTOR;

        if (rms > release_integrated)
        {
            release_integrated = rms;
        }
        else
        {
            release_integration.integrate(release_integrated, rms);
        }

        amplify =
            target_level / std::max(minimum_detection, release_integrated);
    }

    void apply_detect(Index<float> & frame_out) override
    {
        for (float & sample : frame_out)
        {
            sample *= amplify;
        }
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_DOUBLE_INTEGRATION_TIME_RMS_DETECTION_H
