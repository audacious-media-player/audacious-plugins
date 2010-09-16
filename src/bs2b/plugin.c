/*
 * Audacious bs2b effect plugin
 * Copyright (C) 2009, Sebastian Pipping <sebastian@pipping.org>
 * Copyright (C) 2009, Tony Vroon <chainsaw@gentoo.org>
 * Copyright (C) 2010, John Lindgren <john.lindgren@tds.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <audacious/plugin.h>
#include <bs2b.h>
#define AB_EFFECT_LEVEL BS2B_DEFAULT_CLEVEL

static t_bs2bdp bs2b = NULL;
static gint bs2b_channels;

void init() {
    bs2b = bs2b_open();
    if (bs2b == NULL) {
        return;
    }
    bs2b_set_level(bs2b, AB_EFFECT_LEVEL);
}

static void cleanup() {
    if (bs2b == NULL) {
        return;
    }
    bs2b_close(bs2b);
    bs2b = NULL;
}

static void bs2b_start (gint * channels, gint * rate)
{
    if (bs2b == NULL)
        return;

    bs2b_channels = * channels;

    if (* channels != 2)
        return;

    bs2b_set_srate (bs2b, * rate);
}

static void bs2b_process (gfloat * * data, gint * samples)
{
    if (bs2b == NULL || bs2b_channels != 2)
        return;

    bs2b_cross_feed_f (bs2b, * data, (* samples) / 2);
}

static void bs2b_flush (void)
{
}

static void bs2b_finish (gfloat * * data, gint * samples)
{
    bs2b_process (data, samples);
}

static gint bs2b_decoder_to_output_time (gint time)
{
    return time;
}

static gint bs2b_output_to_decoder_time (gint time)
{
    return time;
}

static EffectPlugin audaciousBs2b = {
    .description = "Bauer stereophonic-to-binaural 1.1",
    .init = init,
    .cleanup = cleanup,
    .start = bs2b_start,
    .process = bs2b_process,
    .flush = bs2b_flush,
    .finish = bs2b_finish,
    .decoder_to_output_time = bs2b_decoder_to_output_time,
    .output_to_decoder_time = bs2b_output_to_decoder_time,
    .preserves_format = TRUE,
};

static EffectPlugin * plugins[] = { &audaciousBs2b, NULL };
SIMPLE_EFFECT_PLUGIN(audaciousBs2b, plugins);
