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
#include "utils.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

template<typename S>
class Integrator
{
    static_assert(std::is_floating_point_v<S>);

    S history_multiplier_ = 0;
    S input_multiplier_ = 1;

public:
    /**
     * Below this fraction of a sample the input multiplier cannot be
     * distinguished from 1.0 and the history multiplier would become de-normal.
     */
    static S minimum_samples()
    {
        static S min = -1.0 / (std::log(std::numeric_limits<S>::epsilon()));
        return min;
    }

    /**
     * Above this number of samples, the history multiplier cannot be
     * distinguished from 1.0, which leads to unstable results.
     */
    static S maximum_samples()
    {
        static S max = -1.0 / std::log(1.0 - std::numeric_limits<S>::epsilon());
        return max;
    }

    /**
     * Returns the history multiplier for integration over the provided number
     * of samples, that can be a fraction. If the fraction is smaller than
     * {@code minimum_samples}, zero is returned. If the number is bigger than
     * {@code maximum_samples} the history multiplier for {@code
     * maximum_samples} is returned.
     * @param samples The number of samples
     * @return the history multiplier that lies between [0 .. 1>.
     */
    static inline S calculate_history_multiplier_from_samples(S samples)
    {
        S absolute = fabs(samples);
        return absolute < minimum_samples()   ? 0
               : absolute > maximum_samples() ? exp(-1 / maximum_samples())
                                              : exp(-1.0 / absolute);
    }

    /**
     * Calculates the history multiplier from a given input multiplier and vice
     * versa.
     * @param multiplier The multiplier
     * @return The other multiplier
     */
    [[maybe_unused]] static inline S calculate_other_multiplier(S multiplier)
    {
        return 1 - multiplier;
    }

    /**
     * Calculates the number of samples for the provided history multiplier. The
     * result is always positive or zero, but not always a whole number. If the
     * history multiplier is not valid, std::invalid_argument is thrown.
     * The choice was made not to return -1 on error, as that easily leads to
     * segfaults.
     */
    [[maybe_unused]] static inline S
    calculate_samples_from_history_multiplier(S history_multiplier)
    {
        double absolute = fabs(history_multiplier);
        if (absolute < std::numeric_limits<S>::epsilon()())
        {
            return 0;
        }
        else if (absolute <= (1 - std::numeric_limits<S>::epsilon()()))
        {
            return 1.0 / log(absolute);
        }
        throw std::invalid_argument(
            "IntegratorBase::calculate_samples_from_history_multiplier("
            "multiplier): multiplier out of range.");
    }

    /**
     * By default, the integrator does not integrate and just passes-through the
     * input.
     */
    constexpr Integrator() {}

    /**
     * Creates an integrator over the provided number of samples.
     * @param samples The number of samples
     */
    [[maybe_unused]] inline explicit Integrator(S samples)
        : history_multiplier_(
              calculate_history_multiplier_from_samples(samples)),
          input_multiplier_(1.0 - history_multiplier_)
    {
    }

    /**
     * Creates an integrator of the the provided number of samples that is
     * calculated from the time and sample rate.
     * @param time The number of time units
     * @param sample_rate The sample-rate samples per unit of time
     */
    [[maybe_unused]] inline Integrator(S time, S sample_rate)
        : Integrator(time * sample_rate)
    {
    }
    template<typename T1, typename T2>
    [[maybe_unused]] inline Integrator(T1 time, T2 sample_rate)
        : Integrator(static_cast<S>(time) * static_cast<S>(sample_rate))
    {
        // Replace with requires in on C++20 adoption
        static_assert(std::is_arithmetic_v<T1>);
        static_assert(std::is_arithmetic_v<T2>);
    }

    /**
     * Integrates value input using history as integrated value.
     * @param history The historic integrated value
     * @param input The new input
     */
    [[maybe_unused]] inline void integrate(S & history, S input) const
    {
        history = history_multiplier_ * history + input_multiplier_ * input;
    }

    /**
     * Integrates value input using history as integrated value.
     * @param history The historic integrated value
     * @param input The new input
     */
    [[maybe_unused]] inline S get_integrated(S history, S input) const
    {
        return history_multiplier_ * history + input_multiplier_ * input;
    }

    /**
     * Getters
     */
    [[nodiscard]] [[maybe_unused]] S history_multiplier() const
    {
        return history_multiplier_;
    }

    [[nodiscard]] [[maybe_unused]] S input_multiplier() const
    {
        return input_multiplier_;
    }

    /**
     * Returns the number of samples that is integrated over. This is calculated
     * from the constants and not totally accurate.
     */
    [[nodiscard]] [[maybe_unused]] S estimate_integration_samples() const
    {
        return -1 / log(history_multiplier_);
    }
};

template<typename S>
class ScaledIntegrator
{
    S scale_ = 1.0;
    S integrated_ = 0;
    Integrator<S> integrator_;

public:
    explicit ScaledIntegrator(const Integrator<S> & s) : integrator_(s) {}
    ScaledIntegrator() = default;

    inline void integrate(S input)
    {
        integrator_.integrate(integrated_, input);
    }

    ScaledIntegrator<S> & operator=(const Integrator<S> & source)
    {
        integrator_ = source;
        return *this;
    }

    void set_value(S new_value) { integrated_ = new_value; }

    void set_scale(S new_scale)
    {
        if (new_scale >= 0 && new_scale <= 1e6)
        {
            scale_ = new_scale;
        }
    }

    [[nodiscard]] S integrated() const { return scale_ * integrated_; }
    [[nodiscard]] S scale() const { return scale_; }
    [[nodiscard]] inline const Integrator<S> & integrator() const
    {
        return integrator_;
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_INTEGRATOR_H
