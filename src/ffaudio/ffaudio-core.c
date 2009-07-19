/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#define FFAUDIO_DEBUG
#include "ffaudio-stdinc.h"

#define FFAUDIO_METADATA 1

/***********************************************************************************
 * Plugin glue.                                                                    *
 ***********************************************************************************/

GMutex *seek_mutex = NULL;
GCond *seek_cond = NULL;
gint64 seek_value = -1;

static void
ffaudio_init(void)
{
    avcodec_init();
    av_register_all();

    _DEBUG("registering audvfs protocol");
    av_register_protocol(&audvfs_protocol);

    _DEBUG("registering audvfsptr protocol");
    av_register_protocol(&audvfsptr_protocol);

    _DEBUG("creating seek mutex/cond");
    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();

    _DEBUG("initialization completed");
}

static void
ffaudio_cleanup(void)
{
    _DEBUG("cleaning up");
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond); 
}

static gint
ffaudio_probe(gchar *filename, VFSFile *file)
{
    AVCodec *codec2 = NULL;
    AVCodecContext *c2 = NULL;
    AVFormatContext *ic2 = NULL;
    gint i, ret;
    gchar uribuf[100];

    _DEBUG("probing for %s, filehandle %p", filename, file);

    g_snprintf(uribuf, 100, "audvfsptr:%p", file);
    if ((ret = av_open_input_file(&ic2, uribuf, NULL, 0, NULL)) < 0) {
        _DEBUG("ic2 is NULL, ret %d/%s", ret, strerror(-ret));
        return 0;
    }

    _DEBUG("file opened, %p", ic2);

    for (i = 0; i < ic2->nb_streams; i++)
    {
        c2 = ic2->streams[i]->codec;
        if(c2->codec_type == CODEC_TYPE_AUDIO)
        {
            av_find_stream_info(ic2);
            codec2 = avcodec_find_decoder(c2->codec_id);
            if (codec2)
                break;
        }
    }

    if (!codec2)
    {
        av_close_input_file(ic2);
        return 0;
    }

    _DEBUG("got codec %s, doublechecking", codec2->name);

    av_find_stream_info(ic2);

    codec2 = avcodec_find_decoder(c2->codec_id);

    if (!codec2)
    {
        av_close_input_file(ic2);
        return 0;
    }

    _DEBUG("probe success for %s", codec2->name);
    av_close_input_file(ic2);

    return 1;
}

#ifdef FFAUDIO_METADATA
static void
copy_tuple_meta(Tuple *tuple, AVFormatContext *ic, const TupleValueType ttype, const gint field, const gchar *key)
{
    AVMetadataTag *tag = NULL;

    if (ic->metadata != NULL)
        tag = av_metadata_get(ic->metadata, key, NULL, AV_METADATA_IGNORE_SUFFIX);

    if (tag != NULL)
    {
        switch (ttype) {
        case TUPLE_STRING:
            aud_tuple_associate_string(tuple, field, NULL, tag->value);
            break;

        case TUPLE_INT:
            aud_tuple_associate_int(tuple, field, NULL, atoi(tag->value));
            break;
        
        default:
            break;
        }
    }
}

static void
ffaudio_get_tuple_data(Tuple *tuple, AVFormatContext *ic, AVCodecContext *c)
{
    if (ic != NULL)
    {
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_ARTIST, "author");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_TITLE, "title");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_ALBUM, "album");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_PERFORMER, "performer");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_COPYRIGHT, "copyright");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_GENRE, "genre");
        copy_tuple_meta(tuple, ic, TUPLE_INT, FIELD_YEAR, "year");
        copy_tuple_meta(tuple, ic, TUPLE_INT, FIELD_TRACK_NUMBER, "track");
        aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, ic->duration / 1000);
    }

    if (c != NULL)
    {
        aud_tuple_associate_int(tuple, FIELD_BITRATE, NULL, c->bit_rate / 1000);
    }
}


static Tuple *
ffaudio_get_song_tuple(gchar *filename)
{
    Tuple *tuple = aud_tuple_new_from_filename(filename);
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVFormatContext *ic = NULL;
    AVStream *s = NULL;
    gchar *uribuf;
    gint i;
    
    if (tuple == NULL) return NULL;
    
    uribuf = g_alloca(strlen(filename) + 8);
    sprintf(uribuf, "audvfs:%s", filename);
    if (av_open_input_file(&ic, uribuf, NULL, 0, NULL) < 0)
    {
        tuple_free(tuple);
        return NULL;
    }
    
    for (i = 0; i < ic->nb_streams; i++)
    {
        s = ic->streams[i];
        c = s->codec;
        if (c->codec_type == CODEC_TYPE_AUDIO)
        {
            av_find_stream_info(ic);
            codec = avcodec_find_decoder(c->codec_id);
            if (codec != NULL)
                break;
        }
    }

    ffaudio_get_tuple_data(tuple, ic, c);
    return tuple;
}
    
#endif

