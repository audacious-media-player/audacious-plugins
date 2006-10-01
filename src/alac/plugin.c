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

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/vfs.h>

#include "demux.h"
#include "decomp.h"
#include "stream.h"

alac_file *alac = NULL;

static VFSFile *input_file = NULL;
static int input_opened = 0;
static stream_t *input_stream;

static int write_wav_format = 0;
static int verbose = 0;

gpointer decode_thread(void *args);
static GThread *playback_thread;

static void alac_init(void)
{
	/* empty */
}

gboolean is_our_file(char *filename)
{
    demux_res_t demux_res;
    input_file = vfs_fopen(filename, "rb");
#ifdef WORDS_BIGENDIAN
    input_stream = stream_create_file(input_file, 1);
#else
    input_stream = stream_create_file(input_file, 0);
#endif
    if (!input_stream)
    {
        fprintf(stderr, "failed to create input stream from file\n");
	vfs_fclose(input_file);
        return FALSE;
    }
        
    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
    {
        fprintf(stderr, "failed to load the QuickTime movie headers\n");
        stream_destroy(input_stream);
	vfs_fclose(input_file);
        return FALSE;
    }

    stream_destroy(input_stream);
    vfs_fclose(input_file);

    return TRUE;
}

static void play_file(char *filename)
{
    playback_thread = g_thread_create(decode_thread, filename, TRUE, NULL);
}

static void stop(void)
{
    g_thread_join(playback_thread);
    output_close_audio();
}

static void seek(gint time)
{
	/* unimplemented */
}

InputPlugin alac_ip = {
    NULL,
    NULL,
    "Apple Lossless Plugin",                       /* Description */  
    alac_init,
    NULL,
    NULL,
    is_our_file,
    NULL,
    play_file,
    stop,
    output_pause,
    seek,
    NULL,
    get_output_time,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,                       /* file_info_box */
    NULL
};

static int get_sample_info(demux_res_t *demux_res, uint32_t samplenum,
                           uint32_t *sample_duration,
                           uint32_t *sample_byte_size)
{
    unsigned int duration_index_accum = 0;
    unsigned int duration_cur_index = 0;

    if (samplenum >= demux_res->num_sample_byte_sizes)
    {
        fprintf(stderr, "sample %i does not exist\n", samplenum);
        return 0;
    }

    if (!demux_res->num_time_to_samples)
    {
        fprintf(stderr, "no time to samples\n");
        return 0;
    }
    while ((demux_res->time_to_sample[duration_cur_index].sample_count + duration_index_accum)
            <= samplenum)
    {
        duration_index_accum += demux_res->time_to_sample[duration_cur_index].sample_count;
        duration_cur_index++;
        if (duration_cur_index >= demux_res->num_time_to_samples)
        {
            fprintf(stderr, "sample %i does not have a duration\n", samplenum);
            return 0;
        }
    }

    *sample_duration = demux_res->time_to_sample[duration_cur_index].sample_duration;
    *sample_byte_size = demux_res->sample_byte_size[samplenum];

    return 1;
}

static void GetBuffer(demux_res_t *demux_res)
{
    unsigned long destBufferSize = 1024*16; /* 16kb buffer = 4096 frames = 1 alac sample */
    void *pDestBuffer = malloc(destBufferSize);
    int bytes_read = 0;

    unsigned int buffer_size = 1024*64;
    void *buffer;

    unsigned int i;

    buffer = malloc(buffer_size);

    for (i = 0; i < demux_res->num_sample_byte_sizes; i++)
    {
        uint32_t sample_duration;
        uint32_t sample_byte_size;

        int outputBytes;

        /* just get one sample for now */
        if (!get_sample_info(demux_res, i,
                             &sample_duration, &sample_byte_size))
        {
            fprintf(stderr, "sample failed\n");
            return;
        }

        if (buffer_size < sample_byte_size)
        {
            fprintf(stderr, "sorry buffer too small! (is %i want %i)\n",
                    buffer_size,
                    sample_byte_size);
            return;
        }

        stream_read(input_stream, sample_byte_size,
                    buffer);

        /* now fetch */
        outputBytes = destBufferSize;
        decode_frame(alac, buffer, pDestBuffer, &outputBytes);

        /* write */
        bytes_read += outputBytes;

        if (verbose)
            fprintf(stderr, "read %i bytes. total: %i\n", outputBytes, bytes_read);

        produce_audio(get_written_time(), FMT_S16_LE, demux_res->num_channels, outputBytes, pDestBuffer, NULL);
    }
    if (verbose)
        fprintf(stderr, "done reading, read %i frames\n", i);
}

static void init_sound_converter(demux_res_t *demux_res)
{
    alac = create_alac(demux_res->sample_size, demux_res->num_channels);

    alac_set_info(alac, demux_res->codecdata);
}

gpointer decode_thread(void *args)
{
    demux_res_t demux_res;
    unsigned int output_size, i;

    input_file = vfs_fopen((char *) args, "rb");
#ifdef WORDS_BIGENDIAN
    input_stream = stream_create_file(input_file, 1);
#else
    input_stream = stream_create_file(input_file, 0);
#endif
    if (!input_stream)
    {
        fprintf(stderr, "failed to create input stream from file\n");
        return 0;
    }

    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
    {
        fprintf(stderr, "failed to load the QuickTime movie headers\n");
        return 0;
    }

    /* initialise the sound converter */
    init_sound_converter(&demux_res);

    /* will convert the entire buffer */
    GetBuffer(&demux_res);

    stream_destroy(input_stream);

    if (input_opened)
        vfs_fclose(input_file);

    return NULL;
}

InputPlugin *get_iplugin_info(void)
{
    return &alac_ip;
}
