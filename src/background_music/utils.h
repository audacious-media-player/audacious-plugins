#ifndef AUDACIOUS_PLUGINS_BGM_UTILS_H
#define AUDACIOUS_PLUGINS_BGM_UTILS_H
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
#include <limits>
#include <stdexcept>

#ifdef BACKGROUND_MUSIC_PRINT_DEBUG_MESSAGES
#include <cstddef>
#define print_debug(...) printf(__VA_ARGS__);
static constexpr bool enabled_print_debug = true;
#else
#define print_debug(...) //
static constexpr bool enabled_print_debug = false;
#endif

template<typename T>
static inline T decibel_level_to_value(T decibel)
{
    return pow(10.0, 0.05 * decibel);
}

template<typename T>
static constexpr size_t get_max_array_size()
{
    const size_t max = std::numeric_limits<size_t>::max();
    const size_t size = sizeof(T);
    return size > 0 ? max / size : max;
}

template<typename T>
static constexpr size_t verified_above_min_below_max(size_t elements,
                                                     size_t minimum = 1)
{
    if (elements > minimum && elements <= get_max_array_size<T>())
    {
        return elements;
    }
    throw std::invalid_argument(
        "verified_above_min_below_max(elements,minimum): number of elements "
        "below minimum or larger than maximum array size for type.");
}

template<typename T>
static T * allocate_if_valid(size_t elements, size_t limit)
{
    size_t valid_elements = verified_above_min_below_max<T>(elements, 1);
    if (limit == 0 || valid_elements <= limit)
    {
        return new T[elements];
    }
    throw std::invalid_argument("allocate_if_valid(elements,limit): number of "
                                "elements larger than supplied limit");
}

#endif // AUDACIOUS_PLUGINS_BGM_UTILS_H
