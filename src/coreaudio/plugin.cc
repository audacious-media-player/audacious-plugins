/*
 * CoreAudio Output Plugin for Audacious
 * Copyright 2014 William Pitcock
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

#include "coreaudio.h"

static const char about[] =
 N_("CoreAudio Output Plugin for Audacious\n"
    "Copyright 2014 William Pitcock\n\n"
    "Based on SDL Output Plugin for Audacious\n"
    "Copyright 2010 John Lindgren");

#define AUD_PLUGIN_NAME        N_("CoreAudio Output")
#define AUD_PLUGIN_ABOUT       about
#define AUD_PLUGIN_INIT        coreaudio::init
#define AUD_PLUGIN_CLEANUP     coreaudio::cleanup
#define AUD_OUTPUT_PRIORITY    1
#define AUD_OUTPUT_GET_VOLUME  coreaudio::get_volume
#define AUD_OUTPUT_SET_VOLUME  coreaudio::set_volume
#define AUD_OUTPUT_OPEN        coreaudio::open_audio
#define AUD_OUTPUT_CLOSE       coreaudio::close_audio
#define AUD_OUTPUT_GET_FREE    coreaudio::buffer_free
#define AUD_OUTPUT_WAIT_FREE   coreaudio::period_wait
#define AUD_OUTPUT_WRITE       coreaudio::write_audio
#define AUD_OUTPUT_DRAIN       coreaudio::drain
#define AUD_OUTPUT_GET_TIME    coreaudio::output_time
#define AUD_OUTPUT_PAUSE       coreaudio::pause
#define AUD_OUTPUT_FLUSH       coreaudio::flush

#define AUD_DECLARE_OUTPUT
#include <libaudcore/plugin-declare.h>
