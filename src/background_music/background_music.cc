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
#include <libaudcore/ringbuf.h>

#include "basic_config.h"
#include "FrameBasedEffectPlugin.h"
#include "DoubleIntegrationTimeRmsDetection.h"

/*
 * The standard "center" volume is set to 0.25 while the dynamic range is set to
 * zero, meaning all songs will be equally loud according to the detected
 * loudness, that is.
 */
static constexpr const char * const background_music_defaults[] = {
    conf_target_level.variable, conf_target_level.default_string,
    conf_maximum_amplification.variable,
    conf_maximum_amplification.default_string};

static constexpr const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(
        N_(conf_target_level.name),
        WidgetFloat(conf_target_level.section, conf_target_level.variable),
        {conf_target_level.value_min, conf_target_level.value_max,
         1.0}), // Limited until limiter feature arrives
    WidgetSpin(N_(conf_maximum_amplification.name),
               WidgetFloat(conf_maximum_amplification.section,
                           conf_maximum_amplification.variable),
               {conf_maximum_amplification.value_min,
                conf_maximum_amplification.value_max, 1.0})};

static constexpr const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static const char background_music_about[] =
    N_("Background music (equal loudness) Plugin for Audacious\n"
       "Copyright 2023 Michel Fleur");



class BackgroundMusicPlugin : public FrameBasedEffectPlugin<DoubleIntegrationTimeRmsDetection> {
public:
    static constexpr PluginInfo info = {N_("Background music (equal loudness)"),
                                        PACKAGE, background_music_about,
                                        &background_music_preferences};

    BackgroundMusicPlugin() : FrameBasedEffectPlugin<DoubleIntegrationTimeRmsDetection>(info, 10) {}

    bool after_init() override
    {
        aud_config_set_defaults(CONFIG_SECTION_BACKGROUND_MUSIC, background_music_defaults);
        detection->init();
        return true;
    }
};


[[maybe_unused]] EXPORT BackgroundMusicPlugin aud_plugin_instance;

