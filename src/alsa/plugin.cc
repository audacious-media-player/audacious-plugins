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
    WidgetCustom (alsa_create_config_widget)
};

static const PluginPreferences alsa_prefs = {{alsa_widgets}};

#define AUD_PLUGIN_NAME        N_("ALSA Output")
#define AUD_PLUGIN_ABOUT       alsa_about
#define AUD_OUTPUT_PRIORITY    5
#define AUD_PLUGIN_INIT        alsa_init
#define AUD_PLUGIN_CLEANUP     alsa_cleanup
#define AUD_OUTPUT_OPEN        alsa_open_audio
#define AUD_OUTPUT_CLOSE       alsa_close_audio
#define AUD_OUTPUT_GET_FREE    alsa_buffer_free
#define AUD_OUTPUT_WRITE       alsa_write_audio
#define AUD_OUTPUT_WAIT_FREE   alsa_period_wait
#define AUD_OUTPUT_DRAIN       alsa_drain
#define AUD_OUTPUT_GET_TIME    alsa_output_time
#define AUD_OUTPUT_FLUSH       alsa_flush
#define AUD_OUTPUT_PAUSE       alsa_pause
#define AUD_OUTPUT_SET_VOLUME  alsa_set_volume
#define AUD_OUTPUT_GET_VOLUME  alsa_get_volume
#define AUD_PLUGIN_PREFS       & alsa_prefs

#define AUD_DECLARE_OUTPUT
#include <libaudcore/plugin-declare.h>
