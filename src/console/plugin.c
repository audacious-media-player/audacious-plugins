/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

#include "configure.h"

Tuple * console_probe_for_tuple(const char *filename, VFSFile *fd);
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
 {WIDGET_LABEL, N_("<b>Playback</b>")},
 {WIDGET_SPIN_BTN, N_("Bass:"), .data = {.spin_btn = {-100, 100, 1}},
  .cfg_type = VALUE_INT, .cfg = & audcfg.bass},
 {WIDGET_SPIN_BTN, N_("Treble:"), .data = {.spin_btn = {-100, 100, 1}},
  .cfg_type = VALUE_INT, .cfg = & audcfg.treble},
 {WIDGET_SPIN_BTN, N_("Echo:"), .data = {.spin_btn = {0, 100, 1}},
  .cfg_type = VALUE_INT, .cfg = & audcfg.echo},
 {WIDGET_SPIN_BTN, N_("Default song length:"),
  .data = {.spin_btn = {-100, 100, 1, N_("seconds")}},
  .cfg_type = VALUE_INT, .cfg = & audcfg.loop_length},
 {WIDGET_LABEL, N_("<b>Resampling</b>")},
 {WIDGET_CHK_BTN, N_("Enable audio resampling"),
  .cfg_type = VALUE_BOOLEAN, .cfg = & audcfg.resample},
 {WIDGET_SPIN_BTN, N_("Resampling rate:"), .child = TRUE,
  .data = {.spin_btn = {11025, 96000, 100, N_("Hz")}},
  .cfg_type = VALUE_INT, .cfg = & audcfg.resample_rate},
 {WIDGET_LABEL, N_("<b>SPC</b>")},
 {WIDGET_CHK_BTN, N_("Ignore length from SPC tags"),
  .cfg_type = VALUE_BOOLEAN, .cfg = & audcfg.ignore_spc_length},
 {WIDGET_CHK_BTN, N_("Increase reverb"),
  .cfg_type = VALUE_BOOLEAN, .cfg = & audcfg.inc_spc_reverb}};

static const PluginPreferences console_prefs = {
 .widgets = console_widgets,
 .n_widgets = ARRAY_LEN (console_widgets)};

AUD_INPUT_PLUGIN
(
    .name = N_("Game Console Music Decoder"),
    .domain = PACKAGE,
    .about_text = console_about,
    .init = console_cfg_load,
    .cleanup = console_cfg_save,
    .prefs = & console_prefs,
    .play = console_play,
    .extensions = gme_fmts,
    .probe_for_tuple = console_probe_for_tuple,
    .have_subtune = TRUE
)
