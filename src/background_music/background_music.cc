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
#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>

static constexpr const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(N_("Target level:"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONF_TARGET_LEVEL_VARIABLE),
               {CONF_TARGET_LEVEL_MIN, CONF_TARGET_LEVEL_MAX, 1.0, N_("dB")}),
    WidgetSpin(N_("Maximum amplification:"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONF_MAX_AMPLIFICATION_VARIABLE),
               {CONF_MAX_AMPLIFICATION_MIN, CONF_MAX_AMPLIFICATION_MAX, 1.0,
                N_("dB")}),
    WidgetLabel(N_("<b>Advanced</b>")),
    WidgetSpin(
        N_("Slow detection weight:"),
        WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC, CONF_SLOW_WEIGHT_VARIABLE),
        {CONF_SLOW_WEIGHT_MIN, CONF_SLOW_WEIGHT_MAX, 0.1}),
    WidgetLabel(N_("<b>Hint</b>")),
    WidgetLabel(
        N_("Slow detection weight is the relative weight\n"
           "of a long term average detection when compared\n"
           "to the actual, faster loudness detection.\n"
           "A value of zero gives a more radio-like sound\n"
           "where soft passages get \"pulled up\" more quickly,\n"
           "a value of two makes the sound feel less compressed."))};

static constexpr const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static constexpr const char background_music_about[] =
    N_("Background Music Plugin for Audacious\n"
       "Copyright 2023;2024 Michel Fleur\n\n"
       "Controls the volume to make the sound equally loud within and between "
       "tracks.\n "
       "It uses accurate loudness measurement and tries to make the volume "
       "changes sound natural without audible peaks, yet without lowering "
       "the volume before a peak in advance.");

[[maybe_unused]] EXPORT FrameBasedEffectPlugin
    aud_plugin_instance({N_("Background Music"), PACKAGE,
                         background_music_about, &background_music_preferences},
                        10);
