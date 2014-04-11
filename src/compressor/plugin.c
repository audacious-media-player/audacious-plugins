/*
 * Dynamic Range Compression Plugin for Audacious
 * Copyright 2010-2012 John Lindgren
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

#include <audacious/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "compressor.h"

/* What is a "normal" volume?  Replay Gain stuff claims to use 89 dB, but what
 * does that translate to in our PCM range?  Does anybody even know? */
static const char * const compressor_defaults[] = {
 "center", "0.5",
 "range", "0.5",
 NULL};

static const PreferencesWidget compressor_widgets[] = {
 {WIDGET_LABEL, N_("<b>Compression</b>")},
 {WIDGET_SPIN_BTN, N_("Center volume:"),
  .cfg_type = VALUE_FLOAT, .csect = "compressor", .cname = "center",
  .data = {.spin_btn = {0.1, 1, 0.1}}},
 {WIDGET_SPIN_BTN, N_("Dynamic range:"),
  .cfg_type = VALUE_FLOAT, .csect = "compressor", .cname = "range",
  .data = {.spin_btn = {0.0, 3.0, 0.1}}}};

static const PluginPreferences compressor_prefs = {
 .widgets = compressor_widgets,
 .n_widgets = ARRAY_LEN (compressor_widgets)};

void compressor_config_load (void)
{
    aud_config_set_defaults ("compressor", compressor_defaults);
}

static const char compressor_about[] =
 N_("Dynamic Range Compression Plugin for Audacious\n"
    "Copyright 2010-2012 John Lindgren");

AUD_EFFECT_PLUGIN
(
    .name = N_("Dynamic Range Compressor"),
    .domain = PACKAGE,
    .about_text = compressor_about,
    .prefs = & compressor_prefs,
    .init = compressor_init,
    .cleanup = compressor_cleanup,
    .start = compressor_start,
    .process = compressor_process,
    .flush = compressor_flush,
    .finish = compressor_finish,
    .adjust_delay = compressor_adjust_delay,
    .preserves_format = TRUE
)
