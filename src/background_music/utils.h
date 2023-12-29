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

#endif // AUDACIOUS_PLUGINS_BGM_UTILS_H
