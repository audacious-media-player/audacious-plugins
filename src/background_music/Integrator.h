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

    static inline double calculate_history_multiplier_from_samples(double samples)
    {
        return fabs(samples) < 1e-6 ? 0 : exp(-1.0 / fabs(samples));
    }
};




#endif // AUDACIOUS_PLUGINS_INTEGRATOR_H
