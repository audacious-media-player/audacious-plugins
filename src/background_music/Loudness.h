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

#include <libaudcore/ringbuf.h>

/**
 * Tools to detect perceived loudness.
 */
class Loudness
{
public:
    /**
     * Loudness perception (IBU R128 if I'm correct) based on RMS measurement.
     */
    struct Metrics
    {
        static constexpr float perception_center_seconds = 0.400;
        static constexpr float perception_peak_seconds = 0.0003;
        static constexpr float perception_weight_power = 0.25;

        static constexpr float get_relative_seconds(const float seconds)
        {
            return std::clamp(seconds, perception_peak_seconds,
                              perception_center_seconds) /
                   perception_center_seconds;
        }

        static constexpr float get_weight(const float seconds)
        {
            return powf(get_relative_seconds(seconds), perception_weight_power);
        }
    };
    /**
     * Provides tools to create equidistant RMS window sizes for perceptive
     * measurement between the perceptive center and perceptive peak. It
     * provides the seconds and weight for a certain step. Step 0 always
     * yields the center window and step N, denoted as "of" parameters,
     * yields the peak window.
     */
    struct Spread
    {

        /**
         * This returns seconds for step "step" of "of" steps.
         */
        static float get_seconds(const int step, const int of)
        {
            static constexpr float peak_perception_ratio =
                Metrics::perception_peak_seconds /
                Metrics::perception_center_seconds;
            const float log_ratio =
                of > 0 ? static_cast<float>(std::clamp(step, 0, of)) /
                             static_cast<float>(of)
                       : 1.0f;
            return Metrics::perception_center_seconds *
                   powf(peak_perception_ratio, log_ratio);
        }

        /**
         * This returns seconds and the weight for step "step" of "of" steps.
         */
        static void set_seconds_and_weight(float & seconds, float & weight,
                                           int step, int of)
        {
            seconds = get_seconds(step, of);
            weight = Metrics::get_weight(seconds);
        }
    };
};

class PerceptiveRMS
{
    static constexpr int steps_ = 20;
    static constexpr float input_scale_ = 4e9f;

    class WindowedRMS
    {
        uint64_t window_sum_ = 0;
        int window_size_ = 0;
        int hold_samples_ = 0;
        int hold_count_ = 0;
        float window_multiplier_ = 1;
        ScaledIntegrator<float> release_;

    public:
        [[nodiscard]] float get() const
        {
            return window_multiplier_ * release_.integrated();
        }

        float add_and_take_and_get(uint64_t add, uint64_t take)
        {
            window_sum_ += add;
            window_sum_ -= take;
            if (hold_samples_ &&
                window_sum_ >= static_cast<uint64_t>(release_.integrated()))
            {
                hold_count_ = hold_samples_;
                release_.set_value(static_cast<float>(window_sum_));
            }
            else if (hold_count_ > 0)
            {
                hold_count_--;
            }
            else
            {
                release_.integrate(static_cast<float>(window_sum_));
            }
            return get();
        }

        void set_multiplier(int window_size, float weight, int hold_samples)
        {
            window_size_ = window_size;
            window_multiplier_ = window_size > 0
                                     ? weight / static_cast<float>(window_size)
                                     : 1.0f;
            hold_samples_ = hold_samples;
            release_.set_samples(static_cast<float>(hold_count_));
            hold_count_ = 0;
        }

        void set_value(uint64_t value)
        {
            window_sum_ = static_cast<uint64_t>(std::round(
                static_cast<float>(value * window_sum_) / window_multiplier_));
        }

        [[nodiscard]] int window_size() const { return window_size_; }
        [[nodiscard]] [[maybe_unused]] int hold_samples() const
        {
            return hold_samples_;
        }
    };

    RingBuf<uint64_t> buffer_;
    WindowedRMS rms_[steps_ + 1];
    int latency_ = 0;

    void init_detection(const int sample_rate)
    {
        const int max_window = seconds_to_window(
            Loudness::Metrics::perception_center_seconds, sample_rate);
        for (int step = 0; step <= steps_; step++)
        {
            float seconds;
            float weight;
            Loudness::Spread::set_seconds_and_weight(seconds, weight, step,
                                                     steps_);
            const int window_size = seconds_to_window(seconds, sample_rate);
            WindowedRMS & rms = rms_[step];
            const float effective_weight = weight * weight / input_scale_;
            rms.set_multiplier(window_size,
                               static_cast<float>(effective_weight),
                               max_window - window_size);
            rms.set_value(0);
        }

        latency_ = static_cast<int>(std::min(
            max_window, static_cast<int>(std::numeric_limits<int>::max())));
    }

    static int seconds_to_window(const float seconds, const int sample_rate)
    {
        return static_cast<int>(
            std::round(seconds * static_cast<float>(sample_rate)));
    }

    [[nodiscard]] uint64_t static squared_value_to_internal_value(
        const float squared_value)
    {
        return static_cast<uint64_t>(
            fabsf(std::round(squared_value * input_scale_)));
    }

public:
    void set_rate_and_value(int sample_rate, float squared_initial_value)
    {
        init_detection(sample_rate);
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

        for (int step = 1; step <= steps_; step++)
        {
            WindowedRMS & rms = rms_[step];
            const auto input = buffer_.nth_from_last(rms.window_size() - 1);
            const auto step_value =
                rms.add_and_take_and_get(internal_value, input);
            max = std::max(max, step_value);
        }
        return max;
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_LOUDNESS_H
