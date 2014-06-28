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

#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "compressor.h"

/* What is a "normal" volume?  Replay Gain stuff claims to use 89 dB, but what
 * does that translate to in our PCM range?  Does anybody even know? */
static const char * const compressor_defaults[] = {
 "center", "0.5",
 "range", "0.5",
 nullptr};

static const PreferencesWidget compressor_widgets[] = {
    WidgetLabel (N_("<b>Compression</b>")),
    WidgetSpin (N_("Center volume:"),
        WidgetFloat ("compressor", "center"),
        {0.1, 1, 0.1}),
    WidgetSpin (N_("Dynamic range:"),
        WidgetFloat ("compressor", "range"),
        {0.0, 3.0, 0.1})
};

static const PluginPreferences compressor_prefs = {
    compressor_widgets,
    ARRAY_LEN (compressor_widgets)
};

void compressor_config_load (void)
{
    aud_config_set_defaults ("compressor", compressor_defaults);
}

static const char compressor_about[] =
 N_("Dynamic Range Compression Plugin for Audacious\n"
    "Copyright 2010-2012 John Lindgren");

#define AUD_PLUGIN_NAME        N_("Dynamic Range Compressor")
#define AUD_PLUGIN_ABOUT       compressor_about
#define AUD_PLUGIN_PREFS       & compressor_prefs
#define AUD_PLUGIN_INIT        compressor_init
#define AUD_PLUGIN_CLEANUP     compressor_cleanup
#define AUD_EFFECT_START       compressor_start
#define AUD_EFFECT_PROCESS     compressor_process
#define AUD_EFFECT_FLUSH       compressor_flush
#define AUD_EFFECT_FINISH      compressor_finish
#define AUD_EFFECT_ADJ_DELAY   compressor_adjust_delay
#define AUD_EFFECT_SAME_FMT    true

#define AUD_DECLARE_EFFECT
#include <libaudcore/plugin-declare.h>
