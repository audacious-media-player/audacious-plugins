/*
 * Copyright (c) 2005 David Hammerton
 * All rights reserved.
 *
 * Adapted from example program by William Pitcock <nenolod@atheme.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>

#if HAVE_STDINT_H
# include <stdint.h>
#else
# if HAVE_INTTYPES_H
# include <inttypes.h>
# endif
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <audacious/i18n.h>

#include <audacious/plugin.h>
#include <audacious/output.h>

#include "demux.h"
#include "decomp.h"
#include "stream.h"

static int input_opened = 0;

gpointer decode_thread(void *args);
static GThread *playback_thread;
static int seek_to = -1;

extern void set_endian();

static void alac_about(void)
{
	static GtkWidget *aboutbox;

	if(aboutbox != NULL)
		return;

	aboutbox = audacious_info_dialog(_("About Apple Lossless Audio Plugin"),
				     _("Copyright (c) 2006 Audacious team\n"
				     "Portions (c) 2005-2006 David Hammerton <crazney -at- crazney.net>"),
				     _("Ok"), FALSE, NULL, NULL);

	g_signal_connect(G_OBJECT(aboutbox), "destroy",
			 G_CALLBACK(gtk_widget_destroyed),
			 &aboutbox);
}

gboolean is_our_fd(char *filename, VFSFile* input_file)
{
    demux_res_t demux_res;
    stream_t *input_stream;

    input_stream = stream_create_file(input_file, 1);

    set_endian();

    if (!input_stream)
        return FALSE;

    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
    {
        stream_destroy(input_stream);
        return FALSE;
    }

    stream_destroy(input_stream);

    return TRUE;
}

static int get_duration(demux_res_t *demux_res)
{
    int i;
    long long samples = 0;

    for (i = 0; i < demux_res->num_time_to_samples; i++)
        samples += demux_res->time_to_sample[i].sample_count *
                    demux_res->time_to_sample[i].sample_duration;

    return (samples * 1000) / demux_res->sample_rate;
}

Tuple *build_aud_tuple_from_demux(demux_res_t *demux_res, char *path)
{
    Tuple *ti = aud_tuple_new_from_filename(path);

    if (demux_res->tuple.art != NULL)
        aud_tuple_associate_string(ti, FIELD_ARTIST, NULL, demux_res->tuple.art);
    if (demux_res->tuple.nam != NULL)
        aud_tuple_associate_string(ti, FIELD_TITLE, NULL, demux_res->tuple.nam);
    if (demux_res->tuple.alb != NULL)
        aud_tuple_associate_string(ti, FIELD_ALBUM, NULL, demux_res->tuple.alb);
    if (demux_res->tuple.gen != NULL)
        aud_tuple_associate_string(ti, FIELD_GENRE, NULL, demux_res->tuple.gen);
    if (demux_res->tuple.cmt != NULL)
        aud_tuple_associate_string(ti, FIELD_COMMENT, NULL, demux_res->tuple.cmt);
    if (demux_res->tuple.day != NULL)
        aud_tuple_associate_int(ti, FIELD_YEAR, NULL, atoi(demux_res->tuple.day));

    aud_tuple_associate_string(ti, FIELD_CODEC, NULL, "Apple Lossless (ALAC)");
    aud_tuple_associate_string(ti, FIELD_QUALITY, NULL, "lossless");
    aud_tuple_associate_int(ti, FIELD_LENGTH, NULL, get_duration(demux_res));

    return ti;
}

Tuple *build_tuple(char *filename)
{
    demux_res_t demux_res;
    VFSFile *input_file;
    stream_t *input_stream;

    input_file = aud_vfs_fopen(filename, "rb");
    input_stream = stream_create_file(input_file, 1);

    set_endian();

    if (!input_stream)
    {
	aud_vfs_fclose(input_file);
        return NULL;
    }

    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
    {
        stream_destroy(input_stream);
        aud_vfs_fclose(input_file);
        return NULL;
    }

    stream_destroy(input_stream);
    aud_vfs_fclose(input_file);

    return build_aud_tuple_from_demux(&demux_res, filename);
}

static InputPlayback *playback;

static void play_file(InputPlayback *data)
{
    char *filename = data->filename;
    playback = data;
    playback->playing = TRUE;
    playback_thread = g_thread_self();
    playback->set_pb_ready(playback);
    decode_thread(filename);
}

static void stop(InputPlayback * data)
{
    playback->playing = FALSE;
    g_thread_join(playback_thread);
    data->output->close_audio();
}

static void do_pause(InputPlayback *data, short paused)
{
    data->output->pause(paused);
}

static void seek(InputPlayback * data, gint time)
{
    seek_to = time;
}

void GetBuffer(demux_res_t *demux_res)
{
    unsigned long destBufferSize = 1024*16; /* 16kb buffer = 4096 frames = 1 alac sample */
    void *pDestBuffer = malloc(destBufferSize);
    int bytes_read = 0;

    unsigned int buffer_size = 1024*128;
    void *buffer;

    unsigned int i;

    buffer = malloc(buffer_size);

    for (i = 0; i < demux_res->num_sample_byte_sizes && playback->playing; i++)
    {
        uint32_t sample_byte_size;

        int outputBytes;

#if 0
	/* XXX: Horribly inaccurate seek. -nenolod */
	if (seek_to != -1)
	{
	    gulong duration =
		(demux_res->num_sample_byte_sizes * (float)((1024 * demux_res->sample_size) - 1.0) /
	       		(float)(demux_res->sample_rate / 251));

	    i = (duration - seek_to) / demux_res->num_sample_byte_sizes;

	    g_print("seek to ALAC frame: %d\n", i);

	    seek_to = -1;
	}
#endif

        /* just get one sample for now */
        sample_byte_size = demux_res->sample_byte_size[i];

        if (buffer_size < sample_byte_size)
            return;

        stream_read(demux_res->stream, sample_byte_size, buffer);

        /* now fetch */
        outputBytes = destBufferSize;
        decode_frame(demux_res->alac, buffer, pDestBuffer, &outputBytes);

        /* write */
        bytes_read += outputBytes;

        playback->pass_audio(playback, FMT_S16_LE, demux_res->num_channels, outputBytes, pDestBuffer, &playback->playing);
    }

    free(buffer);
    free(pDestBuffer);
}