static void
ffaudio_play_file(InputPlayback *playback)
{
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVFormatContext *ic = NULL;
    AVStream *s = NULL;
    gint out_size, len, errcount;
    AVPacket pkt = {};
    guint8 outbuf[AVCODEC_MAX_AUDIO_FRAME_SIZE];
    gint i, stream_id;
    gchar *uribuf, *title;
#ifdef FFAUDIO_METADATA
    Tuple *tuple;
#endif

    uribuf = g_alloca(strlen(playback->filename) + 8);
    sprintf(uribuf, "audvfs:%s", playback->filename);

    _DEBUG("opening %s", uribuf);

    if (av_open_input_file(&ic, uribuf, NULL, 0, NULL) < 0)
       return;

    for (i = 0; i < ic->nb_streams; i++)
    {
        s = ic->streams[i];
        c = s->codec;
        if (c->codec_type == CODEC_TYPE_AUDIO)
        {
            av_find_stream_info(ic);
            codec = avcodec_find_decoder(c->codec_id);
            stream_id = i;
            if (codec != NULL)
                break;
        }
    }

    if (codec == NULL)
        return;

    _DEBUG("got codec %s, opening", codec->name);

    if (avcodec_open(c, codec) < 0)
        return;

    _DEBUG("opening audio output");

    if (playback->output->open_audio(FMT_S16_NE, c->sample_rate, c->channels) <= 0)
        return;

    _DEBUG("setting parameters");


#ifdef FFAUDIO_METADATA
    tuple = aud_tuple_new_from_filename(playback->filename);
    ffaudio_get_tuple_data(tuple, ic, c);
    title = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
    tuple_free(tuple);
#else
    title = playback->filename;
#endif
    
    playback->set_params(playback, title, ic->duration / 1000, c->bit_rate, c->sample_rate, c->channels);

#ifdef FFAUDIO_METADATA
    g_free(title);
#endif
    
    playback->playing = 1;
    playback->set_pb_ready(playback);

    _DEBUG("playback ready, entering decode loop");

    errcount = 0;
    while (playback->playing)
    {
        gint size;
        guint8 *data_p;
        gint ret;

        /* Perform seek, if requested */
        g_mutex_lock(seek_mutex);

        if (seek_value != -1)
        {
            playback->output->flush(seek_value * 1000);
            av_seek_frame(ic, -1, seek_value * AV_TIME_BASE, AVSEEK_FLAG_ANY);
            seek_value = -1;
            g_cond_signal(seek_cond);
        }

        g_mutex_unlock(seek_mutex);

        /* Read next frame (or more) of data */
        if ((ret = av_read_frame(ic, &pkt)) < 0)
        {
            if (ret == AVERROR_EOF)
            {
                _DEBUG("eof reached");
                break;
            }
            else
            {
                if (++errcount > 4) {
                    _DEBUG("av_read_frame error %d, giving up.", ret);
                    break;
                }
            }
        } else
            errcount = 0;

        size = pkt.size;
        data_p = pkt.data;
        
        while (size > 0 && playback->playing)
        {
            guint8 *outbuf_p = outbuf;
            out_size = sizeof(outbuf);
            memset(outbuf, 0, sizeof(outbuf));

            /* Check for seek request and bail out if we have one */
            g_mutex_lock(seek_mutex);
            if (seek_value != -1) {
                g_mutex_unlock(seek_mutex);
                break;
            }
            g_mutex_unlock(seek_mutex);
            
            /* Decode whatever we can of the frame data */
            len = avcodec_decode_audio2(c, (gint16 *)outbuf, &out_size, data_p, size);
            if (len < 0)
            {
                _DEBUG("codec failure, breaking out of loop");
                break;
            }
            
            size -= len;
            data_p += len;

            if (out_size <= 0)
            {
                _DEBUG("no output PCM, continuing");
                continue;
            }

            while (out_size > 0 && playback->playing)
            {
                gsize writeoff = out_size >= 512 ? 512 : out_size;

                playback->pass_audio(playback, FMT_S16_NE,
                          c->channels, out_size >= 512 ? 512 : out_size, (gint16 *)outbuf_p, &playback->playing);

                outbuf_p += writeoff;
                out_size -= writeoff;

                /* Check for seek request and bail out if we have one */
                g_mutex_lock(seek_mutex);
                if (seek_value != -1) {
                    g_mutex_unlock(seek_mutex);
                    break;
                }
                g_mutex_unlock(seek_mutex);
            }
        }
        
        if (pkt.data)
            av_free_packet(&pkt);
    }

    _DEBUG("decode loop finished, shutting down");

    playback->playing = 0;

    if (pkt.data)
        av_free_packet(&pkt);
    if (c)
        avcodec_close(c);
    if (ic)
        av_close_input_file(ic);

    playback->output->close_audio();
    _DEBUG("exiting thread");
}

static void
ffaudio_stop(InputPlayback *playback)
{
    playback->playing = 0;
}

static void
ffaudio_pause(InputPlayback *playback, short p)
{
    playback->output->pause(p);
}

static void
ffaudio_seek(InputPlayback *playback, gint seek)
{
    g_mutex_lock(seek_mutex);
    seek_value = seek;
    g_cond_wait(seek_cond, seek_mutex);
    g_mutex_unlock(seek_mutex);
}

static gchar *ffaudio_fmts[] = { 
    /* musepack, SV7/SV8 */
    "mpc", "mp+", "mpp",

    /* windows media audio */
    "wma",

    /* shorten */
    "shn",

    /* atrac3 */
    "aa3", "oma",

    /* MPEG 2/4 AC3 */
    "ac3",

    /* end of table */
    NULL
};

InputPlugin ffaudio_ip = {
    .init = ffaudio_init,
    .cleanup = ffaudio_cleanup,
    .is_our_file_from_vfs = ffaudio_probe,
    .play_file = ffaudio_play_file,
    .stop = ffaudio_stop,
    .pause = ffaudio_pause,
    .seek = ffaudio_seek,
#ifdef FFAUDIO_METADATA
    .get_song_tuple = ffaudio_get_song_tuple,
#endif
    .description = "FFaudio Plugin",
    .vfs_extensions = ffaudio_fmts,
};

static InputPlugin *ffaudio_iplist[] = { &ffaudio_ip, NULL };

SIMPLE_INPUT_PLUGIN(ffaudio, ffaudio_iplist);
