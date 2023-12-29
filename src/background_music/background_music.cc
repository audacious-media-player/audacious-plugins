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
#include "FrameBasedEffectPlugin.h"
#include "basic_config.h"
#include <cstring>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

static constexpr const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(N_("Target level (dB)"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONF_TARGET_LEVEL_VARIABLE),
               {CONF_TARGET_LEVEL_MIN, CONF_TARGET_LEVEL_MAX, 1.0}),
    WidgetSpin(N_("Maximum amplification"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONF_MAX_AMPLIFICATION_VARIABLE),
               {CONF_MAX_AMPLIFICATION_MIN, CONF_MAX_AMPLIFICATION_MAX, 1.0}),
    WidgetLabel(N_("<b>Advanced</b>")),
    WidgetSpin(
        N_("Detection Balance\n(-1=perception; 1=slow)"),
        WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC, CONF_BALANCE_VARIABLE),
        {CONF_BALANCE_MIN, CONF_BALANCE_MAX, 0.1})
};

static constexpr const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static constexpr const char background_music_about[] =
    N_("Background music (equal loudness) Plugin for Audacious\n"
       "Copyright 2023 Michel Fleur");

[[maybe_unused]] EXPORT FrameBasedEffectPlugin
    aud_plugin_instance({N_("Background music (equal loudness)"), PACKAGE,
                         background_music_about, &background_music_preferences},
                        10);
