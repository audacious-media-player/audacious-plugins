/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <audacious/i18n.h>
#include <audacious/plugin.h>

static int voice_channels;

static void voice_start(int *channels, int *rate)
{
	voice_channels = *channels;
}

static void voice_process(float **d, int *samples)
{
	float *f = *d, *end;
	end = *d + *samples;

	if (voice_channels != 2 || samples == 0)
		return;

	for (f = *d; f < end; f += 2)
	{
		f[0] -= f[1];
		f[1] = f[0];
	}
}

static void voice_finish(float **d, int *samples)
{
	voice_process(d, samples);
}

AUD_EFFECT_PLUGIN
(
	.name = N_("Voice Removal"),
	.domain = PACKAGE,
	.start = voice_start,
	.process = voice_process,
	.finish = voice_finish
)
