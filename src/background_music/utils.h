#ifndef AUDACIOUS_PLUGINS_UTILS_H
#define AUDACIOUS_PLUGINS_UTILS_H
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
#include <limits>
#include <type_traits>

template<typename T, typename V>
static inline constexpr T narrow_clamp(const V & v)
{
    auto value = v + static_cast<T>(0);
    using W = decltype(value);

    if constexpr (std::numeric_limits<W>::lowest() <
                  std::numeric_limits<T>::lowest())
    {
        if (value < static_cast<W>(std::numeric_limits<T>::lowest()))
        {
            return std::numeric_limits<T>::lowest();
        }
    }
    if constexpr (std::numeric_limits<W>::max() > std::numeric_limits<T>::max())
    {
        if (value > static_cast<W>(std::numeric_limits<T>::max()))
        {
            return std::numeric_limits<T>::max();
        }
    }
    return static_cast<T>(value);
}

template<typename M, typename N, typename D>
static inline bool is_multiple_of(M multiple_of, N numerator, D denominator)
{
    static_assert(std::is_integral_v<M>);
    static_assert(std::is_integral_v<N>);
    static_assert(std::is_integral_v<D>);
    using W = decltype(numerator + denominator + multiple_of);

    return (numerator != 0) && (denominator != 0) && (multiple_of != 0) &&
           ((static_cast<W>(numerator) / static_cast<W>(denominator)) %
            static_cast<W>(multiple_of)) == 0;
}


class Integrator
{
    double history_multiply;
    double input_multiply;

public:
    Integrator();
    explicit Integrator(double samples);
    Integrator(double time, int sample_rate);

    inline void integrate(double & history, double input) const
    {
        history = history_multiply * history + input_multiply * input;
    }

    static inline double
    calculate_history_multiplier_from_samples(double samples);
};

#endif // AUDACIOUS_PLUGINS_UTILS_H
