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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/tuple_formatter.h>

#include "ao.h"
#include "corlett.h"
#include "vio2sf.h"

/* xsf_get_lib: called to load secondary files */
static gchar *path;
int xsf_get_lib(char *filename, void **buffer, unsigned int *length)
{
	void *filebuf;
	gint64 size;
	char buf[PATH_MAX];

	snprintf(buf, PATH_MAX, "%s/%s", dirname(path), filename);

	vfs_file_get_contents (buf, & filebuf, & size);

	*buffer = filebuf;
	*length = (uint64)size;

	return AO_SUCCESS;
}

static gint seek = 0;
Tuple *xsf_tuple(const gchar *filename)
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

	tuple_associate_int(t, FIELD_LENGTH, NULL, c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1);
	tuple_associate_string(t, FIELD_ARTIST, NULL, c->inf_artist);
	tuple_associate_string(t, FIELD_ALBUM, NULL, c->inf_game);
	tuple_associate_string(t, -1, "game", c->inf_game);
	tuple_associate_string(t, FIELD_TITLE, NULL, c->inf_title);
	tuple_associate_string(t, FIELD_COPYRIGHT, NULL, c->inf_copy);
	tuple_associate_string(t, FIELD_QUALITY, NULL, "sequenced");
	tuple_associate_string(t, FIELD_CODEC, NULL, "GBA/Nintendo DS Audio");
	tuple_associate_string(t, -1, "console", "GBA/Nintendo DS");

	free(c);
	g_free(buf);

	return t;
}

gchar *xsf_title(gchar *filename, gint *length)
{
	gchar *title = NULL;
	Tuple *tuple = xsf_tuple(filename);

	if (tuple != NULL)
	{
		title = tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
		*length = tuple_get_int(tuple, FIELD_LENGTH, NULL);
		tuple_free(tuple);
	}
	else
	{
		title = g_path_get_basename(filename);
		*length = -1;
	}

	return title;
}

void xsf_update(unsigned char *buffer, long count, InputPlayback *playback);


static gboolean xsf_play(InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
	void *buffer;
	gint64 size;
	gint length;
	gchar *title = xsf_title(data->filename, &length);
	gint16 samples[44100*2];
	gint seglen = 44100 / 60;
	gfloat pos;

	path = g_strdup(data->filename);
	vfs_file_get_contents (data->filename, & buffer, & size);

	if (xsf_start(buffer, size) != AO_SUCCESS)
	{
		free(buffer);
		return TRUE;
	}

	data->output->open_audio(FMT_S16_NE, 44100, 2);

        data->set_params(data, title, length, 44100*2*2*8, 44100, 2);

	data->playing = TRUE;
	data->set_pb_ready(data);

	for (;;)
	{
		while (data->playing && !seek && !data->eof)
		{
			xsf_gen(samples, seglen);
			xsf_update((guint8 *)samples, seglen * 4, data);

			if (data->output->written_time () > length)
				data->eof = TRUE;
		}

		if (seek)
		{
			if (seek > data->output->written_time ())
			{
				pos = data->output->written_time ();
				while (pos < seek)
				{
					xsf_gen(samples, seglen);
					pos += 16.666;
				}

				data->output->flush(seek);
				seek = 0;

				continue;
			}
			else if (seek < data->output->written_time ())
			{
				data->eof = FALSE;

				g_print("xsf_term\n");
				xsf_term();

				g_print("xsf_start... ");
				if (xsf_start(buffer, size) == AO_SUCCESS)
				{
					g_print("ok!\n");
					pos = 0;
					while (pos < seek)
					{
						xsf_gen(samples, seglen);
						pos += 16.666; /* each segment is 16.666ms */
					}

					data->output->flush(seek);
					seek = 0;

					continue;
				}
				else
				{
					g_print("fail :(\n");

					data->output->close_audio();

					g_free(buffer);
					g_free(path);
				        g_free(title);

					data->playing = FALSE;

					return TRUE;
				}
			}
		}

		xsf_term();

		while (data->eof && data->output->buffer_playing())
			g_usleep(10000);

		data->output->close_audio();

		break;
	}

	g_free(buffer);
	g_free(path);
        g_free(title);

	data->playing = FALSE;
	return FALSE;
}

void xsf_update(guint8 *buffer, long count, InputPlayback *playback)
{
	if (buffer == NULL)
	{
		playback->playing = FALSE;
		playback->eof = TRUE;

		return;
	}

	playback->output->write_audio (buffer, count);
}

void xsf_Stop(InputPlayback *playback)
{
	playback->playing = FALSE;
}

void xsf_pause(InputPlayback *playback, short p)
{
	playback->output->pause(p);
}

gint xsf_is_our_fd(const gchar *filename, VFSFile *file)
{
	gchar magic[4];
	vfs_fread(magic, 1, 4, file);

	if (!memcmp(magic, "PSF$", 4))
		return 1;

	if (!memcmp(magic, "PSF\"", 4))
		return 1;

	return 0;
}

void xsf_Seek(InputPlayback *playback, gulong time)
{
	seek = time;
}

static const gchar *xsf_fmts[] = { "2sf", "mini2sf", "gsf", "minigsf", NULL };

static InputPlugin xsf_ip =
{
	.description = "GSF/2SF Audio Plugin",
	.play = xsf_play,
	.stop = xsf_Stop,
	.pause = xsf_pause,
	.mseek = xsf_Seek,
	.get_song_tuple = xsf_tuple,
	.is_our_file_from_vfs = xsf_is_our_fd,
	.vfs_extensions = xsf_fmts,
};

static InputPlugin *xsf_iplist[] = { &xsf_ip, NULL };

DECLARE_PLUGIN(psf2, NULL, NULL, xsf_iplist, NULL, NULL, NULL, NULL, NULL);

