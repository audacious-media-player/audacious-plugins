/*
 * SDL Output Plugin for Audacious
 * Copyright 2010 John Lindgren
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

#include "sdlout.h"

static const char sdlout_about[] =
 N_("SDL Output Plugin for Audacious\n"
    "Copyright 2010 John Lindgren");

AUD_OUTPUT_PLUGIN
(
    .name = N_("SDL Output"),
    .domain = PACKAGE,
    .about_text = sdlout_about,
    .init = sdlout_init,
    .cleanup = sdlout_cleanup,
    .probe_priority = 1,
    .get_volume = sdlout_get_volume,
    .set_volume = sdlout_set_volume,
    .open_audio = sdlout_open_audio,
    .close_audio = sdlout_close_audio,
    .buffer_free = sdlout_buffer_free,
    .period_wait = sdlout_period_wait,
    .write_audio = sdlout_write_audio,
    .drain = sdlout_drain,
    .output_time = sdlout_output_time,
    .pause = sdlout_pause,
    .flush = sdlout_flush
)
