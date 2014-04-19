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

#define AUD_PLUGIN_NAME        N_("SDL Output")
#define AUD_PLUGIN_ABOUT       sdlout_about
#define AUD_PLUGIN_INIT        sdlout_init
#define AUD_PLUGIN_CLEANUP     sdlout_cleanup
#define AUD_OUTPUT_PRIORITY    1
#define AUD_OUTPUT_GET_VOLUME  sdlout_get_volume
#define AUD_OUTPUT_SET_VOLUME  sdlout_set_volume
#define AUD_OUTPUT_OPEN        sdlout_open_audio
#define AUD_OUTPUT_CLOSE       sdlout_close_audio
#define AUD_OUTPUT_GET_FREE    sdlout_buffer_free
#define AUD_OUTPUT_WAIT_FREE   sdlout_period_wait
#define AUD_OUTPUT_WRITE       sdlout_write_audio
#define AUD_OUTPUT_DRAIN       sdlout_drain
#define AUD_OUTPUT_GET_TIME    sdlout_output_time
#define AUD_OUTPUT_PAUSE       sdlout_pause
#define AUD_OUTPUT_FLUSH       sdlout_flush

#define AUD_DECLARE_OUTPUT
#include <libaudcore/plugin-declare.h>
