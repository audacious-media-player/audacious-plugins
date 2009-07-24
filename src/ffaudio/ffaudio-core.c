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
#include "config.h"
#include "ffaudio-stdinc.h"
#include <audacious/i18n.h>

/***********************************************************************************
 * Plugin glue.                                                                    *
 ***********************************************************************************/

static GMutex *seek_mutex = NULL;
static GCond *seek_cond = NULL;
static gint64 seek_value = -1;

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
ffaudio_probe(const gchar *filename, VFSFile *file)
{
    AVCodec *codec2 = NULL;
    AVCodecContext *c2 = NULL;
    AVFormatContext *ic2 = NULL;
    gint i, ret;
    gchar uribuf[64];

    _DEBUG("probing for %s, filehandle %p", filename, file);

    g_snprintf(uribuf, sizeof(uribuf), "audvfsptr:%p", file);
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
ffaudio_get_tuple_data(Tuple *tuple, AVFormatContext *ic, AVCodecContext *c, AVCodec *codec)
{
    if (ic != NULL)
    {
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_ARTIST, "author");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_TITLE, "title");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_ALBUM, "album");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_PERFORMER, "performer");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_COPYRIGHT, "copyright");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_GENRE, "genre");
        copy_tuple_meta(tuple, ic, TUPLE_STRING, FIELD_COMMENT, "comment");
        copy_tuple_meta(tuple, ic, TUPLE_INT, FIELD_YEAR, "year");
        copy_tuple_meta(tuple, ic, TUPLE_INT, FIELD_TRACK_NUMBER, "track");
        aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, ic->duration / 1000);
    }

    if (c != NULL)
    {
        aud_tuple_associate_int(tuple, FIELD_BITRATE, NULL, c->bit_rate / 1000);
    }
    
    if (codec != NULL && codec->long_name != NULL)
    {
        aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, codec->long_name);
    }
}

static Tuple *
ffaudio_get_song_tuple(const gchar *filename)
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

    ffaudio_get_tuple_data(tuple, ic, c, codec);
    return tuple;
}
    
