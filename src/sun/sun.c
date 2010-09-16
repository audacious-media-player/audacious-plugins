/*
 *  Copyright (C) 2001  CubeSoft Communications, Inc.
 *  <http://www.csoft.org>
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

#include <glib.h>

#include "sun.h"
#include <audacious/i18n.h>

#include <errno.h>

struct sun_audio audio;

OutputPlugin sun_op =
{
	.description = "BSD/Sun Output Plugin",	/* Description */
	.probe_priority = 5,
	.init = sun_init,
	.cleanup = sun_cleanup,
	.about = sun_about,
	.configure = sun_configure,
	.get_volume = sun_get_volume,
	.set_volume = sun_set_volume,
	.open_audio = sun_open,
	.write_audio = sun_write,
	.close_audio = sun_close,
	.flush = sun_flush,
	.pause = sun_pause,
	.buffer_free = sun_free,
	.buffer_playing = sun_playing,
	.output_time = sun_output_time,
	.written_time = sun_written_time,
};

OutputPlugin *sun_oplist[] = { &sun_op, NULL };

SIMPLE_OUTPUT_PLUGIN(sun, sun_oplist);

OutputPluginInitStatus sun_init(void)
{
	mcs_handle_t *cfgfile;
	char *s;

	memset(&audio, 0, sizeof(struct sun_audio));

	cfgfile = aud_cfg_db_open();
	/* Devices */
	aud_cfg_db_get_string(cfgfile, "sun", "audio_devaudio", &audio.devaudio);
	aud_cfg_db_get_string(cfgfile, "sun",
			     "audio_devaudioctl", &audio.devaudioctl);
	aud_cfg_db_get_string(cfgfile, "sun", "audio_devmixer", &audio.devmixer);

	/* Buffering */
	aud_cfg_db_get_int(cfgfile, "sun",
			  "buffer_size", &audio.req_buffer_size);
	aud_cfg_db_get_int(cfgfile, "sun",
			  "prebuffer_size", &audio.req_prebuffer_size);

	/* Mixer */
	aud_cfg_db_get_string(cfgfile, "sun", "mixer_voldev", &audio.mixer_voldev);
	aud_cfg_db_get_bool(cfgfile, "sun",
			      "mixer_keepopen", &audio.mixer_keepopen);

	aud_cfg_db_close(cfgfile);

	/* Audio device path */
	if ((s = getenv("AUDIODEVICE")))
		audio.devaudio = g_strdup(s);
	else if (!audio.devaudio || !strcmp("", audio.devaudio))
		audio.devaudio = g_strdup(SUN_DEV_AUDIO);

	/* Audio control device path */
	if (!audio.devaudioctl || !strcmp("", audio.devaudioctl))
		audio.devaudioctl = g_strdup(SUN_DEV_AUDIOCTL);

	/* Mixer device path */
	if ((s = getenv("MIXERDEVICE")))
		audio.devmixer = g_strdup(s);
	else if (!audio.devmixer || !strcmp("", audio.devmixer))
		audio.devmixer = g_strdup(SUN_DEV_MIXER);

	if (!audio.mixer_voldev || !strcmp("", audio.mixer_voldev))
		audio.mixer_voldev = g_strdup(SUN_DEFAULT_VOLUME_DEV);

	/* Default buffering settings */
	if (!audio.req_buffer_size)
		audio.req_buffer_size = SUN_DEFAULT_BUFFER_SIZE;
	if (!audio.req_prebuffer_size)
		audio.req_prebuffer_size = SUN_DEFAULT_PREBUFFER_SIZE;

	audio.input = NULL;
	audio.output = NULL;
	audio.effect = NULL;

	if (pthread_mutex_init(&audio.mixer_mutex, NULL) != 0)
		perror("mixer_mutex");

	return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}

void sun_cleanup(void)
{
	g_free(audio.devaudio);
	g_free(audio.devaudioctl);
	g_free(audio.devmixer);
	g_free(audio.mixer_voldev);

	if (!pthread_mutex_lock(&audio.mixer_mutex))
	{
		if (audio.mixerfd)
			close(audio.mixerfd);
		pthread_mutex_unlock(&audio.mixer_mutex);
		pthread_mutex_destroy(&audio.mixer_mutex);
	}
}

