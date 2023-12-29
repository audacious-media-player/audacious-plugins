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
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#include "DoubleIntegrationTimeRmsDetection.h"
#include "FrameBasedEffectPlugin.h"
#include "basic_config.h"

static constexpr const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(N_(CONF_TARGET_LEVEL_LABEL),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONF_TARGET_LEVEL_VARIABLE),
               {CONF_TARGET_LEVEL_MIN, CONF_TARGET_LEVEL_MAX,
                1.0}), // Limited until limiter feature arrives
    WidgetSpin(N_(CONF_MAX_AMPLIFICATION_LABEL),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONF_MAX_AMPLIFICATION_VARIABLE),
               {CONF_MAX_AMPLIFICATION_MIN, CONF_MAX_AMPLIFICATION_MAX, 1.0})};

static constexpr const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static constexpr const char background_music_about[] =
    N_("Background music (equal loudness) Plugin for Audacious\n"
       "Copyright 2023 Michel Fleur");

class BackgroundMusicPlugin final
    : public FrameBasedEffectPlugin
{
public:
    static constexpr PluginInfo info = {N_("Background music (equal loudness)"),
                                        PACKAGE, background_music_about,
                                        &background_music_preferences};

    BackgroundMusicPlugin()
        : FrameBasedEffectPlugin(info, 10)
    {
    }
};

[[maybe_unused]] EXPORT BackgroundMusicPlugin aud_plugin_instance;
