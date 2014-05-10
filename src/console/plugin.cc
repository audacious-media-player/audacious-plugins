/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#include "configure.h"

Tuple console_probe_for_tuple(const char *filename, VFSFile *fd);
bool_t console_play(const char *filename, VFSFile *file);

static const char console_about[] =
 N_("Console music decoder engine based on Game_Music_Emu 0.5.2\n"
    "Supported formats: AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM, VGZ\n\n"
    "Audacious plugin by:\n"
    "William Pitcock <nenolod@dereferenced.org>\n"
    "Shay Green <gblargg@gmail.com>");

static const char *gme_fmts[] = {
    "ay", "gbs", "gym",
    "hes", "kss", "nsf",
    "nsfe", "sap", "spc",
    "vgm", "vgz", NULL
};

static const PreferencesWidget console_widgets[] = {
    WidgetLabel (N_("<b>Playback</b>")),
    WidgetSpin (N_("Bass:"),
        {VALUE_INT, & audcfg.bass},
        {-100, 100, 1}),
    WidgetSpin (N_("Treble:"),
        {VALUE_INT, & audcfg.treble},
        {-100, 100, 1}),
    WidgetSpin (N_("Echo:"),
        {VALUE_INT, & audcfg.echo},
        {0, 100, 1}),
    WidgetSpin (N_("Default song length:"),
        {VALUE_INT, & audcfg.loop_length},
        {-100, 100, 1, N_("seconds")}),
    WidgetLabel (N_("<b>Resampling</b>")),
    WidgetCheck (N_("Enable audio resampling"),
        {VALUE_BOOLEAN, & audcfg.resample}),
    WidgetSpin (N_("Resampling rate:"),
        {VALUE_INT, & audcfg.resample_rate},
        {11025, 96000, 100, N_("Hz")},
        WIDGET_CHILD),
    WidgetLabel (N_("<b>SPC</b>")),
    WidgetCheck (N_("Ignore length from SPC tags"),
        {VALUE_BOOLEAN, & audcfg.ignore_spc_length}),
    WidgetCheck (N_("Increase reverb"),
        {VALUE_BOOLEAN, & audcfg.inc_spc_reverb})
};

static const PluginPreferences console_prefs = {
 .widgets = console_widgets,
 .n_widgets = ARRAY_LEN (console_widgets)};

#define AUD_PLUGIN_NAME        N_("Game Console Music Decoder")
#define AUD_PLUGIN_ABOUT       console_about
#define AUD_PLUGIN_INIT        console_cfg_load
#define AUD_PLUGIN_CLEANUP     console_cfg_save
#define AUD_PLUGIN_PREFS       & console_prefs
#define AUD_INPUT_IS_OUR_FILE  NULL
#define AUD_INPUT_PLAY         console_play
#define AUD_INPUT_EXTS         gme_fmts
#define AUD_INPUT_READ_TUPLE   console_probe_for_tuple
#define AUD_INPUT_SUBTUNES     TRUE

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
