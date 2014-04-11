/*
 * SoX Resampler Plugin for Audacious
 * Copyright 2013 Michał Lipski
 *
 * Based on Sample Rate Converter Plugin:
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

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <soxr.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <audacious/plugin.h>
#include <libaudcore/preferences.h>

#define MIN_RATE 8000
#define MAX_RATE 192000
#define RATE_STEP 50

#define RESAMPLER_ERROR(e) fprintf (stderr, "sox-resampler: %s\n", e)

static const char * const sox_resampler_defaults[] = {
 "quality", "4", /* SOXR_HQ */
 "rate", "44100",
 NULL};

static soxr_t soxr;
static soxr_error_t error;
static int stored_channels;
static double ratio;
static float * buffer;
static size_t buffer_samples;

bool_t sox_resampler_init (void)
{
    aud_config_set_defaults ("soxr", sox_resampler_defaults);
    return TRUE;
}

void sox_resampler_cleanup (void)
{
    soxr_delete (soxr);
    soxr = 0;
    g_free (buffer);
    buffer = NULL;
    buffer_samples = 0;
}

void sox_resampler_start (int * channels, int * rate)
{
    soxr_delete (soxr);
    soxr = 0;

    int new_rate = aud_get_int ("soxr", "rate");
    new_rate = CLAMP (new_rate, MIN_RATE, MAX_RATE);

    if (new_rate == * rate)
        return;

    soxr_quality_spec_t q = soxr_quality_spec(aud_get_int ("soxr", "quality"), 0);

    soxr = soxr_create((double) * rate, (double) new_rate, * channels, & error, NULL, & q, NULL);

    if (error)
    {
        RESAMPLER_ERROR (error);
        return;
    }

    stored_channels = * channels;
    ratio = (double) new_rate / * rate;
    * rate = new_rate;
}

void do_resample (float * * data, int * samples)
{
    if (! soxr)
         return;

    if (buffer_samples < (int) (* samples * ratio) + 256)
    {
        buffer_samples = (int) (* samples * ratio) + 256;
        buffer = g_renew (float, buffer, buffer_samples);
    }

    size_t samples_done;

    error = soxr_process(soxr, * data, * samples / stored_channels, NULL,
        buffer, buffer_samples / stored_channels, & samples_done);

    if (error)
    {
        RESAMPLER_ERROR (error);
        return;
    }

    * data = buffer;
    * samples = samples_done * stored_channels;
}

void sox_resampler_process (float * * data, int * samples)
{
    do_resample (data, samples);
}

void sox_resampler_flush (void)
{
    if (soxr && (error = soxr_process(soxr, NULL, 0, NULL, NULL, 0, NULL)))
        RESAMPLER_ERROR (error);
}

void sox_resampler_finish (float * * data, int * samples)
{
    do_resample (data, samples);
}

static const char sox_resampler_about[] =
 N_("SoX Resampler Plugin for Audacious\n"
    "Copyright 2013 Michał Lipski\n\n"
    "Based on Sample Rate Converter Plugin:\n"
    "Copyright 2010-2012 John Lindgren");

static const ComboBoxElements method_list[] = {
 {"0", N_("Quick")}, /* SOXR_QQ */
 {"1", N_("Low")}, /* SOXR_QL */
 {"2", N_("Medium")}, /* SOXR_QM */
 {"4", N_("High")}, /* SOXR_QH */
 {"6", N_("Very High")}}; /* SOXR_VHQ */

static const PreferencesWidget sox_resampler_widgets[] = {
 {WIDGET_COMBO_BOX, N_("Quality:"),
  .cfg_type = VALUE_STRING, .csect = "soxr", .cname = "quality",
  .data = {.combo = {method_list, ARRAY_LEN (method_list)}}},
 {WIDGET_SPIN_BTN, N_("Rate:"),
  .cfg_type = VALUE_INT, .csect = "soxr", .cname = "rate",
  .data = {.spin_btn = {MIN_RATE, MAX_RATE, RATE_STEP, N_("Hz")}}}
};

static const PluginPreferences sox_resampler_prefs = {
 .widgets = sox_resampler_widgets,
 .n_widgets = ARRAY_LEN (sox_resampler_widgets)};

AUD_EFFECT_PLUGIN
(
    .name = N_("SoX Resampler"),
    .domain = PACKAGE,
    .about_text = sox_resampler_about,
    .prefs = & sox_resampler_prefs,
    .init = sox_resampler_init,
    .cleanup = sox_resampler_cleanup,
    .start = sox_resampler_start,
    .process = sox_resampler_process,
    .flush = sox_resampler_flush,
    .finish = sox_resampler_finish,
    .order = 2 /* must be before crossfade */
)
