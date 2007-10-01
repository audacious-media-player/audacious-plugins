/*  XMMS - ALSA output plugin
 *    Copyright (C) 2001 Matthieu Sozeau
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "alsa.h"
#include <stdlib.h>

OutputPlugin alsa_op =
{
	.description = "ALSA Output Plugin",
	.init = alsa_init,
	.cleanup = alsa_cleanup,
	.about = alsa_about,
	.configure = alsa_configure,
	.get_volume = alsa_get_volume,
	.set_volume = alsa_set_volume,
	.open_audio = alsa_open,
	.write_audio = alsa_write,
	.close_audio = alsa_close,
	.flush = alsa_flush,
	.pause = alsa_pause,
	.buffer_free = alsa_free,
	.buffer_playing = alsa_playing,
	.output_time = alsa_get_output_time,
	.written_time = alsa_get_written_time,
	.tell_audio = alsa_tell
};

OutputPlugin *alsa_oplist[] = { &alsa_op, NULL };

DECLARE_PLUGIN(alsa, NULL, NULL, NULL, alsa_oplist, NULL, NULL, NULL, NULL)

void alsa_cleanup(void)
{
	if (alsa_cfg.pcm_device) {
		free(alsa_cfg.pcm_device);
		alsa_cfg.pcm_device = NULL;
	}

	if (alsa_cfg.mixer_device) {
		free(alsa_cfg.mixer_device);
		alsa_cfg.mixer_device = NULL;
	}
}
