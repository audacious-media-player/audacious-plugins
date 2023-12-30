#ifndef AUDACIOUS_PLUGINS_BGM_LOUDNESS_FRAME_PROCESSOR_H
#define AUDACIOUS_PLUGINS_BGM_LOUDNESS_FRAME_PROCESSOR_H
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
#include "Integrator.h"
#include "Loudness.h"
#include "basic_config.h"
#include <cmath>
#include <libaudcore/runtime.h>

class LoudnessFrameProcessor
{
    static constexpr float SHORT_INTEGRATION = 0.001;
    static constexpr float LONG_INTEGRATION = 0.001;
    /*
     * This adjusts the RMS measurement so that it's displayed correctly
     * in the audacious VU meter. It is actually, wrong, but helps with
     * expectation management, as people are normally not used to RMS.
     */
    static constexpr float VU_FUDGE_FACTOR = 3.0f;

    IntegratorCoefficients release_integration;
    IntegratorCoefficients long_integration;
    double long_integrated = 0, release_integrated = 0;
    /**
     * Perceptive loudness.
     */
    PerceptiveRMS perceivedLoudness;
    float slow_weight = 0, perceived_weight = 0;
    float target_level = 0.1;
    float maximum_amplification = 1;
    float perception_slow_balance = 0.3;
    float minimum_detection = 1e-6;
    RingBuf<float> read_ahead_buffer;
    int channels_ = 0;
    int processed_frames = 0;

    static float get_clamped_value(const char * variable, const double minimum,
                                   const double maximum)
    {
        return static_cast<float>(std::clamp(
            aud_get_double(CONFIG_SECTION_BACKGROUND_MUSIC, variable),
            static_cast<double>(minimum), static_cast<double>(maximum)));
    }

    static float get_clamped_decibel_value(const char * variable,
                                           const double minimum,
                                           const double maximum)
    {
        const float decibels = get_clamped_value(variable, minimum, maximum);
        return powf(10.0f, 0.05f * decibels);
    }

public:
    [[nodiscard]] int latency() const { return perceivedLoudness.latency(); }

    LoudnessFrameProcessor()
    {
        aud_config_set_defaults(CONFIG_SECTION_BACKGROUND_MUSIC,
                                background_music_defaults);
    }

    void init()
    {
        update_config();
        long_integrated = 0;
        release_integrated = target_level * target_level;
        minimum_detection = target_level / maximum_amplification;
    }

    void start(const int channels, int rate)
    {
        update_config();
        channels_ = channels;
        processed_frames = 0;
        release_integration.set_seconds_for_rate(SHORT_INTEGRATION, rate);
        /*
         * This RMS (Root-mean-square) calculation integrates squared samples
         * with the RC-style integrator and then draws the square root. This has
         * the effect that rises in averages are tracked twice as fast while
         * decreases are tracked twice as slow. As the decrease is what "counts"
         * for the effective time the signal climbs back up after a peak, we
         * must therefore half the integration time.
         */
        long_integration.set_seconds_for_rate(LONG_INTEGRATION / 2.0, rate);
        perceivedLoudness.set_rate_and_value(rate, target_level);
        // As data is added, then fetched, we need 1 extra frame in the buffer.
        const int alloc_size = channels_ * (latency() + 1);
        
        if (read_ahead_buffer.size() < alloc_size)
        {
            read_ahead_buffer.alloc(alloc_size);
        }
    }

    void update_config()
    {
        target_level = get_clamped_decibel_value(CONF_TARGET_LEVEL_VARIABLE,
                                                 CONF_TARGET_LEVEL_MIN,
                                                 CONF_TARGET_LEVEL_MAX);
        maximum_amplification = get_clamped_decibel_value(
            CONF_MAX_AMPLIFICATION_VARIABLE, CONF_MAX_AMPLIFICATION_MIN,
            CONF_MAX_AMPLIFICATION_MAX);
        perception_slow_balance = get_clamped_value(
            CONF_BALANCE_VARIABLE, CONF_BALANCE_MIN, CONF_BALANCE_MAX);
        minimum_detection = target_level / maximum_amplification;

        if (perception_slow_balance < 0)
        {
            perceived_weight = 1.0;
            slow_weight =
                std::clamp(1.0f + perception_slow_balance, 0.0f, 1.0f);
        }
        else
        {
            perceived_weight = 1.0;
            slow_weight = std::clamp(1.0f + perception_slow_balance, 1.0f, 2.0f);
        }
        slow_weight *= slow_weight;
    }

    bool process_has_output(const Index<float> &frame_in, Index<float> &frame_out)
    {
        read_ahead_buffer.copy_in(frame_in.begin(), channels_);

        float square_sum = 0.0;
        float square_max = 0.0;
        for (const float sample : frame_in)
        {
            const float square = sample * sample;
            square_max = std::max(square_max, square);
            square_sum += square;
        }
        square_sum /= static_cast<float>(channels_);
        square_sum += square_max;
        long_integration.integrate(long_integrated, square_sum);
        const double perceived = perceivedLoudness.get_mean_squared(square_sum);
        const double weighted = std::max(slow_weight * long_integrated,
                                         perceived_weight * perceived);
        const double rms = sqrt(weighted) * VU_FUDGE_FACTOR;
        if (rms > release_integrated)
        {
            release_integrated = rms;
        }
        else
        {
            release_integration.integrate(release_integrated, rms);
        }

        const float gain =
            target_level /
            std::max(minimum_detection, static_cast<float>(release_integrated));

        if (processed_frames >= latency())
        {
            read_ahead_buffer.move_out(frame_out.begin(), channels_);
            for (float & sample : frame_out)
            {
                sample *= gain;
            }
            return true;
        }
        processed_frames++;

        return false;
    }

    void discard_buffer()
    {
        read_ahead_buffer.discard();
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_LOUDNESS_FRAME_PROCESSOR_H
