/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#include "configure.h"
#include "plugin.h"

EXPORT ConsolePlugin aud_plugin_instance;

const char ConsolePlugin::about[] =
 N_("Console music decoder engine based on Game_Music_Emu 0.5.2\n"
    "Supported formats: AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM, VGZ\n\n"
    "Audacious plugin by:\n"
    "William Pitcock <nenolod@dereferenced.org>\n"
    "Shay Green <gblargg@gmail.com>");

const char * const ConsolePlugin::exts[] = {
    "ay", "gbs", "gym",
    "hes", "kss", "nsf",
    "nsfe", "sap", "spc",
    "vgm", "vgz", nullptr
};

const PreferencesWidget ConsolePlugin::widgets[] = {
    WidgetLabel (N_("<b>Playback</b>")),
    WidgetSpin (N_("Bass:"),
        WidgetInt (audcfg.bass),
        {-100, 100, 1}),
    WidgetSpin (N_("Treble:"),
        WidgetInt (audcfg.treble),
        {-100, 100, 1}),
    WidgetSpin (N_("Echo:"),
        WidgetInt (audcfg.echo),
        {0, 100, 1}),
    WidgetSpin (N_("Default song length:"),
        WidgetInt (audcfg.loop_length),
        {1, 7200, 1, N_("seconds")}),
    WidgetLabel (N_("<b>Resampling</b>")),
    WidgetCheck (N_("Enable audio resampling"),
        WidgetBool (audcfg.resample)),
    WidgetSpin (N_("Sample rate:"),
        WidgetInt (audcfg.resample_rate),
        {11025, 96000, 100, N_("Hz")},
        WIDGET_CHILD),
    WidgetLabel (N_("<b>SPC</b>")),
    WidgetCheck (N_("Ignore length from SPC tags"),
        WidgetBool (audcfg.ignore_spc_length)),
    WidgetCheck (N_("Increase reverb"),
        WidgetBool (audcfg.inc_spc_reverb))
};

const PluginPreferences ConsolePlugin::prefs = {{widgets}};
