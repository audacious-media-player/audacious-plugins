#ifndef AUDACIOUS_PLUGINS_BGM_BASIC_CONFIG_H
#define AUDACIOUS_PLUGINS_BGM_BASIC_CONFIG_H
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
#include <libaudcore/i18n.h>

static constexpr const char * const CONFIG_SECTION_BACKGROUND_MUSIC =
    "background_music";

static constexpr const char * CONF_TARGET_LEVEL_LABEL =
    N_("Target level (dB):");
static constexpr const char * CONF_TARGET_LEVEL_VARIABLE = "target_level";
static constexpr const char * CONF_TARGET_LEVEL_DEFAULT_STRING = "-12.0";
static constexpr const double CONF_TARGET_LEVEL_MIN = -30.0;
static constexpr const double CONF_TARGET_LEVEL_MAX = -6.0;

static constexpr const char * CONF_MAX_AMPLIFICATION_LABEL =
    N_("Maximum amplification (dB):");
static constexpr const char * CONF_MAX_AMPLIFICATION_VARIABLE =
    "maximum_amplification";
static constexpr const char * CONF_MAX_AMPLIFICATION_DEFAULT_STRING = "10.0";
static constexpr const double CONF_MAX_AMPLIFICATION_MIN = 0.0;
static constexpr const double CONF_MAX_AMPLIFICATION_MAX = 40.0;

static constexpr const char * const background_music_defaults[] = {
    CONF_TARGET_LEVEL_VARIABLE, CONF_TARGET_LEVEL_DEFAULT_STRING,
    CONF_MAX_AMPLIFICATION_VARIABLE, CONF_MAX_AMPLIFICATION_DEFAULT_STRING,
    nullptr};


#endif // AUDACIOUS_PLUGINS_BGM_BASIC_CONFIG_H
