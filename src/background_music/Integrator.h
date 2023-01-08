#ifndef AUDACIOUS_PLUGINS_INTEGRATOR_H
#define AUDACIOUS_PLUGINS_INTEGRATOR_H
/*
 * RC-circuit style integrator.
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
#include <stdexcept>

class Integrator
{
protected:
    double history_multiply;
    double input_multiply;

public:
    inline Integrator() : history_multiply(0), input_multiply(1) {}
    [[maybe_unused]] inline explicit Integrator(double samples)
        : history_multiply(calculate_history_multiplier_from_samples(samples)),
          input_multiply(1.0 - history_multiply)
    {
    }

    [[maybe_unused]] inline Integrator(double time, int sample_rate)
        : Integrator(time * sample_rate)
    {
    }

    inline void integrate(double & history, double input) const
    {
        history = history_multiply * history + input_multiply * input;
    }

    static inline double
    calculate_history_multiplier_from_samples(double samples)
    {
        return fabs(samples) < 1e-6 ? 0 : exp(-1.0 / fabs(samples));
    }

    [[nodiscard]] double history_multiplication() const
    {
        return history_multiply;
    }
    [[nodiscard]] double input_multiplication() const { return input_multiply; }

    /**
     * Determines how many samples it takes until the threshold is reach when a
     * step response is provided to the integrators.
     *
     * @param squares_integrator The integrator
     * @param maximum_samples After how many samples to bail out.
     * @return The sample count needed to reach teh desired value.
     */
    static int
    get_samples_until_threshold_step(const Integrator & squares_integrator,
                                     const Integrator & smoother_integrator,
                                     double threshold, int maximum_samples)
    {
        double integrated = 0;
        double smooth_intermediate = 0;
        double smooth_integrated = 0;
        for (int i = 0; i < maximum_samples; i++)
        {
            squares_integrator.integrate(integrated, 1);
            smoother_integrator.integrate(smooth_intermediate, integrated);
            smoother_integrator.integrate(smooth_integrated,
                                          smooth_intermediate);
            if (smooth_integrated >= threshold)
            {
                return i;
            }
        }
        return maximum_samples;
    }
};

class ScaledIntegrator
{
    double scale_ = 1.0;
    double integrated_ = 0;
    Integrator integrator_;

public:
    explicit ScaledIntegrator(const Integrator & s) : integrator_(s) {}
    ScaledIntegrator() = default;

    inline void integrate(double input)
    {
        integrator_.integrate(integrated_, input);
    }

    ScaledIntegrator & operator=(const Integrator & source)
    {
        integrator_ = source;
        return *this;
    }

    void set_value(double new_value) { integrated_ = new_value; }

    void set_scale(double new_scale)
    {
        if (new_scale >= 0 && new_scale <= 1e6)
        {
            scale_ = new_scale;
        }
    }

    [[nodiscard]] double integrated() const { return scale_ * integrated_; }
    [[nodiscard]] double scale() const { return scale_; }
    [[nodiscard]] inline const Integrator & integrator() const
    {
        return integrator_;
    }
};

class DoubleIntegrator
{
    double intermediate_ = 0;
    double integrated_ = 0;
    Integrator integrator_;

public:
    explicit DoubleIntegrator(const Integrator & s) : integrator_(s) {}
    DoubleIntegrator() = default;

    inline double integrate(double input)
    {
        integrator_.integrate(intermediate_, input);
        integrator_.integrate(integrated_, intermediate_);
        return integrated_;
    }

    DoubleIntegrator & operator=(const Integrator & source)
    {
        integrator_ = source;
        return *this;
    }

    void set_value(double new_value)
    {
        integrated_ = intermediate_ = new_value;
    }

    [[nodiscard]] double integrated() const { return integrated_; }
    [[nodiscard]] inline const Integrator & integrator() const
    {
        return integrator_;
    }
};

template<size_t N>
class MultiIntegrator
{
    static_assert(N >= 1 && N <= 100); // Ah well, a hundred it is!

    DoubleIntegrator smoothing_;
    ScaledIntegrator integrator_[N];
    int look_ahead_[N];
    int reduced_look_ahead_[N];
    int max_look_ahead = 0;
    CircularBuffer<double> buffer_;

public:
    MultiIntegrator() { reset_lookahead(); }

