/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2012 John Lindgren
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

#include "alsa.h"

static const char alsa_about[] =
 N_("ALSA Output Plugin for Audacious\n"
    "Copyright 2009-2012 John Lindgren\n\n"
    "My thanks to William Pitcock, author of the ALSA Output Plugin NG, whose "
    "code served as a reference when the ALSA manual was not enough.");

static const PreferencesWidget alsa_widgets[] = {
 {WIDGET_CUSTOM, .data = {.populate = alsa_create_config_widget}}};

static const PluginPreferences alsa_prefs = {
 .widgets = alsa_widgets,
 .n_widgets = ARRAY_LEN (alsa_widgets)};

AUD_OUTPUT_PLUGIN
(
    .name = N_("ALSA Output"),
    .domain = PACKAGE,
    .about_text = alsa_about,
    .probe_priority = 5,
    .init = alsa_init,
    .cleanup = alsa_cleanup,
    .open_audio = alsa_open_audio,
    .close_audio = alsa_close_audio,
    .buffer_free = alsa_buffer_free,
    .write_audio = alsa_write_audio,
    .period_wait = alsa_period_wait,
    .drain = alsa_drain,
    .output_time = alsa_output_time,
    .flush = alsa_flush,
    .pause = alsa_pause,
    .set_volume = alsa_set_volume,
    .get_volume = alsa_get_volume,
    .prefs = & alsa_prefs,
)
