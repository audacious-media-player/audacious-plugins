#ifndef AUDACIOUS_PLUGINS_BASIC_CONFIG_H
#define AUDACIOUS_PLUGINS_BASIC_CONFIG_H
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
#include "ConfigValue.h"

static constexpr const char * const CONFIG_SECTION_BACKGROUND_MUSIC =
    "background_music";

static constexpr auto conf_target_level =
    ConfigDouble(CONFIG_SECTION_BACKGROUND_MUSIC)
        .withName(N_("Target level (dB):"))
        .withVariable("target_level")
        .withMinimum(-30.0)
        .withDefault(-12, "-12.0")
        .withMaximum(-6);

static constexpr auto conf_maximum_amplification =
    ConfigDouble(CONFIG_SECTION_BACKGROUND_MUSIC)
        .withName(N_("Maximum amplification (dB):"))
        .withVariable("maximum_amplification")
        .withMinimum(0.0)
        .withDefault(0.0, "10.0")
        .withMaximum(40.0);


#endif // AUDACIOUS_PLUGINS_BASIC_CONFIG_H