    [[nodiscard]] const Integrator & integrator(size_t i) const
    {
        if (i < (size_t)N)
        {
            return integrator_[i].integrator();
        }
        throw std::out_of_range(
            "MultiIntegrator::integrator(i): index out of bounds");
    }

    [[nodiscard]] double scale(size_t i) const
    {
        if (i < (size_t)N)
        {
            return integrator_[i].scale();
        }
        throw std::out_of_range(
            "MultiIntegrator::scale(i): index out of bounds");
    }

    [[nodiscard]] const Integrator & smoothing() const
    {
        return smoothing_.integrator();
    }

    [[nodiscard]] size_t number_of_integrators() const { return N; }

    [[nodiscard]] const int * look_ahead() const { return look_ahead_; }
    [[nodiscard]] const int * reduced_look_ahead() const
    {
        return reduced_look_ahead_;
    }

    [[nodiscard]] double integrated() const { return smoothing_.integrated(); }

    void integrate(double input)
    {
        double oldest_value = buffer_.get_and_set(input);

        integrator_[0].integrate(
            reduced_look_ahead_[0] == max_look_ahead
                ? oldest_value
                : buffer_.peek_back(reduced_look_ahead_[0]));
        double max_integrated = integrator_[0].integrated();
        if constexpr (N > 1)
        {
            for (size_t n = 1; n < N; n++)
            {
                double value = reduced_look_ahead_[n] == max_look_ahead
                                   ? oldest_value
                                   : buffer_.peek_back(reduced_look_ahead_[n]);
                integrator_[n].integrate(value);
                max_integrated =
                    std::max(max_integrated, integrator_[n].integrated());
            }
        }
        smoothing_.integrate(max_integrated);
    }

    void set_value(double new_value)
    {
        integrator_[0].set_value(new_value);
        smoothing_.set_value(new_value);
        if constexpr (N > 1)
        {
            for (size_t n = 1; n < N; n++)
            {
                integrator_[n].set_value(new_value);
            }
        }
    }

    void set_multipliers(size_t i, const Integrator & multipliers)
    {
        if (i < (size_t)N)
        {
            integrator_[i] = multipliers;
            return;
        }
        throw std::out_of_range(
            "MultiIntegrator::set_multipliers: index out of bounds");
    }

    void set_scale(size_t i, double scale)
    {
        if (i < (size_t)N)
        {
            integrator_[i].set_scale(scale);
            return;
        }
        throw std::out_of_range(
            "MultiIntegrator::set_scale: index out of bounds");
    }

    void set_smoothing(const Integrator & new_smoothing)
    {
        smoothing_ = new_smoothing;
    }

    void reset_lookahead()
    {
        for (size_t i = 0; i < N; i++)
        {
            look_ahead_[i] = 0;
            reduced_look_ahead_[i] = 0;
        }
    }

    int
    prepare_lookahead(size_t integrator_with_maximum_lookahead,
                      double fill_value,
                      int max_samples_to_try = std::numeric_limits<int>::max())
    {
        if (integrator_with_maximum_lookahead < N)
        {
            double threshold = 1 - 1 / M_E;
            threshold *= threshold;
            max_look_ahead =
                Integrator::get_samples_until_threshold_step(
                    integrator_[integrator_with_maximum_lookahead].integrator(), smoothing_.integrator(),
                    threshold, max_samples_to_try);
            look_ahead_[integrator_with_maximum_lookahead] = max_look_ahead;
            for (size_t n = 0; n < N; n++)
            {
                if (n != integrator_with_maximum_lookahead)
                {
                    look_ahead_[n] = Integrator::get_samples_until_threshold_step(
                        integrator_[n].integrator(), smoothing_.integrator(),
                        threshold, max_look_ahead + 1);
                }
            }
            for (size_t n = 0; n < N; n++)
            {
                reduced_look_ahead_[n] =
                    std::max(0, max_look_ahead - look_ahead_[n]);
            }
            buffer_.set_size(max_look_ahead, fill_value, false);
            return max_look_ahead;
        }
        throw std::out_of_range(
            "MultiIntegrator::calculate_lookahead: index out of bounds");
    }
};

#endif // AUDACIOUS_PLUGINS_INTEGRATOR_H