static gchar *fmts[] = { "m4a", "alac", NULL };

InputPlugin alac_ip = {
    .description = "Apple Lossless Plugin",
    .about = alac_about,
    .play_file = play_file,
    .stop = stop,
    .pause = do_pause,
    .seek = seek,
    .get_song_tuple = build_tuple,
    .is_our_file_from_vfs = is_our_fd,
    .vfs_extensions = fmts,
};

InputPlugin *alac_iplist[] = { &alac_ip, NULL };

DECLARE_PLUGIN(alac, NULL, NULL, alac_iplist, NULL, NULL, NULL, NULL, NULL);

gpointer decode_thread(void *args)
{
    demux_res_t demux_res;
    VFSFile *input_file;
    stream_t *input_stream;
    Tuple *ti;
    gchar *title;

    memset(&demux_res, 0, sizeof(demux_res));

    set_endian();

    input_file = aud_vfs_fopen((char *) args, "rb");
    input_stream = stream_create_file(input_file, 1);

    if (!input_stream)
        return NULL;

    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
        return NULL;

    demux_res.stream = input_stream;

    /* Get the titlestring ready. */
    ti = build_aud_tuple_from_demux(&demux_res, (char *) args);
    title = aud_tuple_formatter_make_title_string(ti, aud_get_gentitle_format());

    /* initialise the sound converter */
    demux_res.alac = create_alac(demux_res.sample_size, demux_res.num_channels);
    alac_set_info(demux_res.alac, demux_res.codecdata);

    playback->output->open_audio(FMT_S16_LE, demux_res.sample_rate, demux_res.num_channels);
    playback->set_params(playback, title, get_duration(&demux_res), -1, demux_res.sample_rate, demux_res.num_channels);

    /* will convert the entire buffer */
    GetBuffer(&demux_res);

    if (playback->playing) {
        playback->output->buffer_free();
        playback->output->buffer_free();
        while (playback->output->buffer_playing() && playback->playing)
            g_usleep(100000);
    }

    stream_destroy(input_stream);

    if (input_opened)
        aud_vfs_fclose(input_file);

    playback->output->close_audio();

    playback->playing = FALSE;

    return NULL;
}
