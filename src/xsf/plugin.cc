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
#include "vio2sf.h"

/* xsf_get_lib: called to load secondary files */
static String dirpath;

int xsf_get_lib(char *filename, void **buffer, unsigned int *length)
{
	void *filebuf;
	int64_t size;

	StringBuf path = filename_build ({dirpath, filename});
	vfs_file_get_contents(path, &filebuf, &size);

	*buffer = filebuf;
	*length = (uint64_t)size;

	return AO_SUCCESS;
}

Tuple xsf_tuple(const char *filename, VFSFile *fd)
{
	Tuple t;
	corlett_t *c;
	void *buf;
	int64_t sz;

	vfs_file_get_contents (filename, & buf, & sz);

	if (!buf)
		return t;

	if (corlett_decode((uint8_t *) buf, sz, nullptr, nullptr, &c) != AO_SUCCESS)
		return t;

	t.set_filename (filename);

	t.set_int (FIELD_LENGTH, c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1);
	t.set_str (FIELD_ARTIST, c->inf_artist);
	t.set_str (FIELD_ALBUM, c->inf_game);
	t.set_str (FIELD_TITLE, c->inf_title);
	t.set_str (FIELD_COPYRIGHT, c->inf_copy);
	t.set_str (FIELD_QUALITY, _("sequenced"));
	t.set_str (FIELD_CODEC, "GBA/Nintendo DS Audio");

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

	if (corlett_decode((uint8_t *) buf, size, nullptr, nullptr, &c) != AO_SUCCESS)
	{
		free(buf);
		return -1;
	}

	int length = c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1;

	free(c);
	free(buf);

	return length;
}

static bool xsf_play(const char * filename, VFSFile * file)
{
	void *buffer;
	int64_t size;
	int length = xsf_get_length(filename);
	int16_t samples[44100*2];
	int seglen = 44100 / 60;
	float pos;
	bool error = false;

	const char * slash = strrchr (filename, '/');
	if (! slash)
		return false;

	dirpath = String (str_copy (filename, slash + 1 - filename));

	vfs_file_get_contents (filename, & buffer, & size);

	if (xsf_start(buffer, size) != AO_SUCCESS)
	{
		error = true;
		goto ERR_NO_CLOSE;
	}

	if (!aud_input_open_audio(FMT_S16_NE, 44100, 2))
	{
		error = true;
		goto ERR_NO_CLOSE;
	}

	aud_input_set_bitrate(44100*2*2*8);

	while (! aud_input_check_stop ())
	{
		int seek_value = aud_input_check_seek ();

		if (seek_value >= 0)
		{
			if (seek_value > aud_input_written_time ())
			{
				pos = aud_input_written_time ();

				while (pos < seek_value)
				{
					xsf_gen(samples, seglen);
					pos += 16.666;
				}
			}
			else if (seek_value < aud_input_written_time ())
			{
				xsf_term();

				if (xsf_start(buffer, size) == AO_SUCCESS)
				{
					pos = 0;
					while (pos < seek_value)
					{
						xsf_gen(samples, seglen);
						pos += 16.666; /* each segment is 16.666ms */
					}
				}
			   	else
				{
					error = true;
					goto CLEANUP;
				}
			}
		}

		xsf_gen(samples, seglen);
		aud_input_write_audio((uint8_t *)samples, seglen * 4);

		if (aud_input_written_time() >= length)
			goto CLEANUP;
	}

CLEANUP:
	xsf_term();

ERR_NO_CLOSE:
	dirpath = String ();
	free(buffer);

	return !error;
}

bool xsf_is_our_fd(const char *filename, VFSFile *file)
{
	char magic[4];
	if (vfs_fread(magic, 1, 4, file) < 4)
		return false;

	if (!memcmp(magic, "PSF$", 4))
		return 1;

	return 0;
}

static const char *xsf_fmts[] = { "2sf", "mini2sf", nullptr };

#define AUD_PLUGIN_NAME        N_("2SF Decoder")
#define AUD_INPUT_PLAY         xsf_play
#define AUD_INPUT_READ_TUPLE   xsf_tuple
#define AUD_INPUT_IS_OUR_FILE  xsf_is_our_fd
#define AUD_INPUT_EXTS         xsf_fmts

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
