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

#include <audacious/plugin.h>

#include "ao.h"
#include "corlett.h"
#include "vio2sf.h"

/* xsf_get_lib: called to load secondary files */
static gchar *path;
int xsf_get_lib(char *filename, void **buffer, unsigned int *length)
{
	guchar *filebuf;
	gsize size;
	char buf[PATH_MAX];

	snprintf(buf, PATH_MAX, "%s/%s", dirname(path), filename);

	aud_vfs_file_get_contents(buf, (gchar **) &filebuf, &size);

	*buffer = filebuf;
	*length = (uint64)size;

	return AO_SUCCESS;
}

static gint seek = 0;
Tuple *xsf_tuple(gchar *filename)
{
	Tuple *t;
	corlett_t *c;
	guchar *buf;
	gsize sz;

	aud_vfs_file_get_contents(filename, (gchar **) &buf, &sz);

	if (!buf)
		return NULL;

	if (corlett_decode(buf, sz, NULL, NULL, &c) != AO_SUCCESS)
		return NULL;	

	t = aud_tuple_new_from_filename(filename);

	aud_tuple_associate_int(t, FIELD_LENGTH, NULL, c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1);
	aud_tuple_associate_string(t, FIELD_ARTIST, NULL, c->inf_artist);
	aud_tuple_associate_string(t, FIELD_ALBUM, NULL, c->inf_game);
	aud_tuple_associate_string(t, -1, "game", c->inf_game);
	aud_tuple_associate_string(t, FIELD_TITLE, NULL, c->inf_title);
	aud_tuple_associate_string(t, FIELD_COPYRIGHT, NULL, c->inf_copy);
	aud_tuple_associate_string(t, FIELD_QUALITY, NULL, "sequenced");
	aud_tuple_associate_string(t, FIELD_CODEC, NULL, "Nintendo DS Audio");
	aud_tuple_associate_string(t, -1, "console", "Nintendo DS");

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
		title = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
		*length = aud_tuple_get_int(tuple, FIELD_LENGTH, NULL);
		aud_tuple_free(tuple);
	}
	else
	{
		title = g_path_get_basename(filename);
		*length = -1;
	}

	return title;
}

void xsf_update(unsigned char *buffer, long count, InputPlayback *playback);

void xsf_play(InputPlayback *data)
{
	guchar *buffer;
	gsize size;
	gint length;
	gchar *title = xsf_title(data->filename, &length);
	gint16 samples[44100*2];
	gint seglen = 44100 / 60;

	path = g_strdup(data->filename);
	aud_vfs_file_get_contents(data->filename, (gchar **) &buffer, &size);

	if (xsf_start(buffer, size) != AO_SUCCESS)
	{
		free(buffer);
		return;
	}
	
	data->output->open_audio(FMT_S16_NE, 44100, 2);

        data->set_params(data, title, length, 44100*2*2*8, 44100, 2);

	data->playing = TRUE;
	data->set_pb_ready(data);

	for (;;)
	{
		while (data->playing && !seek)
		{
			xsf_gen(samples, seglen);
			xsf_update(samples, seglen * 4, data);

			if (data->output->written_time() > (length / 1000))
				data->eof = TRUE;
		}

		if (seek)
		{
			data->eof = FALSE;
			data->output->flush(seek);

			xsf_term();

			if (xsf_start(buffer, size) == AO_SUCCESS)
			{
				//xsf_seek(seek);
				seek = 0;
				continue;
			}
			else
			{
				data->output->close_audio();
				break;
			}
		}

		xsf_term();

		data->output->buffer_free();
		data->output->buffer_free();

		while (data->eof && data->output->buffer_playing())
			g_usleep(10000);

		data->output->close_audio();

		break;
	}	

	g_free(buffer);
	g_free(path);
        g_free(title);

	data->playing = FALSE;
}

void xsf_update(unsigned char *buffer, long count, InputPlayback *playback)
{
	const int mask = ~((((16 / 8) * 2)) - 1);

	if (buffer == NULL)
	{
		playback->playing = FALSE;
		playback->eof = TRUE;

		return;
	}

	while (count > 0)
	{
		int t = playback->output->buffer_free() & mask;
		if (t > count)
			playback->pass_audio(playback, FMT_S16_NE, 2, count, buffer, NULL);
		else
		{
			if (t)
				playback->pass_audio(playback, FMT_S16_NE, 2, t, buffer, NULL);

			g_usleep((count-t)*1000*5/441/2);
		}
		count -= t;
		buffer += t;
	}

#if 0
	if (seek)
	{
		if (xsf_seek(seek))
		{
			playback->output->flush(seek);
			seek = 0;
		}
		else
		{
			playback->eof = TRUE;
			return;
		}
	}
#endif
}

void xsf_Stop(InputPlayback *playback)
{
	playback->playing = FALSE;
}

void xsf_pause(InputPlayback *playback, short p)
{
	playback->output->pause(p);
}

int xsf_is_our_fd(gchar *filename, VFSFile *file)
{
	gchar magic[4];
	aud_vfs_fread(magic, 1, 4, file);

	if (!memcmp(magic, "PSF$", 4))
		return 1;

	return 0;
}

void xsf_Seek(InputPlayback *playback, int time)
{
	seek = time * 1000;
}

gchar *xsf_fmts[] = { "2sf", "mini2sf", NULL };

InputPlugin xsf_ip =
{
	.description = "2SF Audio Plugin",
	.play_file = xsf_play,
	.stop = xsf_Stop,
	.pause = xsf_pause,
	.seek = xsf_Seek,
	.get_song_tuple = xsf_tuple,
	.is_our_file_from_vfs = xsf_is_our_fd,
	.vfs_extensions = xsf_fmts,
};

InputPlugin *xsf_iplist[] = { &xsf_ip, NULL };

DECLARE_PLUGIN(psf2, NULL, NULL, xsf_iplist, NULL, NULL, NULL, NULL, NULL);

