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

class Integrator
{
    double history_multiplier_ = 0;
    double input_multiplier_ = 1;

public:
    /**
     * Returns the history multiplier for integration over the provided number
     * of samples, that can be a fraction. If the fraction is smaller than
     * \c minimum_samples, zero is returned. If the number is bigger than
     * \c code maximum_samples the history multiplier for \c maximum_samples} is
     * returned.
     * @param samples The number of samples
     * @return The history multiplier that is equal or larger than zero and
     * smaller than 1.
     */
    static inline double
    calculate_history_multiplier_from_samples(double samples)
    {
        return exp(-1.0 / fabs(samples));
    }

    /**
     * By default, the integrator does not integrate and just passes-through the
     * input.
     */
    constexpr Integrator() = default;

    /**
     * Creates an integrator over the provided number of samples.
     * @param samples The number of samples
     */
    void set_samples(const double samples)
    {
        const double count = fabs(samples);
        history_multiplier_ = count > 0 ? exp(-1.0 / count) : 0.0;
        input_multiplier_ = 1.0 - history_multiplier_;
    }

    /**
     * Creates an integrator over the provided number of samples.
     * @param seconds The number of samples
     * @param rate The sample rate.
     */
    template<typename Seconds, typename SampleRate>
    void set_seconds_for_rate(const Seconds seconds, const SampleRate rate)
    {
        static_assert(std::is_arithmetic_v<Seconds>);
        static_assert(std::is_arithmetic_v<SampleRate>);

        set_samples(static_cast<double>(seconds) * static_cast<double>(rate));
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

class ScaledIntegrator : public Integrator
{
    double scale_ = 1.0;
    double integrated_ = 0;

public:
    // explicit ScaledIntegrator(const Integrator & s) : integrator_(s) {}
    ScaledIntegrator() = default;

    void integrate(double input) { Integrator::integrate(integrated_, input); }

    void set_value(const double new_value) { integrated_ = new_value; }

    void set_scale(const double new_scale)
    {
        if (new_scale >= 0 && new_scale <= 1e6)
        {
            scale_ = new_scale;
        }
    }

    [[nodiscard]] double integrated() const { return scale_ * integrated_; }
};

#endif // AUDACIOUS_PLUGINS_BGM_INTEGRATOR_H
