/*
	Audio Overload SDK - main driver.  for demonstration only, not user friendly!

	Copyright (c) 2007-2008 R. Belmont and Richard Bannister.

	All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
	* Neither the names of R. Belmont and Richard Bannister nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "ao.h"
#include "corlett.h"
#include "vio2sf.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int seek_value = -1;
static bool_t stop_flag = FALSE;

/* xsf_get_lib: called to load secondary files */
static char *path;
int xsf_get_lib(char *filename, void **buffer, unsigned int *length)
{
	void *filebuf;
	int64_t size;

	char *dirpath = dirname(path);
	SPRINTF(path2, "%s/%s", dirpath, filename);
	vfs_file_get_contents(path2, &filebuf, &size);

	*buffer = filebuf;
	*length = (uint64_t)size;

	return AO_SUCCESS;
}

Tuple *xsf_tuple(const char *filename, VFSFile *fd)
{
	Tuple *t;
	corlett_t *c;
	void *buf;
	int64_t sz;

	vfs_file_get_contents (filename, & buf, & sz);

	if (!buf)
		return NULL;

	if (corlett_decode(buf, sz, NULL, NULL, &c) != AO_SUCCESS)
		return NULL;

	t = tuple_new_from_filename(filename);

	tuple_set_int(t, FIELD_LENGTH, NULL, c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1);
	tuple_set_str(t, FIELD_ARTIST, NULL, c->inf_artist);
	tuple_set_str(t, FIELD_ALBUM, NULL, c->inf_game);
	tuple_set_str(t, -1, "game", c->inf_game);
	tuple_set_str(t, FIELD_TITLE, NULL, c->inf_title);
	tuple_set_str(t, FIELD_COPYRIGHT, NULL, c->inf_copy);
	tuple_set_str(t, FIELD_QUALITY, NULL, _("sequenced"));
	tuple_set_str(t, FIELD_CODEC, NULL, "GBA/Nintendo DS Audio");
	tuple_set_str(t, -1, "console", "GBA/Nintendo DS");

	free(c);
	free(buf);

	return t;
}

static int xsf_get_length(const char *filename)
{
	corlett_t *c;
	void *buf;
	int64_t size;

	vfs_file_get_contents(filename, &buf, &size);

	if (!buf)
		return -1;

	if (corlett_decode(buf, size, NULL, NULL, &c) != AO_SUCCESS)
	{
		free(buf);
		return -1;
	}

	free(c);
	free(buf);

	return c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1;
}

static bool_t xsf_play(InputPlayback * playback, const char * filename, VFSFile * file, int start_time, int stop_time, bool_t pause)
{
	void *buffer;
	int64_t size;
	int length = xsf_get_length(filename);
	int16_t samples[44100*2];
	int seglen = 44100 / 60;
	float pos;
	bool_t error = FALSE;

	path = strdup(filename);
	vfs_file_get_contents (filename, & buffer, & size);

	if (xsf_start(buffer, size) != AO_SUCCESS)
	{
		error = TRUE;
		goto ERR_NO_CLOSE;
	}

	if (!playback->output->open_audio(FMT_S16_NE, 44100, 2))
	{
		error = TRUE;
		goto ERR_NO_CLOSE;
	}

	playback->set_params(playback, 44100*2*2*8, 44100, 2);

	if (pause)
		playback->output->pause (TRUE);

	stop_flag = FALSE;
	playback->set_pb_ready(playback);

	while (!stop_flag)
	{
		pthread_mutex_lock (& mutex);

		if (seek_value >= 0)
		{
			if (seek_value > playback->output->written_time ())
			{
				pos = playback->output->written_time ();

				while (pos < seek_value)
				{
					xsf_gen(samples, seglen);
					pos += 16.666;
				}

				playback->output->flush(seek_value);
				seek_value = -1;
			}
			else if (seek_value < playback->output->written_time ())
			{
				xsf_term();

				free(path);
				path = strdup(filename);

				if (xsf_start(buffer, size) == AO_SUCCESS)
				{
					pos = 0;
					while (pos < seek_value)
					{
						xsf_gen(samples, seglen);
						pos += 16.666; /* each segment is 16.666ms */
					}

					playback->output->flush(seek_value);
					seek_value = -1;
				}
			   	else
				{
					error = TRUE;
					goto CLEANUP;
				}
			}
		}

		pthread_mutex_unlock (& mutex);

		xsf_gen(samples, seglen);
		playback->output->write_audio((uint8_t *)samples, seglen * 4);

		if (playback->output->written_time() >= length)
			goto CLEANUP;
	}

CLEANUP:
	xsf_term();

	pthread_mutex_lock (& mutex);
	stop_flag = TRUE;
	pthread_mutex_unlock (& mutex);

ERR_NO_CLOSE:
	free(buffer);
	free(path);

	return !error;
}

void xsf_stop(InputPlayback *playback)
{
	pthread_mutex_lock (& mutex);

	if (!stop_flag)
	{
		stop_flag = TRUE;
		playback->output->abort_write();
	}

	pthread_mutex_unlock (& mutex);
}

void xsf_pause(InputPlayback *playback, bool_t pause)
{
	pthread_mutex_lock (& mutex);

	if (!stop_flag)
		playback->output->pause(pause);

	pthread_mutex_unlock (& mutex);
}

int xsf_is_our_fd(const char *filename, VFSFile *file)
{
	char magic[4];
	if (vfs_fread(magic, 1, 4, file) < 4)
		return FALSE;

	if (!memcmp(magic, "PSF$", 4))
		return 1;

	return 0;
}

void xsf_seek(InputPlayback *playback, int time)
{
	pthread_mutex_lock (& mutex);

	if (!stop_flag)
	{
		seek_value = time;
		playback->output->abort_write();
	}

	pthread_mutex_unlock (& mutex);
}

static const char *xsf_fmts[] = { "2sf", "mini2sf", NULL };

AUD_INPUT_PLUGIN
(
	.name = N_("2SF Decoder"),
	.domain = PACKAGE,
	.play = xsf_play,
	.stop = xsf_stop,
	.pause = xsf_pause,
	.mseek = xsf_seek,
	.probe_for_tuple = xsf_tuple,
	.is_our_file_from_vfs = xsf_is_our_fd,
	.extensions = xsf_fmts,
)
