/*
 * Copyright (c) 2008 William Pitcock <nenolod@nenolod.net>
 * Copyright (c) 2010 John Lindgren <john.lindgren@tds.net>
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

#include <glib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

static gboolean init (void);
static void cryst_start (gint * channels, gint * rate);
static void cryst_process (gfloat * * data, gint * samples);
static void cryst_flush ();
static void cryst_finish (gfloat * * data, gint * samples);
static gint cryst_decoder_to_output_time (gint time);
static gint cryst_output_to_decoder_time (gint time);

static const gchar * const cryst_defaults[] = {
 "intensity", "1",
 NULL};

static PreferencesWidget cryst_prefs_widgets[] = {
 {WIDGET_LABEL, N_("<b>Effect</b>")},
 {WIDGET_SPIN_BTN, N_("Intensity:"),
  .cfg_type = VALUE_FLOAT, .csect = "crystalizer", .cname = "intensity",
  .data = {.spin_btn = {0, 10, 0.1}}}};

static PluginPreferences cryst_prefs = {
 .domain = PACKAGE,
 .title = N_("Crystalizer Settings"),
 .prefs = cryst_prefs_widgets,
 .n_prefs = G_N_ELEMENTS (cryst_prefs_widgets)};

AUD_EFFECT_PLUGIN
(
    .name = "Crystalizer",
    .init = init,
    .settings = & cryst_prefs,
    .start = cryst_start,
    .process = cryst_process,
    .flush = cryst_flush,
    .finish = cryst_finish,
    .decoder_to_output_time = cryst_decoder_to_output_time,
    .output_to_decoder_time = cryst_output_to_decoder_time,
    .preserves_format = TRUE,
)

static gint cryst_channels;
static gfloat * cryst_prev;

static gboolean init (void)
{
    aud_config_set_defaults ("crystalizer", cryst_defaults);
    return TRUE;
}

static void cryst_start (gint * channels, gint * rate)
{
    cryst_channels = * channels;
    cryst_prev = g_realloc (cryst_prev, sizeof (gfloat) * cryst_channels);
    memset (cryst_prev, 0, sizeof (gfloat) * cryst_channels);
}

static void cryst_process (gfloat * * data, gint * samples)
{
    gfloat value = aud_get_double ("crystalizer", "intensity");
    gfloat * f = * data;
    gfloat * end = f + (* samples);
    gint channel;

    while (f < end)
    {
        for (channel = 0; channel < cryst_channels; channel ++)
        {
            gfloat current = * f;

            * f ++ = current + (current - cryst_prev[channel]) * value;
            cryst_prev[channel] = current;
        }
    }
}

static void cryst_flush ()
{
    memset (cryst_prev, 0, sizeof (gfloat) * cryst_channels);
}

static void cryst_finish (gfloat * * data, gint * samples)
{
    cryst_process (data, samples);
}

static gint cryst_decoder_to_output_time (gint time)
{
    return time;
}

static gint cryst_output_to_decoder_time (gint time)
{
    return time;
}
