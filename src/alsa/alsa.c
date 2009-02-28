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
#include <glib.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <ctype.h>

struct alsa_config alsa_cfg;


static void alsa_cleanup(void)
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


static OutputPluginInitStatus alsa_init(void)
{
	if (dlopen("libasound.so.2", RTLD_NOW | RTLD_GLOBAL) == NULL)
	{
		g_message("Cannot load alsa library: %s", dlerror());
		return OUTPUT_PLUGIN_INIT_FAIL;
	}

	mcs_handle_t *cfgfile;

	memset(&alsa_cfg, 0, sizeof (alsa_cfg));
	
	alsa_cfg.buffer_time = 500;
	alsa_cfg.period_time = 100;
	alsa_cfg.debug = 0;
	alsa_cfg.vol.left = 100;
	alsa_cfg.vol.right = 100;

	cfgfile = aud_cfg_db_open();
	if (!aud_cfg_db_get_string(cfgfile, ALSA_CFGID, "pcm_device",
				  &alsa_cfg.pcm_device))
		alsa_cfg.pcm_device = g_strdup("default");
	g_message("device: %s", alsa_cfg.pcm_device);
	if (!aud_cfg_db_get_string(cfgfile, ALSA_CFGID, "mixer_device",
				  &alsa_cfg.mixer_device))
		alsa_cfg.mixer_device = g_strdup("PCM");
	aud_cfg_db_get_int(cfgfile, ALSA_CFGID, "mixer_card", &alsa_cfg.mixer_card);
	aud_cfg_db_get_int(cfgfile, ALSA_CFGID, "buffer_time", &alsa_cfg.buffer_time);
	aud_cfg_db_get_int(cfgfile, ALSA_CFGID, "period_time", &alsa_cfg.period_time);

	aud_cfg_db_get_bool(cfgfile, ALSA_CFGID, "debug", &alsa_cfg.debug);
	aud_cfg_db_close(cfgfile);

	if (!alsa_hardware_present())
	{
		return OUTPUT_PLUGIN_INIT_NO_DEVICES;
	} else {
		return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
	}
}


static OutputPlugin alsa_op =
{
	.description = "ALSA Output Plugin",
	.probe_priority = 1,
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
