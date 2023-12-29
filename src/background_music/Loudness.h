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
        static constexpr double perception_center_seconds = 0.400;
        static constexpr double perception_peak_seconds = 0.0003;
        static constexpr double perception_weight_power = 0.25;

        static constexpr double get_relative_seconds(double seconds)
        {
            return std::clamp(seconds, perception_peak_seconds,
                              perception_center_seconds) /
                   perception_center_seconds;
        }

        static constexpr double get_weight(double seconds)
        {
            return pow(get_relative_seconds(seconds), perception_weight_power);
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
        static double get_seconds(size_t step, size_t of)
        {
            static constexpr double peak_perception_ratio =
                Metrics::perception_peak_seconds /
                Metrics::perception_center_seconds;
            double log_ratio =
                of > 0 ? static_cast<double>(std::clamp(step, size_t(0), of)) /
                             static_cast<double>(of)
                       : 1;
            return Metrics::perception_center_seconds *
                   pow(peak_perception_ratio, log_ratio);
        }

        /**
         * This returns seconds and the weight for step "step" of "of" steps.
         */
        static void set_seconds_and_weight(double & seconds, double & weight,
                                           size_t step, size_t of)
        {
            seconds = get_seconds(step, of);
            weight = Metrics::get_weight(seconds);
        }
    };
};

class PerceptiveRMS
{
    static constexpr int steps_ = 20;
    static constexpr double input_scale_ = std::numeric_limits<uint32_t>::max();

    class WindowedRMS
    {
        uint64_t window_sum_ = 0;
        size_t window_size_ = 0;
        size_t hold_samples_ = 0;
        size_t hold_count_ = 0;
        double window_multiplier_ = 1;
        ScaledIntegrator<double> release_;

    public:
        [[nodiscard]] double get() const
        {
            return window_multiplier_ * release_.integrated();
        }

        double add_and_take_and_get(uint64_t add, uint64_t take)
        {
            window_sum_ += add;
            window_sum_ -= take;
            if (hold_samples_ &&
                window_sum_ >= static_cast<uint64_t>(release_.integrated()))
            {
                hold_count_ = hold_samples_;
                release_.set_value(static_cast<double>(window_sum_));
            }
            else if (hold_count_ > 0)
            {
                hold_count_--;
            }
            else
            {
                release_.integrate(static_cast<double>(window_sum_));
            }
            return get();
        }

        void set_multiplier(size_t window_size, double weight,
                            size_t hold_samples)
        {
            window_size_ = window_size;
            window_multiplier_ = window_size > 0
                                     ? weight / static_cast<double>(window_size)
                                     : 1.0;
            hold_samples_ = hold_samples;
            release_ = Integrator<double>(hold_count_);
            hold_count_ = 0;
        }

        void set_value(uint64_t value)
        {
            window_sum_ = static_cast<uint64_t>(std::round(
                static_cast<double>(value * window_sum_) / window_multiplier_));
        }

        [[nodiscard]] size_t window_size() const { return window_size_; }
        [[nodiscard]] [[maybe_unused]] size_t hold_samples() const
        {
            return hold_samples_;
        }
    };

    RingBuf<uint64_t> buffer_;
    WindowedRMS rms_[steps_ + 1];
    int latency_ = 0;

    void init_detection(uint64_t sample_rate)
    {
        const size_t max_window = seconds_to_window(
            Loudness::Metrics::perception_center_seconds, sample_rate);
        for (int step = 0; step <= steps_; step++)
        {
            double seconds;
            double weight;
            Loudness::Spread::set_seconds_and_weight(seconds, weight, step,
                                                     steps_);
            size_t window_size = seconds_to_window(seconds, sample_rate);
            WindowedRMS &rms = rms_[step];
            rms.set_multiplier(window_size,
                                      weight * weight / input_scale_,
                                      max_window - window_size);
            rms.set_value(0);
        }

        latency_ = static_cast<int>(std::min(
            max_window, static_cast<size_t>(std::numeric_limits<int>::max())));
    }

    static size_t seconds_to_window(double seconds, uint64_t sample_rate)
    {
        return static_cast<size_t>(
            std::round(seconds * static_cast<double>(sample_rate)));
    }

    [[nodiscard]] uint64_t squared_value_to_internal_value(double value) const
    {
        return static_cast<uint64_t>(fabs(std::round(value * input_scale_)));
    }

public:
    void set_rate_and_value(uint64_t sample_rate, double squared_initial_value)
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

    [[nodiscard]] size_t latency() const { return latency_; }

    double get_mean_squared(double squared_input)
    {
        const uint64_t internal_value =
            squared_value_to_internal_value(squared_input);
        const uint64_t oldest = buffer_.pop();
        buffer_.push(internal_value);

        double max = rms_[0].add_and_take_and_get(internal_value, oldest);

        for (int step = 1; step <= steps_; step++)
        {
            WindowedRMS &rms = rms_[step];
            const auto input = buffer_.nth_from_last(rms.window_size() - 1);
            const auto step_value =
                rms.add_and_take_and_get(internal_value, input);
            max = std::max(max, step_value);
        }
        return max;
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_LOUDNESS_H
