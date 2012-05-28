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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

static bool_t init (void);
static void cryst_start (int * channels, int * rate);
static void cryst_process (float * * data, int * samples);
static void cryst_flush ();
static void cryst_finish (float * * data, int * samples);

static const char * const cryst_defaults[] = {
 "intensity", "1",
 NULL};

static PreferencesWidget cryst_prefs_widgets[] = {
 {WIDGET_LABEL, N_("<b>Crystalizer</b>")},
 {WIDGET_SPIN_BTN, N_("Intensity:"),
  .cfg_type = VALUE_FLOAT, .csect = "crystalizer", .cname = "intensity",
  .data = {.spin_btn = {0, 10, 0.1}}}};

static PluginPreferences cryst_prefs = {
 .domain = PACKAGE,
 .title = N_("Crystalizer Settings"),
 .prefs = cryst_prefs_widgets,
 .n_prefs = sizeof cryst_prefs_widgets / sizeof cryst_prefs_widgets[0]};

AUD_EFFECT_PLUGIN
(
    .name = "Crystalizer",
    .init = init,
    .settings = & cryst_prefs,
    .start = cryst_start,
    .process = cryst_process,
    .flush = cryst_flush,
    .finish = cryst_finish,
    .preserves_format = TRUE
)

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
