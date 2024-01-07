#ifndef AUDACIOUS_PLUGINS_BGM_INTEGRATOR_H
#define AUDACIOUS_PLUGINS_BGM_INTEGRATOR_H
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

#include <algorithm>
#include <cmath>
#include <type_traits>

/**
 * Implements a classic RC-filter using an recursive filter with one input and
 * one feedback coefficient.
 * The impulse response of the filter is a decaying exponent that will reach
 * \em 1/e of its original value after the number of "characteristic" samples.
 * Characteristic samples larger than about 400M may yield inaccurate results
 * due to repeated calculations and floating point precision.
 */
class IntegratorCoefficients
{
    /*
     * The constructed defaults constitute an integrator that has zero
     * characteristic samples, meaning it just echoes the input.
     */
    double history_multiplier_ = 0;
    double input_multiplier_ = 1;

public:
    constexpr IntegratorCoefficients() = default;

    void set_samples(const double characteristic_samples,
                     const double scale = 1.0)
    {
        const double count = fabs(characteristic_samples);
        history_multiplier_ = count > 0 ? exp(-1.0 / count) : 0.0;
        set_scale(scale);
    }

    void set_seconds_for_rate(const double seconds, const int rate,
                              const double scale = 1.0)
    {
        set_samples(seconds * static_cast<double>(rate), scale);
    }

    void set_scale(const double scale)
    {
        input_multiplier_ = fabs(scale) * (1.0 - history_multiplier_);
    }

    /**
     * Integrates value input using history as integrated value.
     * @param history The historic integrated value
     * @param input The new input
     */
    void integrate(double & history, const double input) const
    {
        history = history_multiplier_ * history + input_multiplier_ * input;
    }
};

class Integrator : public IntegratorCoefficients
{
    double integrated_ = 0;

public:
    double integrate(const double input)
    {
        IntegratorCoefficients::integrate(integrated_, input);
        return integrated_;
    }

    template<typename T>
    T integrate(const T input)
    {
        return static_cast<T>(integrate(static_cast<double>(input)));
    }

    void set_output(const double new_value) { integrated_ = new_value; }
};

/**
 * Implements an envelope function that follows rises in input - attack -
 * immediately and on input that is lower than the current output, optionally
 * the value for a number of samples and then smoothly integrates towards the
 * lower input value using a double integrator.
 */
class FastAttackSmoothRelease
{
    IntegratorCoefficients coefficients_;
    double integrated_1 = 0;
    double integrated_2 = 0;
    int hold_samples_ = 0;
    int hold_count_ = 0;

public:
    /**
     * Applying double integration, to be really smooth, effectively increases
     * the number of characteristic samples in the impulse response of the
     * output. This factor compensates for that.
     */
    static constexpr double CHARACTERISTIC_SAMPLE_CORRECTION = 0.465941272863;

    void set_samples(const double samples, int hold_samples)
    {
        coefficients_.set_samples(samples * CHARACTERISTIC_SAMPLE_CORRECTION,
                                  1.0);
        hold_samples_ = std::max(0, hold_samples);
    }

    void set_seconds_for_rate(const double seconds, const int rate,
                              int hold_samples)
    {
        coefficients_.set_seconds_for_rate(
            seconds * CHARACTERISTIC_SAMPLE_CORRECTION, rate, 1.0);
        hold_samples_ = std::max(0, hold_samples);
    }

    void set_output(const double output)
    {
        integrated_2 = output;
        integrated_1 = output;
        hold_count_ = hold_samples_;
    }

    double get_envelope(double signal)
    {
        if (signal > integrated_2)
        {
            set_output(signal);
        }
        else if (hold_count_)
        {
            hold_count_--;
        }
        else
        {
            coefficients_.integrate(integrated_1, signal);
            coefficients_.integrate(integrated_2, integrated_1);
        }
        return integrated_2;
    }

    template<typename T>
    T get_envelope(const T signal)
    {
        return static_cast<T>(get_envelope(static_cast<double>(signal)));
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_INTEGRATOR_H
