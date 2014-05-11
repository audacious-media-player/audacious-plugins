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

#include <libaudcore/i18n.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>
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
static String dirpath;

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

	StringBuf path = filename_build ({dirpath, filename});
	vfs_file_get_contents(path, &filebuf, &size);

	*buffer = (uint8_t *) filebuf;
	*length = (uint64_t)size;

	return AO_SUCCESS;
}

Tuple psf2_tuple(const char *filename, VFSFile *file)
{
	Tuple t;
	corlett_t *c;
	void *buf;
	int64_t sz;

	vfs_file_get_contents (filename, & buf, & sz);

	if (!buf)
		return t;

	if (corlett_decode((uint8_t *) buf, sz, NULL, NULL, &c) != AO_SUCCESS)
		return t;

	t.set_filename (filename);

	t.set_int (FIELD_LENGTH, c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1);
	t.set_str (FIELD_ARTIST, c->inf_artist);
	t.set_str (FIELD_ALBUM, c->inf_game);
	t.set_str (FIELD_TITLE, c->inf_title);
	t.set_str (FIELD_COPYRIGHT, c->inf_copy);
	t.set_str (FIELD_QUALITY, _("sequenced"));
	t.set_str (FIELD_CODEC, "PlayStation 1/2 Audio");

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

	const char * slash = strrchr (filename, '/');
	if (! slash)
		return FALSE;

	dirpath = String (str_copy (filename, slash + 1 - filename));

	vfs_file_get_contents (filename, & buffer, & size);

	eng = psf_probe((uint8_t *) buffer);
	if (eng == ENG_NONE || eng == ENG_COUNT)
	{
		free(buffer);
		return FALSE;
	}

	f = &psf_functor_map[eng];
	if (f->start((uint8_t *) buffer, size) != AO_SUCCESS)
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
	dirpath = String ();
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

#define AUD_PLUGIN_NAME        N_("OpenPSF PSF1/PSF2 Decoder")
#define AUD_INPUT_PLAY         psf2_play
#define AUD_INPUT_READ_TUPLE   psf2_tuple
#define AUD_INPUT_IS_OUR_FILE  psf2_is_our_fd
#define AUD_INPUT_EXTS         psf2_fmts

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
