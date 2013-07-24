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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "ao.h"
#include "corlett.h"
#include "eng_protos.h"

typedef enum {
    ENG_NONE = 0,
    ENG_PSF1,
    ENG_PSF2,
    ENG_SPX,
    ENG_COUNT
} PSFEngine;

typedef struct {
    int32_t (*start)(uint8_t *buffer, uint32_t length);
    int32_t (*stop)(void);
    int32_t (*seek)(uint32_t);
    int32_t (*execute)(InputPlayback *data);
} PSFEngineFunctors;

static PSFEngineFunctors psf_functor_map[ENG_COUNT] = {
    {NULL, NULL, NULL, NULL},
    {psf_start, psf_stop, psf_seek, psf_execute},
    {psf2_start, psf2_stop, psf2_seek, psf2_execute},
    {spx_start, spx_stop, psf_seek, spx_execute},
};

static PSFEngine psf_probe(uint8_t *buffer)
{
	if (!memcmp(buffer, "PSF\x01", 4))
		return ENG_PSF1;

	if (!memcmp(buffer, "PSF\x02", 4))
		return ENG_PSF2;

	if (!memcmp(buffer, "SPU", 3))
		return ENG_SPX;

	if (!memcmp(buffer, "SPX", 3))
		return ENG_SPX;

	return ENG_NONE;
}

/* ao_get_lib: called to load secondary files */
static char *path;

int ao_get_lib(char *filename, uint8_t **buffer, uint64_t *length)
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

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int seek = 0;
bool_t stop_flag = FALSE;

Tuple *psf2_tuple(const char *filename, VFSFile *file)
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
	tuple_set_str(t, FIELD_CODEC, NULL, "PlayStation 1/2 Audio");
	tuple_set_str(t, -1, "console", "PlayStation 1/2");

	free(c);
	free(buf);

	return t;
}

static bool_t psf2_play(InputPlayback * data, const char * filename, VFSFile * file, int start_time, int stop_time, bool_t pause)
{
	void *buffer;
	int64_t size;
	PSFEngine eng;
	PSFEngineFunctors *f;
	bool_t error = FALSE;

	path = strdup(filename);
	vfs_file_get_contents (filename, & buffer, & size);

	eng = psf_probe(buffer);
	if (eng == ENG_NONE || eng == ENG_COUNT)
	{
		free(buffer);
		return FALSE;
	}

	f = &psf_functor_map[eng];
	if (f->start(buffer, size) != AO_SUCCESS)
	{
		free(buffer);
		return FALSE;
	}

	data->output->open_audio(FMT_S16_NE, 44100, 2);

	data->set_params(data, 44100*2*2*8, 44100, 2);

	stop_flag = FALSE;
	data->set_pb_ready(data);

	for (;;)
	{
		f->execute(data);

		if (seek)
		{
			data->output->flush(seek);

			f->stop();

			if (f->start(buffer, size) == AO_SUCCESS)
			{
				f->seek(seek);
				seek = 0;
				continue;
			}
			else
				break;
		}

		f->stop();
		break;
	}

	free(buffer);
	free(path);

	return ! error;
}

void psf2_update(unsigned char *buffer, long count, InputPlayback *playback)
{
	if (buffer == NULL)
	{
		stop_flag = TRUE;

		return;
	}

	playback->output->write_audio (buffer, count);

	if (seek)
	{
		if (psf2_seek(seek))
		{
			playback->output->flush(seek);
			seek = 0;
		}
		else
		{
			stop_flag = TRUE;
			return;
		}
	}
}

void psf2_Stop(InputPlayback *playback)
{
	pthread_mutex_lock (& mutex);

	stop_flag = TRUE;
	playback->output->abort_write ();

	pthread_mutex_unlock (& mutex);
}

void psf2_pause(InputPlayback *playback, bool_t pause)
{
	playback->output->pause (pause);
}

int psf2_is_our_fd(const char *filename, VFSFile *file)
{
	uint8_t magic[4];
	if (vfs_fread(magic, 1, 4, file) < 4)
		return FALSE;

	return (psf_probe(magic) != ENG_NONE);
}

static void psf2_Seek(InputPlayback *playback, int time)
{
	seek = time;
}

static const char *psf2_fmts[] = { "psf", "minipsf", "psf2", "minipsf2", "spu", "spx", NULL };

AUD_INPUT_PLUGIN
(
	.name = N_("OpenPSF PSF1/PSF2 Decoder"),
	.domain = PACKAGE,
	.play = psf2_play,
	.stop = psf2_Stop,
	.pause = psf2_pause,
	.mseek = psf2_Seek,
	.probe_for_tuple = psf2_tuple,
	.is_our_file_from_vfs = psf2_is_our_fd,
	.extensions = psf2_fmts,
)
