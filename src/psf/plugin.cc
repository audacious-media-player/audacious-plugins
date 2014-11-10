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
    {nullptr, nullptr, nullptr, nullptr},
    {psf_start, psf_stop, psf_seek, psf_execute},
    {psf2_start, psf2_stop, psf2_seek, psf2_execute},
    {spx_start, spx_stop, psf_seek, spx_execute},
};

static PSFEngineFunctors *f;
static String dirpath;

bool stop_flag = false;

static PSFEngine psf_probe(const char *buf, int len)
{
	if (len < 4)
		return ENG_NONE;

	if (!memcmp(buf, "PSF\x01", 4))
		return ENG_PSF1;

	if (!memcmp(buf, "PSF\x02", 4))
		return ENG_PSF2;

	if (!memcmp(buf, "SPU", 3))
		return ENG_SPX;

	if (!memcmp(buf, "SPX", 3))
		return ENG_SPX;

	return ENG_NONE;
}

/* ao_get_lib: called to load secondary files */
Index<char> ao_get_lib(char *filename)
{
	VFSFile file(filename_build({dirpath, filename}), "r");
	return file ? file.read_all() : Index<char>();
}

Tuple psf2_tuple(const char *filename, VFSFile &file)
{
	Tuple t;
	corlett_t *c;

	Index<char> buf = file.read_all ();

	if (!buf.len())
		return t;

	if (corlett_decode((uint8_t *)buf.begin(), buf.len(), nullptr, nullptr, &c) != AO_SUCCESS)
		return t;

	t.set_filename (filename);

	t.set_int (Tuple::Length, psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade));
	t.set_str (Tuple::Artist, c->inf_artist);
	t.set_str (Tuple::Album, c->inf_game);
	t.set_str (Tuple::Title, c->inf_title);
	t.set_str (Tuple::Copyright, c->inf_copy);
	t.set_str (Tuple::Quality, _("sequenced"));
	t.set_str (Tuple::Codec, "PlayStation 1/2 Audio");

	free(c);

	return t;
}

static bool psf2_play(const char * filename, VFSFile & file)
{
	bool error = false;

	const char * slash = strrchr (filename, '/');
	if (! slash)
		return false;

	dirpath = String (str_copy (filename, slash + 1 - filename));

	Index<char> buf = file.read_all ();

	PSFEngine eng = psf_probe(buf.begin(), buf.len());
	if (eng == ENG_NONE || eng == ENG_COUNT)
	{
		error = true;
		goto cleanup;
	}

	f = &psf_functor_map[eng];
	if (f->start((uint8_t *)buf.begin(), buf.len()) != AO_SUCCESS)
	{
		error = true;
		goto cleanup;
	}

	aud_input_set_bitrate(44100*2*2*8);

	aud_input_open_audio(FMT_S16_NE, 44100, 2);

	stop_flag = false;

	f->execute();
	f->stop();

cleanup:
	f = nullptr;
	dirpath = String ();

	return ! error;
}

void psf2_update(unsigned char *buffer, long count)
{
	if (! buffer || aud_input_check_stop ())
	{
		stop_flag = true;
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

bool psf2_is_our_fd(const char *filename, VFSFile &file)
{
	char magic[4];
	if (file.fread (magic, 1, 4) < 4)
		return false;

	return (psf_probe(magic, 4) != ENG_NONE);
}

static const char *psf2_fmts[] = { "psf", "minipsf", "psf2", "minipsf2", "spu", "spx", nullptr };

#define AUD_PLUGIN_NAME        N_("OpenPSF PSF1/PSF2 Decoder")
#define AUD_INPUT_PLAY         psf2_play
#define AUD_INPUT_READ_TUPLE   psf2_tuple
#define AUD_INPUT_IS_OUR_FILE  psf2_is_our_fd
#define AUD_INPUT_EXTS         psf2_fmts

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
