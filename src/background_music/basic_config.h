#ifndef AUDACIOUS_PLUGINS_BGM_BASIC_CONFIG_H
#define AUDACIOUS_PLUGINS_BGM_BASIC_CONFIG_H
/*
 * audacious-plugins/basic_config.h
 *
 * Added by michel on 2023-09-23
 * Copyright (C) 2015-2023 Michel Fleur.
 * Source https://github.com/emmef/audacious-plugins
 * Email audacious-plugins@emmef.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

#endif // AUDACIOUS_PLUGINS_BGM_BASIC_CONFIG_H
