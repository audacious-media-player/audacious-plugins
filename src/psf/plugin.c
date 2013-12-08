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

#include <audacious/i18n.h>
#include <audacious/input.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

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
    int32_t (*execute)(void);
} PSFEngineFunctors;

static PSFEngineFunctors psf_functor_map[ENG_COUNT] = {
    {NULL, NULL, NULL, NULL},
    {psf_start, psf_stop, psf_seek, psf_execute},
    {psf2_start, psf2_stop, psf2_seek, psf2_execute},
    {spx_start, spx_stop, psf_seek, spx_execute},
};

static PSFEngineFunctors *f;
static const char *dirpath;

bool_t stop_flag = FALSE;

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
int ao_get_lib(char *filename, uint8_t **buffer, uint64_t *length)
{
	void *filebuf;
	int64_t size;

	SPRINTF(path2, "%s/%s", dirpath, filename);
	vfs_file_get_contents(path2, &filebuf, &size);

	*buffer = filebuf;
	*length = (uint64_t)size;

	return AO_SUCCESS;
}

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

	tuple_set_int(t, FIELD_LENGTH, c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1);
	tuple_set_str(t, FIELD_ARTIST, c->inf_artist);
	tuple_set_str(t, FIELD_ALBUM, c->inf_game);
	tuple_set_str(t, FIELD_TITLE, c->inf_title);
	tuple_set_str(t, FIELD_COPYRIGHT, c->inf_copy);
	tuple_set_str(t, FIELD_QUALITY, _("sequenced"));
	tuple_set_str(t, FIELD_CODEC, "PlayStation 1/2 Audio");

	free(c);
	free(buf);

	return t;
}

static bool_t psf2_play(const char * filename, VFSFile * file)
{
	void *buffer;
	int64_t size;
	PSFEngine eng;
	bool_t error = FALSE;

	char dirbuf[strlen (filename) + 1];
	strcpy (dirbuf, filename);
	dirpath = dirname (dirbuf);

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

	aud_input_open_audio(FMT_S16_NE, 44100, 2);

	aud_input_set_bitrate(44100*2*2*8);

	stop_flag = FALSE;

	f->execute();
	f->stop();

	f = NULL;
	dirpath = NULL;
	free(buffer);

	return ! error;
}

void psf2_update(unsigned char *buffer, long count)
{
	if (! buffer || aud_input_check_stop ())
	{
		stop_flag = TRUE;
		return;
	}

	int seek = aud_input_check_seek ();

	if (seek >= 0)
	{
		f->seek (seek);
		return;
	}

	aud_input_write_audio (buffer, count);
}

int psf2_is_our_fd(const char *filename, VFSFile *file)
{
	uint8_t magic[4];
	if (vfs_fread(magic, 1, 4, file) < 4)
		return FALSE;

	return (psf_probe(magic) != ENG_NONE);
}

static const char *psf2_fmts[] = { "psf", "minipsf", "psf2", "minipsf2", "spu", "spx", NULL };

AUD_INPUT_PLUGIN
(
	.name = N_("OpenPSF PSF1/PSF2 Decoder"),
	.domain = PACKAGE,
	.play = psf2_play,
	.probe_for_tuple = psf2_tuple,
	.is_our_file_from_vfs = psf2_is_our_fd,
	.extensions = psf2_fmts,
)
