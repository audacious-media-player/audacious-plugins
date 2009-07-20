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

static void decode_thread(InputPlayback *playback);
static GThread *playback_thread;
static guint64 seek_to = -1;
static guint64 packet0_offset;

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

gint is_our_fd(const gchar *filename, VFSFile* input_file)
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

Tuple *build_aud_tuple_from_demux(demux_res_t *demux_res, const gchar *path)
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

Tuple *build_tuple(const gchar *filename)
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

static void play_file(InputPlayback *playback)
{
    playback->playing = TRUE;
    playback_thread = g_thread_self();
    playback->set_pb_ready(playback);
    decode_thread(playback);
}

static void stop(InputPlayback *playback)
{
    playback->playing = FALSE;
    g_thread_join(playback_thread);
    playback->output->close_audio();
}

static void do_pause(InputPlayback *playback, short paused)
{
    playback->output->pause(paused);
}

static void mseek(InputPlayback* playback, gulong millisecond)
{
    if (!playback->playing)
        return;

    seek_to = millisecond;

    while (seek_to != -1)
        g_usleep(10000);
}

static void seek(InputPlayback *playback, gint time)
{
    mseek(playback, time * 1000);
}

static guint64 get_packet_offset(demux_res_t *demux_res, guint packet)
{
    guint i;
    guint64 offset = 0;

    for (i = 0; i < packet; i++)
        offset += demux_res->sample_byte_size[i];

    return offset;
}

static guint handle_seek(InputPlayback *playback,
    demux_res_t *demux_res, guint current_packet)
{
    guint i, packet;
    guint64 offset, begin, end;
    guint64 target = (seek_to * demux_res->sample_rate) / 1000;

    begin = 0;
    packet = 0;
    for (i = 0; i < demux_res->num_time_to_samples; i++)
    {
        end = begin + demux_res->time_to_sample[i].sample_count *
                    demux_res->time_to_sample[i].sample_duration;
        if (target >= begin && target < end)
        {
            offset = (target - begin) /
                     demux_res->time_to_sample[i].sample_duration;
            /* Calculate packet to seek to */
            packet += offset;
            /* Calculate resulting seek time */
            seek_to = (begin + offset *
                       demux_res->time_to_sample[i].sample_duration) * 1000 /
                      demux_res->sample_rate;
            /* Do the actual seek and flush */
            offset = get_packet_offset(demux_res, packet) + packet0_offset;
            stream_setpos(demux_res->stream, offset);
            playback->output->flush(seek_to);
            return packet;
        }
        begin = end;
        packet += demux_res->time_to_sample[i].sample_count;
    }

    /* If we get here we somehow failed to find the target sample */
    return current_packet;
}

static guint get_max_packet_size(demux_res_t *demux_res)
{
    guint i;
    guint max = 0;

    for (i = 0; i < demux_res->num_sample_byte_sizes; i++)
        if (demux_res->sample_byte_size[i] > max)
            max = demux_res->sample_byte_size[i];

    return max;
}

static guint get_max_packet_duration(demux_res_t *demux_res)
{
    guint i;
    guint max = 0;

    for (i = 0; i < demux_res->num_time_to_samples; i++)
        if (demux_res->time_to_sample[i].sample_duration > max)
            max = demux_res->time_to_sample[i].sample_duration;

    return max;
}

void GetBuffer(InputPlayback *playback, demux_res_t *demux_res)
{
    void *pDestBuffer = malloc(get_max_packet_duration(demux_res) * 4);
    void *buffer = malloc(get_max_packet_size(demux_res));
    guint i = 0;

    while (playback->playing)
    {
        int outputBytes;

        if (seek_to != -1)
        {
            i = handle_seek(playback, demux_res, i);
            seek_to = -1;
        }

        if (i < demux_res->num_sample_byte_sizes)
        {
            /* just get one sample for now */
            stream_read(demux_res->stream, demux_res->sample_byte_size[i], buffer);

            /* now fetch */
            decode_frame(demux_res->alac, buffer, pDestBuffer, &outputBytes);

            /* write */
            playback->pass_audio(playback, FMT_S16_LE, demux_res->num_channels, outputBytes, pDestBuffer, &playback->playing);

            i++;

            /* When we are at the end stop pre-buffering */
            if (i == demux_res->num_sample_byte_sizes)
            {
                playback->output->buffer_free();
                playback->output->buffer_free();
            }
        }
        else
        {
            /* Wait for output buffer to drain */
            if (!playback->output->buffer_playing())
                playback->playing = FALSE;

            if (playback->playing)
                g_usleep(40000);
        }
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
    .mseek = mseek,
    .get_song_tuple = build_tuple,
    .is_our_file_from_vfs = is_our_fd,
    .vfs_extensions = fmts,
};

InputPlugin *alac_iplist[] = { &alac_ip, NULL };

DECLARE_PLUGIN(alac, NULL, NULL, alac_iplist, NULL, NULL, NULL, NULL, NULL);

static void decode_thread(InputPlayback *playback)
{
    demux_res_t demux_res;
    VFSFile *input_file;
    stream_t *input_stream;
    Tuple *ti;
    gchar *title;

    memset(&demux_res, 0, sizeof(demux_res));

    set_endian();

    input_file = aud_vfs_fopen(playback->filename, "rb");
    if (!input_file)
        goto exit;

    input_stream = stream_create_file(input_file, 1);

    /* if qtmovie_read returns successfully, the stream is up to
     * the movie data, which can be used directly by the decoder */
    if (!qtmovie_read(input_stream, &demux_res))
        goto exit_free_stream;

    demux_res.stream = input_stream;
    packet0_offset = stream_tell(input_stream);

    /* Get the titlestring ready. */
    ti = build_aud_tuple_from_demux(&demux_res, playback->filename);
    title = aud_tuple_formatter_make_title_string(ti, aud_get_gentitle_format());

    /* initialise the sound converter */
    demux_res.alac = create_alac(demux_res.sample_size, demux_res.num_channels);
    alac_set_info(demux_res.alac, demux_res.codecdata);

    if (!playback->output->open_audio(FMT_S16_LE, demux_res.sample_rate, demux_res.num_channels))
        goto exit_free_alac;

    playback->set_params(playback, title, get_duration(&demux_res), -1, demux_res.sample_rate, demux_res.num_channels);

    /* will convert the entire buffer */
    GetBuffer(playback, &demux_res);

    playback->output->close_audio();
exit_free_alac:
    free(demux_res.alac);
exit_free_stream:
    stream_destroy(input_stream);
    aud_vfs_fclose(input_file);
exit:
    playback->playing = FALSE;
    return;
}
