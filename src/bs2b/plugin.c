/*
 * Audacious bs2b effect plugin
 * Copyright (C) 2009, Sebastian Pipping <sebastian@pipping.org>
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
#include <bs2bversion.h>
#define AB_EFFECT_LEVEL BS2B_DEFAULT_CLEVEL

static t_bs2bdp bs2b = NULL;
static gint bs2b_srate = BS2B_DEFAULT_SRATE;


static void cleanup() {
	if (bs2b == NULL) {
		return;
	}
	bs2b_close(bs2b);
	bs2b = NULL;
}


static gint mod_samples(gpointer * data, gint length, AFormat fmt, gint srate, gint nch) {
	if ((data == NULL) || (*data == NULL) || (nch != 2)) {
		return length;
	}

	if (srate != bs2b_srate) {
		bs2b_set_srate(bs2b, srate);
		bs2b_srate = srate;
	}

	switch (fmt) {
	case FMT_FLOAT:
		{
			const gint num = length / sizeof(float) / 2;
			float * const sample = (float *)*data;
			bs2b_cross_feed_f(bs2b, sample, num);
		}
		break;
	case FMT_S32_NE:
		{
			const gint num = length / sizeof(int32_t) / 2;
			int32_t * const sample = (int32_t *)*data;
			bs2b_cross_feed_s32(bs2b, sample, num);
		}
		break;
	case FMT_S32_BE:
		{
			const gint num = length / sizeof(int32_t) / 2;
			int32_t * const sample = (int32_t *)*data;
			bs2b_cross_feed_s32be(bs2b, sample, num);
		}
		break;
	case FMT_S32_LE:
		{
			const gint num = length / sizeof(int32_t) / 2;
			int32_t * const sample = (int32_t *)*data;
			bs2b_cross_feed_s32le(bs2b, sample, num);
		}
		break;
	case FMT_U32_NE:
		{
			const gint num = length / sizeof(uint32_t) / 2;
			uint32_t * const sample = (uint32_t *)*data;
			bs2b_cross_feed_u32(bs2b, sample, num);
		}
		break;
	case FMT_U32_BE:
		{
			const gint num = length / sizeof(uint32_t) / 2;
			uint32_t * const sample = (uint32_t *)*data;
			bs2b_cross_feed_u32be(bs2b, sample, num);
		}
		break;
	case FMT_U32_LE:
		{
			const gint num = length / sizeof(uint32_t) / 2;
			uint32_t * const sample = (uint32_t *)*data;
			bs2b_cross_feed_u32le(bs2b, sample, num);
		}
		break;
	case FMT_S16_NE:
		{
			const gint num = length / sizeof(int16_t) / 2;
			int16_t * const sample = (int16_t *)*data;
			bs2b_cross_feed_s16(bs2b, sample, num);
		}
		break;
	case FMT_S16_BE:
		{
			const gint num = length / sizeof(int16_t) / 2;
			int16_t * const sample = (int16_t *)*data;
			bs2b_cross_feed_s16be(bs2b, sample, num);
		}
		break;
	case FMT_S16_LE:
		{
			const gint num = length / sizeof(int16_t) / 2;
			int16_t * const sample = (int16_t *)*data;
			bs2b_cross_feed_s16le(bs2b, sample, num);
		}
		break;
	case FMT_U16_NE:
		{
			const gint num = length / sizeof(uint16_t) / 2;
			uint16_t * const sample = (uint16_t *)*data;
			bs2b_cross_feed_u16(bs2b, sample, num);
		}
		break;
	case FMT_U16_BE:
		{
			const gint num = length / sizeof(uint16_t) / 2;
			uint16_t * const sample = (uint16_t *)*data;
			bs2b_cross_feed_u16be(bs2b, sample, num);
		}
		break;
	case FMT_U16_LE:
		{
			const gint num = length / sizeof(uint16_t) / 2;
			uint16_t * const sample = (uint16_t *)*data;
			bs2b_cross_feed_u16le(bs2b, sample, num);
		}
		break;
	case FMT_S8:
		{
			const gint num = length / sizeof(int8_t) / 2;
			int8_t * const sample = (int8_t *)*data;
			bs2b_cross_feed_s8(bs2b, sample, num);
		}
		break;
	case FMT_U8:
		{
			const gint num = length / sizeof(uint8_t) / 2;
			uint8_t * const sample = (uint8_t *)*data;
			bs2b_cross_feed_u8(bs2b, sample, num);
		}
		break;
	default:
		{
		;
		}
	}
	return length;
}


static void init();


static EffectPlugin audaciousBs2b = {
	.description = "Bauer stereophonic-to-binaural " BS2B_VERSION_STR,
	.init = init,
	.cleanup = cleanup,
	.mod_samples = mod_samples
};


void init() {
	bs2b = bs2b_open();
	if (bs2b == NULL) {
		return;
	}
	bs2b_set_level(bs2b, AB_EFFECT_LEVEL);
}


static EffectPlugin * plugins[] = { &audaciousBs2b, NULL };
SIMPLE_EFFECT_PLUGIN(audaciousBs2b, plugins);
