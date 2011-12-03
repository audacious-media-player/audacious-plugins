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

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "ao.h"
#include "corlett.h"
#include "vio2sf.h"

static GMutex *seek_mutex;
static GCond *seek_cond;
static gint seek_value = -1;
static gboolean stop_flag = FALSE;

static gboolean xsf_init(void)
{
	seek_mutex = g_mutex_new();
	seek_cond = g_cond_new();

	return TRUE;
}

static void xsf_cleanup(void)
{
	g_mutex_free(seek_mutex);
	g_cond_free(seek_cond);
}

/* xsf_get_lib: called to load secondary files */
static gchar *path;
int xsf_get_lib(char *filename, void **buffer, unsigned int *length)
{
	void *filebuf;
	gint64 size;

	gchar * path2 = g_strdup_printf ("%s/%s", dirname (path), filename);
	vfs_file_get_contents (path2, & filebuf, & size);
	g_free (path2);

	*buffer = filebuf;
	*length = (uint64)size;

	return AO_SUCCESS;
}

Tuple *xsf_tuple(const gchar *filename, VFSFile *fd)
{
	Tuple *t;
	corlett_t *c;
	void *buf;
	gint64 sz;

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
	tuple_set_str(t, FIELD_QUALITY, NULL, "sequenced");
	tuple_set_str(t, FIELD_CODEC, NULL, "GBA/Nintendo DS Audio");
	tuple_set_str(t, -1, "console", "GBA/Nintendo DS");

	free(c);
	g_free(buf);

	return t;
}

static gint xsf_get_length(const gchar *filename)
{
	corlett_t *c;
	void *buf;
	gint64 size;

	vfs_file_get_contents(filename, &buf, &size);

	if (!buf)
		return -1;

	if (corlett_decode(buf, size, NULL, NULL, &c) != AO_SUCCESS)
	{
		g_free(buf);
		return -1;
	}

	free(c);
	g_free(buf);

	return c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1;
}

static gboolean xsf_play(InputPlayback * playback, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
	void *buffer;
	gint64 size;
	gint length = xsf_get_length(filename);
	gint16 samples[44100*2];
	gint seglen = 44100 / 60;
	gfloat pos;
	gboolean error = FALSE;

	path = g_strdup(filename);
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
		g_mutex_lock(seek_mutex);

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

				g_cond_signal(seek_cond);
			}
			else if (seek_value < playback->output->written_time ())
			{
				xsf_term();

				g_free(path);
				path = g_strdup(filename);

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

					g_cond_signal(seek_cond);
				}
			   	else
				{
					error = TRUE;
					goto CLEANUP;
				}
			}
		}

		g_mutex_unlock(seek_mutex);

		xsf_gen(samples, seglen);
		playback->output->write_audio((guint8 *)samples, seglen * 4);

		if (playback->output->written_time() >= length)
		{
			while (!stop_flag && playback->output->buffer_playing())
				g_usleep(10000);

			goto CLEANUP;
		}
	}

CLEANUP:
	xsf_term();

	g_mutex_lock(seek_mutex);
	stop_flag = TRUE;
	g_cond_signal(seek_cond); /* wake up any waiting request */
	g_mutex_unlock(seek_mutex);

	playback->output->close_audio();

ERR_NO_CLOSE:
	g_free(buffer);
	g_free(path);

	return !error;
}

void xsf_stop(InputPlayback *playback)
{
	g_mutex_lock(seek_mutex);

	if (!stop_flag)
	{
		stop_flag = TRUE;
		playback->output->abort_write();
		g_cond_signal(seek_cond);
	}

	g_mutex_unlock (seek_mutex);
}

void xsf_pause(InputPlayback *playback, gboolean pause)
{
	g_mutex_lock(seek_mutex);

	if (!stop_flag)
		playback->output->pause(pause);

	g_mutex_unlock(seek_mutex);
}

gint xsf_is_our_fd(const gchar *filename, VFSFile *file)
{
	gchar magic[4];
	if (vfs_fread(magic, 1, 4, file) < 4)
		return FALSE;

	if (!memcmp(magic, "PSF$", 4))
		return 1;

	return 0;
}

void xsf_seek(InputPlayback *playback, gint time)
{
	g_mutex_lock(seek_mutex);

	if (!stop_flag)
	{
		seek_value = time;
		playback->output->abort_write();
		g_cond_signal(seek_cond);
		g_cond_wait(seek_cond, seek_mutex);
	}

	g_mutex_unlock(seek_mutex);
}

static const gchar *xsf_fmts[] = { "2sf", "mini2sf", NULL };

AUD_INPUT_PLUGIN
(
	.name = "2SF Audio",
	.init = xsf_init,
	.cleanup = xsf_cleanup,
	.play = xsf_play,
	.stop = xsf_stop,
	.pause = xsf_pause,
	.mseek = xsf_seek,
	.probe_for_tuple = xsf_tuple,
	.is_our_file_from_vfs = xsf_is_our_fd,
	.extensions = xsf_fmts,
)
