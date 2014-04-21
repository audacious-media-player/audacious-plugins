/*
 * Copyright (c) 2008 William Pitcock <nenolod@nenolod.net>
 * Copyright (c) 2010-2012 John Lindgren <john.lindgren@tds.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

static bool_t init (void);
static void cryst_start (int * channels, int * rate);
static void cryst_process (float * * data, int * samples);
static void cryst_flush ();
static void cryst_finish (float * * data, int * samples);

static const char * const cryst_defaults[] = {
 "intensity", "1",
 NULL};

static const PreferencesWidget cryst_widgets[] = {
 {WIDGET_LABEL, N_("<b>Crystalizer</b>")},
 {WIDGET_SPIN_BTN, N_("Intensity:"),
  .cfg_type = VALUE_FLOAT, .csect = "crystalizer", .cname = "intensity",
  .data = {.spin_btn = {0, 10, 0.1}}}};

static const PluginPreferences cryst_prefs = {
 .widgets = cryst_widgets,
 .n_widgets = ARRAY_LEN (cryst_widgets)};

#define AUD_PLUGIN_NAME        N_("Crystalizer")
#define AUD_PLUGIN_PREFS       & cryst_prefs
#define AUD_PLUGIN_INIT        init
#define AUD_EFFECT_START       cryst_start
#define AUD_EFFECT_PROCESS     cryst_process
#define AUD_EFFECT_FLUSH       cryst_flush
#define AUD_EFFECT_FINISH      cryst_finish
#define AUD_EFFECT_SAME_FMT    TRUE

#define AUD_DECLARE_EFFECT
#include <libaudcore/plugin-declare.h>

static int cryst_channels;
static float * cryst_prev;

static bool_t init (void)
{
    aud_config_set_defaults ("crystalizer", cryst_defaults);
    return TRUE;
}

static void cryst_start (int * channels, int * rate)
{
    cryst_channels = * channels;
    cryst_prev = realloc (cryst_prev, sizeof (float) * cryst_channels);
    memset (cryst_prev, 0, sizeof (float) * cryst_channels);
}

static void cryst_process (float * * data, int * samples)
{
    float value = aud_get_double ("crystalizer", "intensity");
    float * f = * data;
    float * end = f + (* samples);
    int channel;

    while (f < end)
    {
        for (channel = 0; channel < cryst_channels; channel ++)
        {
            float current = * f;

            * f ++ = current + (current - cryst_prev[channel]) * value;
            cryst_prev[channel] = current;
        }
    }
}

static void cryst_flush ()
{
    memset (cryst_prev, 0, sizeof (float) * cryst_channels);
}

static void cryst_finish (float * * data, int * samples)
{
    cryst_process (data, samples);
}
