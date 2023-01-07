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
#include <cmath>

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

    inline double integrate(double input)
    {
        integrator_.integrate(integrated_, input);
        return scale_ * integrated_;
    }

    ScaledIntegrator & operator=(const Integrator & source)
    {
        integrator_ = source;
    }

    void set_value(double new_value) { integrated_ = new_value; }

    void set_scale(double new_scale)
    {
        if (new_scale >= 0 && new_scale <= 1e6)
        {
            scale_ = new_scale;
        }
    }

    [[nodiscard]] double integrated() const { return integrated_; }
    [[nodiscard]] double scale() const { return scale_; }
    [[nodiscard]] inline const Integrator & integrator() const
    {
        return integrator_;
    }
};

#endif // AUDACIOUS_PLUGINS_INTEGRATOR_H
