/*
 * libxmmsstandard - XMMS plugin.
 * Copyright (C) 2000-2001 Konstantin Laevsky <debleek63@yahoo.com>
 *
 * audacious port of the voice removal code from libxmmsstandard
 * by Thomas Cort <linuxgeek@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "config.h"

#include <gtk/gtk.h>
#include <audacious/plugin.h>


static int apply_effect (gpointer *d, gint length, AFormat afmt,
			gint srate, gint nch);
static void query_format (AFormat *fmt, gint *rate, gint *nch);

static EffectPlugin xmms_plugin = {
	.description = "Voice Removal Plugin",
	.mod_samples = apply_effect,
	.query_format = query_format
};

EffectPlugin *voice_eplist[] = { &xmms_plugin, NULL };

DECLARE_PLUGIN(voice_removal, NULL, NULL, NULL, NULL, voice_eplist, NULL, NULL, NULL);

static void query_format (AFormat *fmt, gint *rate, gint *nch)
{
	if (!((*fmt == FMT_S16_NE) ||
		(*fmt == FMT_S16_LE && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
		(*fmt == FMT_S16_BE && G_BYTE_ORDER == G_BIG_ENDIAN)))
		*fmt = FMT_S16_NE;

	if (*nch != 2)
		*nch = 2;
}

static int apply_effect (gpointer *d, gint length, AFormat afmt,
			gint srate, gint nch) {
	int x;
	int left, right;
	gint16 *dataptr = (gint16 *) * d;

	if (!((afmt == FMT_S16_NE) ||
		(afmt == FMT_S16_LE && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
		(afmt == FMT_S16_BE && G_BYTE_ORDER == G_BIG_ENDIAN))   ||
		(nch != 2)) {
		return length;
	}

	for (x = 0; x < length; x += 4) {
		left  = CLAMP(dataptr[1] - dataptr[0], -32768, 32767);
		right = CLAMP(dataptr[0] - dataptr[1], -32768, 32767);
		dataptr[0] = left;
		dataptr[1] = right;
		dataptr += 2;
	}

	return (length);
}
