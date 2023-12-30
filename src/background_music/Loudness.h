#ifndef AUDACIOUS_PLUGINS_BGM_LOUDNESS_H
#define AUDACIOUS_PLUGINS_BGM_LOUDNESS_H
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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <libaudcore/ringbuf.h>

/**
 * Tools to detect perceived loudness.
 */
struct Loudness
{
    static constexpr float perception_center_seconds = 0.400;
    static constexpr float perception_fast_seconds = 0.001;
    static constexpr float perception_peak_seconds = 0.00001;
    static constexpr float perception_weight_power = 0.25;
    static constexpr float max_prediction = 0.03;
    static constexpr float window_percentage = 1.0;

    struct Metrics
    {
        int window_samples = 1;
        int latency_samples = 0;
        float weight = 0;
    };

    static constexpr float get_weight(const float seconds)
    {
        const float relative = std::clamp(seconds, perception_peak_seconds,
                                          perception_center_seconds) /
                               perception_center_seconds;
        return powf(relative, perception_weight_power);
    }
    static int get_samples(const float seconds, const int rate)
    {
        return std::max(1, static_cast<int>(
                               roundf(seconds * static_cast<float>(rate))));
    }

    static Metrics get_metrics(const int step, const int of,
                                     const int rate)
    {
        static constexpr float peak_perception_ratio =
            perception_fast_seconds /
            perception_center_seconds;

        const float log_ratio =
            of > 0 ? static_cast<float>(std::clamp(step, 0, of)) /
                         static_cast<float>(of)
                   : 1.0f;
        const float seconds = perception_center_seconds *
                              powf(peak_perception_ratio, log_ratio);

        const float latency =
            std::min(seconds * window_percentage, max_prediction);

        return {get_samples(seconds, rate), get_samples(latency, rate),
                get_weight(seconds)};
    }
};

class PerceptiveRMS
{
    static constexpr int STEPS = 24;
    static constexpr float INPUT_SCALE = 4e9f;
    static constexpr float OUTPUT_SCALE = 1.0f / INPUT_SCALE;

    class WindowedRMS
    {
        uint64_t window_sum_ = 0;
        int window_size_ = 0;
        int latency_minus_one = 0;
        float scale_;
        float output_;

    public:
        [[nodiscard]] float get() const { return output_; }

        float add_and_take_and_get(uint64_t add, uint64_t take)
        {
            window_sum_ += add;
            window_sum_ -= take;
            const auto sum = static_cast<float>(window_sum_);
            output_ = scale_ * sum;
            return output_;
        }

        void configure(const Loudness::Metrics & metrics)
        {
            window_size_ = metrics.window_samples;
            latency_minus_one = std::max(0, metrics.latency_samples - 1);
            scale_ = metrics.weight * metrics.weight /
                     static_cast<float>(window_size_);
            window_sum_ = 0;
            output_ = 0;
        }

        [[nodiscard]] int delayed_sample_index() const
        {
            return latency_minus_one;
        }
    };

    RingBuf<uint64_t> buffer_;
    WindowedRMS rms_[STEPS + 1];
    int sample_rate_ = 0;
    int latency_ = 0;
    IntegratorCoefficients smooth_release_;
    double smooth_release_int1_ = 0.0;
    double smooth_release_int2 = 0.0;
    int hold_samples_ = 0;
    int hold_count_ = 0;
    const float peak_weight_ = Loudness::get_weight(0.0);

        void init_detection()
    {
        const auto max_metrics =
            Loudness::get_metrics(0, STEPS, sample_rate_);
        latency_ = max_metrics.latency_samples;
        smooth_release_.set_samples(max_metrics.window_samples);
        hold_samples_ = max_metrics.window_samples;
        hold_count_ = 0;

        for (int step = 0; step <= STEPS; step++)
        {
            rms_[step].configure(
                Loudness::get_metrics(step, STEPS, sample_rate_));
        }
    }

    [[nodiscard]] uint64_t static squared_value_to_internal_value(
        const float squared_value)
    {
        return static_cast<uint64_t>(
            fabsf(std::round(squared_value * INPUT_SCALE)));
    }

public:
    void set_rate_and_value(int sample_rate, float squared_initial_value)
    {
        if (sample_rate_ == sample_rate)
        {
            return;
        }
        sample_rate_ = sample_rate;
        init_detection();
        buffer_.discard();
        buffer_.alloc(latency_);
        buffer_.fill_with(0);

        for (int i = 0; i <= latency_; i++)
        {
            get_mean_squared(squared_initial_value);
        }
    }

    [[nodiscard]] int latency() const { return latency_; }

    float get_mean_squared(const float squared_input)
    {
        const uint64_t internal_value =
            squared_value_to_internal_value(squared_input);
        const uint64_t oldest = buffer_.pop();
        buffer_.push(internal_value);

        float max = rms_[0].add_and_take_and_get(internal_value, oldest);
        max = std::max(max, static_cast<float>(internal_value) * peak_weight_);

        for (int step = 1; step <= STEPS; step++)
        {
            WindowedRMS & rms = rms_[step];
            const auto delayed_input =
                buffer_.nth_from_last(rms.delayed_sample_index());
            const auto step_value =
                rms.add_and_take_and_get(internal_value, delayed_input);
            max = std::max(max, step_value);
        }
        max *= OUTPUT_SCALE;
        if (hold_samples_ && max > smooth_release_int2)
        {
            smooth_release_int1_ = smooth_release_int2 = max;
            hold_count_ = hold_samples_;
        }
        else if (hold_count_)
        {
            hold_count_--;
        }
        else
        {
            smooth_release_.integrate(smooth_release_int1_, max);
            smooth_release_.integrate(smooth_release_int2,
                                      smooth_release_int1_);
        }
        return static_cast<float>(smooth_release_int2);
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_LOUDNESS_H
