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
#include <cmath>
#include <type_traits>

/**
 * Implements a classic RC-filter using an recursive filter with one input and
 * one feedback coefficient.
 *
 * Output of the filter is normalized, meaning that for a constant input, the
 * output will asymptotically approach the same amplitude.
 */
class IntegratorCoefficients
{
    double history_multiplier_ = 0;
    double input_multiplier_ = 1;

public:
    /**
     * By default, the integrator does not integrate and just passes-through the
     * input.
     */
    constexpr IntegratorCoefficients() = default;

    /**
     * Creates an integrator over the provided number of samples.
     * @param samples The number of samples
     */
    void set_samples(const double samples, const double scale = 1.0)
    {
        const double count = fabs(samples);
        history_multiplier_ = count > 0 ? exp(-1.0 / count) : 0.0;
        set_scale(scale);
    }

    /**
     * Creates an integrator over the provided number of samples.
     * @param seconds The number of samples
     * @param rate The sample rate.
     */
    template<typename Seconds, typename SampleRate>
    void set_seconds_for_rate(const Seconds seconds, const SampleRate rate,
                              const double scale = 1.0)
    {
        static_assert(std::is_arithmetic_v<Seconds>);
        static_assert(std::is_arithmetic_v<SampleRate>);

        set_samples(static_cast<double>(seconds) * static_cast<double>(rate),
                    scale);
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
    template<typename T>
    T integrate(const T input)
    {
        static_assert(std::is_arithmetic_v<T>);
        IntegratorCoefficients::integrate(integrated_,
                                          static_cast<double>(input));
        return static_cast<T>(integrated_);
    }

    void set_output(const double new_value) { integrated_ = new_value; }

    [[nodiscard]] double get_output() const { return integrated_; }
};

#endif // AUDACIOUS_PLUGINS_BGM_INTEGRATOR_H
