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
#include "eng_protos.h"

/* file types */
static uint32 type;

static struct 
{ 
	uint32 sig; 
	char *name; 
	int32 (*start)(uint8 *, uint32); 
	int32 (*stop)(void); 
	int32 (*command)(int32, int32); 
	uint32 rate; 
} types[] = {
	{ 0x50534602, "Sony PlayStation 2 (.psf2)", psf2_start, psf2_stop, psf2_command, 60 },
	{ 0xffffffff, "", NULL, NULL, NULL, 0 }
};

static char *path;

/* ao_get_lib: called to load secondary files */
int ao_get_lib(char *filename, uint8 **buffer, uint64 *length)
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

Tuple *psf2_tuple(gchar *filename)
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

	aud_tuple_associate_int(t, FIELD_LENGTH, NULL, psfTimeToMS(c->inf_length));
	aud_tuple_associate_string(t, FIELD_ARTIST, NULL, c->inf_artist);
	aud_tuple_associate_string(t, FIELD_ALBUM, NULL, c->inf_game);
	aud_tuple_associate_string(t, -1, "game", c->inf_game);
	aud_tuple_associate_string(t, FIELD_TITLE, NULL, c->inf_title);
	aud_tuple_associate_string(t, FIELD_COPYRIGHT, NULL, c->inf_copy);
	aud_tuple_associate_string(t, FIELD_QUALITY, NULL, "sequenced");
	aud_tuple_associate_string(t, FIELD_CODEC, NULL, "PlayStation2 Audio");
	aud_tuple_associate_string(t, -1, "console", "PlayStation 2");

	free(c);
	g_free(buf);

	return t;
}

gchar *psf2_title(gchar *filename, gint *length)
{
	gchar *title = NULL;
	Tuple *tuple = psf2_tuple(filename);

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

void psf2_play(InputPlayback *data)
{
	guchar *buffer;
	gsize size;
	uint32 filesig;
	gint length;
	gchar *title = psf2_title(data->filename, &length);

	path = g_strdup(data->filename);
	aud_vfs_file_get_contents(data->filename, (gchar **) &buffer, &size);

	// now try to identify the file
	type = 0;
	filesig = buffer[0]<<24 | buffer[1]<<16 | buffer[2]<<8 | buffer[3];
	while (types[type].sig != 0xffffffff)
	{
		if (filesig == types[type].sig)
		{
			break;
		}
		else
		{
			type++;
		}
	}

	// now did we identify it above or just fall through?
	if (types[type].sig == 0xffffffff)
	{
		printf("ERROR: File is unknown, signature bytes are %02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
		free(buffer);
		return;
	}

	if (psf2_start(buffer, size) != AO_SUCCESS)
	{
		free(buffer);
		printf("ERROR: Engine rejected file!\n");
		return;
	}
	
	data->output->open_audio(FMT_S16_NE, 44100, 2);

        data->set_params(data, title, length, 44100*2*2*8, 44100, 2);

	data->playing = TRUE;
	data->set_pb_ready(data);
	while (data->playing)
	{
		psf2_execute(data);
	}	

	g_free(buffer);
	g_free(path);
        g_free(title);
}

void psf2_update(unsigned char *buffer, long count, InputPlayback *playback)
{
	const int mask = ~((((16 / 8) * 2)) - 1);

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
        if(sexypsf_seek(seek))
        {
            playback->output->flush(seek);
            seek = 0;
        }
        else  // negative time - must make a C time machine
        {
            sexypsf_stop();
            return;
        }
    }
    if (stop)
        sexypsf_stop();
#endif
}

void psf2_Stop(InputPlayback *playback)
{
	playback->playing = FALSE;
}

void psf2_pause(InputPlayback *playback, short p)
{
	playback->output->pause(p);
}

int psf2_is_our_fd(gchar *filename, VFSFile *file)
{
	gchar magic[4];
	aud_vfs_fread(magic, 1, 4, file);

	if (!memcmp(magic, "PSF\x02", 4))
		return 1;

	return 0;
}

gchar *psf2_fmts[] = { "psf2", "minipsf2", NULL };

InputPlugin psf2_ip =
{
    .description = "PSF2 Audio Plugin",
    .play_file = psf2_play,
    .stop = psf2_Stop,
    .pause = psf2_pause,
#if 0
    .seek = sexypsf_xmms_seek,
    .get_song_info = sexypsf_xmms_getsonginfo,
#endif
    .get_song_tuple = psf2_tuple,
    .is_our_file_from_vfs = psf2_is_our_fd,
    .vfs_extensions = psf2_fmts,
};

InputPlugin *psf2_iplist[] = { &psf2_ip, NULL };

DECLARE_PLUGIN(psf2, NULL, NULL, psf2_iplist, NULL, NULL, NULL, NULL, NULL);