static void
ffaudio_play_file(InputPlayback *playback)
{
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVFormatContext *ic = NULL;
    AVStream *s = NULL;
    AVPacket pkt = {};
    guint8 *outbuf = NULL, *resbuf = NULL;
    gint i, stream_id, out_channels, errcount;
    gint in_sample_size, out_sample_size;
    ReSampleContext *resctx = NULL;
    gboolean resample = FALSE;
    AFormat out_fmt;
    gchar *uribuf, *title;
    Tuple *tuple;

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
        goto error_exit;

    _DEBUG("got codec %s for stream index %d, opening", codec->name, stream_id);

    if (avcodec_open(c, codec) < 0)
        goto error_exit;

    /* Determine if audio conversion or resampling is needed */
    out_channels = c->channels;
    in_sample_size = av_get_bits_per_sample_format(c->sample_fmt) / 8;
    out_sample_size = av_get_bits_per_sample_format(SAMPLE_FMT_S16) / 8;

    if (c->channels > 2)
        resample = TRUE;
    
    switch (c->sample_fmt) {
        case SAMPLE_FMT_U8: out_fmt = FMT_U8; break;
        case SAMPLE_FMT_S16: out_fmt = FMT_S16_NE; break;
        case SAMPLE_FMT_S32: out_fmt = FMT_S32_NE; break;
        case SAMPLE_FMT_FLT: out_fmt = FMT_FLOAT; break;
        default: resample = TRUE; break;
    }
    
    if (resample)
    {
        /* Initialize resampling context */
        out_channels = 2;
        out_fmt = FMT_S16_NE;

        _DEBUG("resampling needed chn=%d, rate=%d, fmt=%d -> chn=%d, rate=%d, fmt=S16NE",
            c->channels, c->sample_rate, c->sample_fmt,
            out_channels, c->sample_rate);

        resctx = av_audio_resample_init(
            out_channels, c->channels,
            c->sample_rate, c->sample_rate,
            SAMPLE_FMT_S16, c->sample_fmt,
            16, 10, 0, 0.8);

        if (resctx == NULL)
            goto error_exit;
    }
    
    /* Open audio output */
    _DEBUG("opening audio output");

    if (playback->output->open_audio(out_fmt, c->sample_rate, out_channels) <= 0)
        goto error_exit;

    /* Allocate output buffer aligned to 16 byte boundary */
    outbuf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    resbuf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

    _DEBUG("setting parameters");
    tuple = aud_tuple_new_from_filename(playback->filename);
    ffaudio_get_tuple_data(tuple, ic, c, codec);
    title = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
    tuple_free(tuple);
    playback->set_params(playback, title, ic->duration / 1000, c->bit_rate, c->sample_rate, c->channels);
    g_free(title);
    
    playback->playing = 1;
    playback->set_pb_ready(playback);
    
    errcount = 0;

    _DEBUG("playback ready, entering decode loop");
    while (playback->playing)
    {
        AVPacket tmp;
        gint ret;

        /* Perform seek, if requested */
        g_mutex_lock(seek_mutex);

        if (seek_value >= 0)
        {
            playback->output->flush(seek_value * 1000);
            if (av_seek_frame(ic, -1, seek_value * AV_TIME_BASE, AVSEEK_FLAG_ANY) < 0)
            {
                _DEBUG("error while seeking");
            } else
                errcount = 0;
            
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
                } else
                    continue;
            }
        } else
            errcount = 0;

        /* Ignore any other substreams */
        if (pkt.stream_index != stream_id) {
            av_free_packet(&pkt);
            continue;
        }
        
        /* Decode and play packet/frame */
        memcpy(&tmp, &pkt, sizeof(tmp));
        while (tmp.size > 0 && playback->playing)
        {
            gint len, out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
            guint8 *outbuf_p;

            /* Check for seek request and bail out if we have one */
            g_mutex_lock(seek_mutex);
            if (seek_value != -1) {
                g_mutex_unlock(seek_mutex);
                break;
            }
            g_mutex_unlock(seek_mutex);
            
            /* Decode whatever we can of the frame data */
#if (LIBAVCODEC_VERSION_MAJOR <= 52) && (LIBAVCODEC_VERSION_MINOR <= 25)
            len = avcodec_decode_audio2(c, (gint16 *)outbuf, &out_size, tmp.data, tmp.size);
#else
            len = avcodec_decode_audio3(c, (gint16 *)outbuf, &out_size, &tmp);
#endif
            if (len < 0)
            {
                _DEBUG("codec failure, breaking out of loop");
                break;
            }
            
            tmp.size -= len;
            tmp.data += len;

            if (out_size <= 0)
                continue;
            
            /* Perform audio resampling if necessary */
            if (resample)
            {
                out_size = audio_resample(resctx,
                    (gint16 *)resbuf, (gint16 *)outbuf,
                    out_size / in_sample_size) * out_sample_size;
                outbuf_p = resbuf;
            }
            else
                outbuf_p = outbuf;

            /* Output audio in small blocks */
            while (out_size > 0 && playback->playing)
            {
                gsize writeoff = out_size >= 512 ? 512 : out_size;

                playback->pass_audio(playback, out_fmt,
                    c->channels, out_size >= 512 ? 512 : out_size,
                    (gint16 *)outbuf_p, NULL);

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

error_exit:

    _DEBUG("decode loop finished, shutting down");

    playback->playing = 0;

    av_free(outbuf);
    av_free(resbuf);
    if (resctx != NULL)
        audio_resample_close(resctx);
    if (pkt.data)
        av_free_packet(&pkt);
    if (c != NULL)
        avcodec_close(c);
    if (ic != NULL)
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
ffaudio_pause(InputPlayback *playback, gshort p)
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

    /* WAV (PCM or GSM payload) */
    "wav",

    /* end of table */
    NULL
};

static void
ffaudio_about(void)
{
    static GtkWidget *aboutbox = NULL;
    
    if (aboutbox == NULL)
    {
        gchar *formats = g_strjoinv(", ", ffaudio_fmts);
        gchar *description = g_strdup_printf(
        _("Multi-format audio decoding plugin for Audacious based on \n"
        "FFmpeg multimedia framework (http://www.ffmpeg.org/) \n"
        "Copyright (c) 2000-2009 Fabrice Bellard, et al.\n\n"
        "Supported formats: %s\n\n"
        "Audacious plugin by: \n"
        "            William Pitcock <nenolod@nenolod.net>,\n"
        "            Matti Hämäläinen <ccr@tnsp.org>\n"),
        formats);

        aboutbox = audacious_info_dialog(
            _("About FFaudio Plugin"),
            description, _("Ok"), FALSE, NULL, NULL);

        g_free(formats);
        g_free(description);

        g_signal_connect(G_OBJECT(aboutbox), "destroy",
            G_CALLBACK(gtk_widget_destroyed), &aboutbox);
    }
}

InputPlugin ffaudio_ip = {
    .init = ffaudio_init,
    .cleanup = ffaudio_cleanup,
    .is_our_file_from_vfs = ffaudio_probe,
    .play_file = ffaudio_play_file,
    .stop = ffaudio_stop,
    .pause = ffaudio_pause,
    .seek = ffaudio_seek,
    .get_song_tuple = ffaudio_get_song_tuple,
    .about = ffaudio_about,
    .description = "FFaudio Plugin",
    .vfs_extensions = ffaudio_fmts,
};

static InputPlugin *ffaudio_iplist[] = { &ffaudio_ip, NULL };

SIMPLE_INPUT_PLUGIN(ffaudio, ffaudio_iplist);
