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

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/vfs.h>
#include <audacious/util.h>

#include "demux.h"
#include "decomp.h"
#include "stream.h"

static int input_opened = 0;

gpointer decode_thread(void *args);
static GThread *playback_thread;
static int going = 0;
static int seek_to = -1;

extern void set_endian();

static gchar *
extname(const char *filename)
{
    gchar *ext = strrchr(filename, '.');

    if (ext != NULL)
        ++ext;

    return ext;  
}

static void alac_about(void)
{
	static GtkWidget *aboutbox;
    
	if(aboutbox != NULL)
		return;

	aboutbox = xmms_show_message("About Apple Lossless Audio Plugin",
				     "Copyright (c) 2006 Audacious team\n"
				     "Portions (c) 2005-2006 David Hammerton <crazney -at- crazney.net>",
				     _("Ok"), FALSE, NULL, NULL);

	g_signal_connect(G_OBJECT(aboutbox), "destroy",
			 G_CALLBACK(gtk_widget_destroyed),
			 &aboutbox);
}

static void alac_init(void)
{
	/* empty */
}

gboolean is_our_file(char *filename)
{
    demux_res_t demux_res;
    VFSFile *input_file;
    stream_t *input_stream;
 
    input_file = vfs_fopen(filename, "rb");
    input_stream = stream_create_file(input_file, 1);
 
    set_endian();
 
    if (!input_stream)
    {
	vfs_fclose(input_file);
        return FALSE;
    }
         
    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
    {
        stream_destroy(input_stream);
	vfs_fclose(input_file);
        return FALSE;
    }
 
    stream_destroy(input_stream);
    vfs_fclose(input_file);

    return TRUE;
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

TitleInput *build_tuple_from_demux(demux_res_t *demux_res, char *path)
{
    TitleInput *ti = bmp_title_input_new();

    if (demux_res->tuple.art != NULL)
        ti->performer = g_strdup(demux_res->tuple.art);
    if (demux_res->tuple.nam != NULL)
        ti->track_name = g_strdup(demux_res->tuple.nam);
    if (demux_res->tuple.alb != NULL)
        ti->album_name = g_strdup(demux_res->tuple.alb);
    if (demux_res->tuple.gen != NULL)
        ti->genre = g_strdup(demux_res->tuple.gen);
    if (demux_res->tuple.cmt != NULL)
        ti->comment = g_strdup(demux_res->tuple.cmt);
    if (demux_res->tuple.day != NULL)
        ti->year = atoi(demux_res->tuple.day);

    ti->file_name = g_path_get_basename(path);
    ti->file_path = g_path_get_dirname(path);
    ti->file_ext = extname(path);

    return ti;
}

TitleInput *build_tuple(char *filename)
{
    demux_res_t demux_res;
    VFSFile *input_file;
    stream_t *input_stream;
    TitleInput *ti;

    input_file = vfs_fopen(filename, "rb");
    input_stream = stream_create_file(input_file, 1);

    set_endian();

    if (!input_stream)
    {
	vfs_fclose(input_file);
        return NULL;
    }
        
    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
    {
        stream_destroy(input_stream);
	vfs_fclose(input_file);
        return NULL;
    }

    stream_destroy(input_stream);
    vfs_fclose(input_file);

    return build_tuple_from_demux(&demux_res, filename);
}

static void play_file(char *filename)
{
    going = 1;
    playback_thread = g_thread_create(decode_thread, filename, TRUE, NULL);
}

static void stop(void)
{
    going = 0;
    g_thread_join(playback_thread);
    output_close_audio();
}

static void seek(gint time)
{
    seek_to = time;
}

static gint get_time(void)
{
    if (going)
	return get_output_time();
    else
	return -1;
}

InputPlugin alac_ip = {
    NULL,
    NULL,
    "Apple Lossless Plugin",                       /* Description */  
    alac_init,
    alac_about,
    NULL,
    is_our_file,
    NULL,
    play_file,
    stop,
    output_pause,
    seek,
    NULL,
    get_time,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,                       /* file_info_box */
    NULL,
    build_tuple,
    NULL,
    NULL,
    is_our_fd,
};

static int get_sample_info(demux_res_t *demux_res, uint32_t samplenum,
                           uint32_t *sample_duration,
                           uint32_t *sample_byte_size)
{
    unsigned int duration_index_accum = 0;
    unsigned int duration_cur_index = 0;

    if (samplenum >= demux_res->num_sample_byte_sizes)
        return 0;

    if (!demux_res->num_time_to_samples)
        return 0;

    while ((demux_res->time_to_sample[duration_cur_index].sample_count + duration_index_accum)
            <= samplenum)
    {
        duration_index_accum += demux_res->time_to_sample[duration_cur_index].sample_count;
        duration_cur_index++;

        if (duration_cur_index >= demux_res->num_time_to_samples)
            return 0;
    }

    *sample_duration = demux_res->time_to_sample[duration_cur_index].sample_duration;
    *sample_byte_size = demux_res->sample_byte_size[samplenum];

    return 1;
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

    for (i = 0; i < demux_res->num_sample_byte_sizes && going == 1; i++)
    {
        uint32_t sample_duration;
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
        if (!get_sample_info(demux_res, i,
                             &sample_duration, &sample_byte_size))
            return;

        if (buffer_size < sample_byte_size)
            return;

        stream_read(demux_res->stream, sample_byte_size, buffer);

        /* now fetch */
        outputBytes = destBufferSize;
        decode_frame(demux_res->alac, buffer, pDestBuffer, &outputBytes);

        /* write */
        bytes_read += outputBytes;

        produce_audio(alac_ip.output->written_time(), FMT_S16_LE, demux_res->num_channels, outputBytes, pDestBuffer, &going);
    }

    free(buffer);
    free(pDestBuffer);
}

gpointer decode_thread(void *args)
{
    demux_res_t demux_res;
    unsigned int output_size, i;
    gulong duration = 0;	/* samples added up */
    gint framesize;
    VFSFile *input_file;
    stream_t *input_stream;
    TitleInput *ti;
    gchar *title;

    memset(&demux_res, 0, sizeof(demux_res));

    set_endian();

    input_file = vfs_fopen((char *) args, "rb");
    input_stream = stream_create_file(input_file, 1);

    if (!input_stream)
        return NULL;

    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
        return NULL;

    demux_res.stream = input_stream;

    /* Get the titlestring ready. */
    ti = build_tuple_from_demux(&demux_res, (char *) args);
    title = xmms_get_titlestring(xmms_get_gentitle_format(), ti);

    /* initialise the sound converter */
    demux_res.alac = create_alac(demux_res.sample_size, demux_res.num_channels);
    alac_set_info(demux_res.alac, demux_res.codecdata);

    /* Sample rates are multiples of 251?! Apple is *fucking* *insane*! -nenolod */
    duration = (demux_res.num_sample_byte_sizes * (float)((1024 * demux_res.sample_size) - 1.0) /
	(float)(demux_res.sample_rate / 251));

    alac_ip.output->open_audio(FMT_S16_LE, demux_res.sample_rate, demux_res.num_channels);
    alac_ip.set_info(title, duration, -1, demux_res.sample_rate, demux_res.num_channels);

    /* will convert the entire buffer */
    GetBuffer(&demux_res);

    going = 0;

    stream_destroy(input_stream);

    if (input_opened)
        vfs_fclose(input_file);

    alac_ip.output->close_audio();

    return NULL;
}

InputPlugin *get_iplugin_info(void)
{
    return &alac_ip;
}
