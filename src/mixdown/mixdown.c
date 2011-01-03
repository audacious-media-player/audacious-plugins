/*
 * Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>
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

#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>

#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static gint in_channel_count = 0;

static void mixdown_start (gint *channels, gint *rate)
{
	in_channel_count = *channels;
	*channels = 1;
}

static void mixdown_process(gfloat **data, gint *samples)
{
	gfloat *x, *y, *end;
	gfloat avg;

	if (in_channel_count != 2 || *samples == 0)
		return;

	for (x = y = *data, end = *data + *samples; x < end; x += 2, y++)
	{
		avg = (x[0] + x[1]) / 2;
		*y = avg;
	}

	*samples /= 2;
}

static void mixdown_flush(void)
{
}

static void mixdown_finish(gfloat **data, gint *samples)
{
	mixdown_process(data, samples);
}

static gint mixdown_decoder_to_output_time (gint time)
{
	return time;
}

static gint mixdown_output_to_decoder_time (gint time)
{
	return time;
}

EffectPlugin mixdown_ep =
{
	.description = "Mixdown Plugin",
	.start = mixdown_start,
	.process = mixdown_process,
	.finish = mixdown_finish,
	.flush = mixdown_flush,
	.decoder_to_output_time = mixdown_decoder_to_output_time,
	.output_to_decoder_time = mixdown_output_to_decoder_time,
};

EffectPlugin *mixdown_eplist[] = { &mixdown_ep, NULL };

SIMPLE_EFFECT_PLUGIN(mixdown, mixdown_eplist);

