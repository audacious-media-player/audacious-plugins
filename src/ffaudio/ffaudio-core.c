/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
 *                  Matti Hämäläinen <ccr@tnsp.org>
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

#undef FFAUDIO_DOUBLECHECK  /* Doublecheck probing result for debugging purposes */
#undef FFAUDIO_NO_BLACKLIST /* Don't blacklist any recognized codecs/formats */
#define FFAUDIO_USE_AUDTAG  /* Use Audacious tagging library */

#include "config.h"
#include "ffaudio-stdinc.h"
#include <audacious/i18n.h>
#include <audacious/debug.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#ifdef FFAUDIO_USE_AUDTAG
#include <audacious/audtag.h>
#endif
#include <libaudcore/audstrings.h>

/***********************************************************************************
 * Plugin glue.                                                                    *
 ***********************************************************************************/

static GMutex *ctrl_mutex = NULL;
static GCond *ctrl_cond = NULL;
static gint64 seek_value = -1;
static gboolean stop_flag = FALSE;

static gboolean ffaudio_init (void)
{
    avcodec_init();
    av_register_all();

    AUDDBG("registering audvfsptr protocol\n");
#if CHECK_LIBAVFORMAT_VERSION (52, 69, 0)
    av_register_protocol2 (& audvfsptr_protocol, sizeof audvfsptr_protocol);
#else
    av_register_protocol (& audvfsptr_protocol);
#endif

    AUDDBG("creating seek mutex/cond\n");
    ctrl_mutex = g_mutex_new();
    ctrl_cond = g_cond_new();

#ifdef FFAUDIO_USE_AUDTAG
    tag_init();
#endif

    return TRUE;
}

static void
ffaudio_cleanup(void)
{
    AUDDBG("cleaning up\n");
    g_mutex_free(ctrl_mutex);
    g_cond_free(ctrl_cond);
}

static gboolean
ffaudio_codec_is_seekable(AVCodec *codec)
{
    /*
     * Blacklist certain codecs from seeking, which have problems.  Codecs which have
     * problems with seeking should have bugs reported to upstream FFmpeg.  --nenolod
     */
    switch (codec->id) {
#ifndef FFAUDIO_NO_BLACKLIST
        case CODEC_ID_MUSEPACK8:
            AUDDBG("codec is blacklisted from seeking\n");
            return FALSE;
#endif
        default:
            return TRUE;
    }
}

static gint
ffaudio_probe(const gchar *filename, VFSFile *file)
{
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVFormatContext *ic = NULL;
    gint i, ret;
    gchar uribuf[64];

    AUDDBG("probing for %s, filehandle %p\n", filename, file);

    g_snprintf(uribuf, sizeof(uribuf), "audvfsptr:%p", (void *) file);
    if ((ret = av_open_input_file(&ic, uribuf, NULL, 0, NULL)) < 0) {
        AUDDBG("ic is NULL, ret %d/%s\n", ret, strerror(-ret));
        return 0;
    }

    AUDDBG("file opened, %p\n", ic);

    for (i = 0; i < ic->nb_streams; i++)
    {
        c = ic->streams[i]->codec;
        if (c->codec_type == CODEC_TYPE_AUDIO)
        {
            av_find_stream_info(ic);
            codec = avcodec_find_decoder(c->codec_id);
            if (codec != NULL)
                break;
        }
    }

    if (codec == NULL)
    {
        av_close_input_file(ic);
        return 0;
    }

#ifdef FFAUDIO_DOUBLECHECK
    AUDDBG("got codec %s, doublechecking\n", codec->name);

    av_find_stream_info(ic);

    codec = avcodec_find_decoder(c->codec_id);

    if (codec == NULL)
    {
        av_close_input_file(ic);
        return 0;
    }
#endif

    AUDDBG("probe success for %s\n", codec->name);
    av_close_input_file(ic);

    return 1;
}

typedef struct {
    TupleValueType ttype;   /* Tuple field value type */
    gint field;             /* Tuple field constant or if -1, use prim_key as field key */
    gchar *prim_key;        /* Primary FFmpeg metadata key, matches any with this suffix  */
    gchar *alt_keys[5];     /* Fallback keys, strict (but case-insensitive) matching */
} ffaudio_meta_t;

static const ffaudio_meta_t metaentries[] = {
    { TUPLE_STRING, FIELD_ARTIST,       "author",    { "hor", "artist", NULL } },
    { TUPLE_STRING, FIELD_TITLE,        "title",     { "le", NULL } },
    { TUPLE_STRING, FIELD_ALBUM,        "album",     { "WM/AlbumTitle", NULL } },
    { TUPLE_STRING, FIELD_PERFORMER,    "performer", { NULL } },
    { TUPLE_STRING, FIELD_COPYRIGHT,    "copyright", { NULL } },
    { TUPLE_STRING, FIELD_GENRE,        "genre",     { "WM/Genre", NULL } },
    { TUPLE_STRING, FIELD_COMMENT,      "comment",   { NULL } },
    { TUPLE_STRING, FIELD_COMPOSER,     "composer",  { NULL } },
    { TUPLE_STRING, -1,                 "lyrics",    { "WM/Lyrics", NULL } },
    { TUPLE_INT,    FIELD_YEAR,         "year",      { "WM/Year", "date", NULL } },
    { TUPLE_INT,    FIELD_TRACK_NUMBER, "track",     { "WM/TrackNumber", NULL } },
};

static const gint n_metaentries = sizeof(metaentries) / sizeof(metaentries[0]);

static void
ffaudio_get_meta(Tuple *tuple, AVFormatContext *ic, const ffaudio_meta_t *m)
{
    AVMetadataTag *tag = NULL;

    if (ic->metadata != NULL)
    {
        tag = av_metadata_get(ic->metadata, m->prim_key, NULL, AV_METADATA_IGNORE_SUFFIX);
        if (tag == NULL) {
            gint i;
            for (i = 0; tag == NULL && m->alt_keys[i] != NULL; i++)
                tag = av_metadata_get(ic->metadata, m->alt_keys[i], NULL, 0);
        }
    }

    if (tag != NULL)
    {
        const gchar *key_name = (m->field < 0) ? m->prim_key : NULL;
        gchar *tmp;

        switch (m->ttype) {
        case TUPLE_STRING:
            tmp = str_to_utf8(tag->value);
            tuple_associate_string(tuple, m->field, key_name, tmp);
            g_free(tmp);
            break;

        case TUPLE_INT:
            tuple_associate_int(tuple, m->field, key_name, atoi(tag->value));
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
        gint i;
        for (i = 0; i < n_metaentries; i++)
            ffaudio_get_meta(tuple, ic, &metaentries[i]);

        tuple_associate_int(tuple, FIELD_LENGTH, NULL, ic->duration / 1000);
        tuple_associate_int(tuple, FIELD_BITRATE, NULL, ic->bit_rate / 1000);
    }

    if (codec != NULL && codec->long_name != NULL)
    {
        tuple_associate_string(tuple, FIELD_CODEC, NULL, codec->long_name);
    }
}

static Tuple * read_tuple (const gchar * filename, VFSFile * file)
{
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVFormatContext *ic = NULL;
    AVStream *s = NULL;
    gint i;

    gchar uribuf[64];
    snprintf (uribuf, sizeof uribuf, "audvfsptr:%p", (void *) file);

    if (av_open_input_file(&ic, uribuf, NULL, 0, NULL) < 0)
        return NULL;

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

    Tuple *tuple = tuple_new_from_filename(filename);
    ffaudio_get_tuple_data(tuple, ic, c, codec);
    av_close_input_file (ic);

    return tuple;
}

static Tuple *
ffaudio_probe_for_tuple(const gchar *filename, VFSFile *fd)
{
    Tuple * t = read_tuple (filename, fd);
    if (t == NULL)
        return NULL;

#ifdef FFAUDIO_USE_AUDTAG
    vfs_rewind(fd);
    tag_tuple_read(t, fd);
#endif

    return t;
}

#ifdef FFAUDIO_USE_AUDTAG
static gboolean ffaudio_write_tag (const Tuple * tuple, VFSFile * file)
{
    gchar *file_uri = g_ascii_strdown(file->uri, -4);

    if (g_str_has_suffix(file_uri, ".ape"))
    {
        g_free(file_uri);
        return tag_tuple_write(tuple, file, TAG_TYPE_APE);
    }
    g_free(file_uri);

    return tag_tuple_write(tuple, file, TAG_TYPE_NONE);
}
#endif

static gboolean ffaudio_play (InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    if (file == NULL)
        return FALSE;

    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVFormatContext *ic = NULL;
    AVStream *s = NULL;
    AVPacket pkt = {.data = NULL};
    guint8 *outbuf = NULL, *resbuf = NULL;
    gint i, stream_id, errcount;
    gint in_sample_size, out_sample_size, chunk_size;
    ReSampleContext *resctx = NULL;
    gboolean codec_opened = FALSE, do_resampling = FALSE;
    gint out_fmt;
    gboolean seekable;
    gboolean error = FALSE;

    gchar uribuf[64];
    snprintf (uribuf, sizeof uribuf, "audvfsptr:%p", (void *) file);
    if (av_open_input_file(&ic, uribuf, NULL, 0, NULL) < 0)
       return FALSE;

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

    AUDDBG("got codec %s for stream index %d, opening\n", codec->name, stream_id);

    if (avcodec_open(c, codec) < 0)
        goto error_exit;

    codec_opened = TRUE;

    /* Determine if audio conversion or resampling is needed */
#if CHECK_LIBAVCODEC_VERSION (52, 94, 3)
    in_sample_size = av_get_bits_per_sample_fmt (c->sample_fmt) / 8;
    out_sample_size = av_get_bits_per_sample_fmt (SAMPLE_FMT_S16) / 8;
#else
    in_sample_size = av_get_bits_per_sample_format (c->sample_fmt) / 8;
    out_sample_size = av_get_bits_per_sample_format (SAMPLE_FMT_S16) / 8;
#endif

    chunk_size = out_sample_size * c->channels * (c->sample_rate / 50);

    switch (c->sample_fmt) {
        case SAMPLE_FMT_U8: out_fmt = FMT_U8; break;
        case SAMPLE_FMT_S16: out_fmt = FMT_S16_NE; break;
        case SAMPLE_FMT_S32: out_fmt = FMT_S32_NE; break;
        case SAMPLE_FMT_FLT: out_fmt = FMT_FLOAT; break;
        default: do_resampling = TRUE; break;
    }

    if (do_resampling)
    {
        /* Initialize resampling context */
        out_fmt = FMT_S16_NE;

        AUDDBG("resampling needed chn=%d, rate=%d, fmt=%d -> chn=%d, rate=%d, fmt=S16NE\n",
            c->channels, c->sample_rate, c->sample_fmt,
            c->channels, c->sample_rate);

        resctx = av_audio_resample_init(
            c->channels, c->channels,
            c->sample_rate, c->sample_rate,
            SAMPLE_FMT_S16, c->sample_fmt,
            16, 10, 0, 0.8);

        if (resctx == NULL)
            goto error_exit;
    }

    /* Open audio output */
    AUDDBG("opening audio output\n");

    if (playback->output->open_audio(out_fmt, c->sample_rate, c->channels) <= 0)
    {
        error = TRUE;
        goto error_exit;
    }

    playback->set_gain_from_playlist(playback);

    /* Allocate output buffer aligned to 16 byte boundary */
    outbuf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    resbuf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

    AUDDBG("setting parameters\n");

    if (pause)
        playback->output->pause(TRUE);

    playback->set_params(playback, ic->bit_rate, c->sample_rate, c->channels);

    g_mutex_lock(ctrl_mutex);

    stop_flag = FALSE;
    seek_value = (start_time > 0) ? start_time : -1;
    playback->set_pb_ready(playback);
    errcount = 0;
    seekable = ffaudio_codec_is_seekable(codec);

    g_mutex_unlock(ctrl_mutex);

    while (!stop_flag && (stop_time < 0 ||
     playback->output->written_time () < stop_time))
    {
        AVPacket tmp;
        gint ret;

        g_mutex_lock(ctrl_mutex);

        if (seek_value >= 0 && seekable)
        {
            playback->output->flush (seek_value);
            if (av_seek_frame (ic, -1, (gint64) seek_value * AV_TIME_BASE /
             1000, AVSEEK_FLAG_ANY) < 0)
            {
                _ERROR("error while seeking\n");
            } else
                errcount = 0;
        }
        seek_value = -1;
        g_cond_signal(ctrl_cond);


        g_mutex_unlock(ctrl_mutex);

        /* Read next frame (or more) of data */
        if ((ret = av_read_frame(ic, &pkt)) < 0)
        {
            if (ret == AVERROR_EOF)
            {
                AUDDBG("eof reached\n");
                break;
            }
            else
            {
                if (++errcount > 4)
                {
                    _ERROR("av_read_frame error %d, giving up.\n", ret);
                    break;
                } else
                    continue;
            }
        } else
            errcount = 0;

        /* Ignore any other substreams */
        if (pkt.stream_index != stream_id)
        {
            av_free_packet(&pkt);
            continue;
        }

        /* Decode and play packet/frame */
        memcpy(&tmp, &pkt, sizeof(tmp));
        while (tmp.size > 0 && !stop_flag)
        {
            gint len, out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
            guint8 *outbuf_p;

            /* Check for seek request and bail out if we have one */
            g_mutex_lock(ctrl_mutex);
            if (seek_value != -1)
            {
                if (!seekable)
                {
                    seek_value = -1;
                    g_cond_signal(ctrl_cond);
                }
                else
                {
                    g_mutex_unlock(ctrl_mutex);
                    break;
                }
            }
            g_mutex_unlock(ctrl_mutex);

            /* Decode whatever we can of the frame data */
#if (LIBAVCODEC_VERSION_MAJOR <= 52) && (LIBAVCODEC_VERSION_MINOR <= 25)
            len = avcodec_decode_audio2(c, (gint16 *)outbuf, &out_size, tmp.data, tmp.size);
#else
            len = avcodec_decode_audio3(c, (gint16 *)outbuf, &out_size, &tmp);
#endif
            if (len < 0)
            {
                AUDDBG("codec failure, breaking out of loop\n");
                break;
            }

            tmp.size -= len;
            tmp.data += len;

            if (out_size <= 0)
                continue;

            /* Perform audio resampling if necessary */
            if (do_resampling)
            {
                out_size = audio_resample(resctx,
                    (gint16 *)resbuf, (gint16 *)outbuf,
                    out_size / in_sample_size) * out_sample_size;
                outbuf_p = resbuf;
            }
            else
                outbuf_p = outbuf;

            /* Output audio in small blocks */
            while (out_size > 0 && !stop_flag && (stop_time < 0 ||
             playback->output->written_time () < stop_time))
            {
                gint writeoff = MIN (chunk_size, out_size);

                playback->output->write_audio((gint16 *)outbuf_p, writeoff);

                outbuf_p += writeoff;
                out_size -= writeoff;

                /* Check for seek request and bail out if we have one */
                g_mutex_lock(ctrl_mutex);
                if (seek_value != -1)
                {
                    if (!seekable)
                    {
                        seek_value = -1;
                        g_cond_signal(ctrl_cond);
                    }
                    else
                    {
                        g_mutex_unlock(ctrl_mutex);
                        break;
                    }
                }
                g_mutex_unlock(ctrl_mutex);
            }
        }

        if (pkt.data)
            av_free_packet(&pkt);
    }

    g_mutex_lock(ctrl_mutex);

    while (!stop_flag && playback->output->buffer_playing())
        g_usleep(20000);

    playback->output->close_audio();

    g_cond_signal(ctrl_cond); /* wake up any waiting request */
    g_mutex_unlock(ctrl_mutex);

error_exit:

    AUDDBG("decode loop finished, shutting down\n");

    stop_flag = TRUE;

    av_free(outbuf);
    av_free(resbuf);
    if (resctx != NULL)
        audio_resample_close(resctx);
    if (pkt.data)
        av_free_packet(&pkt);
    if (codec_opened)
        avcodec_close(c);
    if (ic != NULL)
        av_close_input_file(ic);

    AUDDBG("exiting thread\n");
    return ! error;
}

static void ffaudio_stop(InputPlayback * playback)
{
    g_mutex_lock(ctrl_mutex);

    if (!stop_flag)
    {
        stop_flag = TRUE;
        playback->output->abort_write();
        g_cond_signal(ctrl_cond);
    }

    g_mutex_unlock (ctrl_mutex);
}

static void ffaudio_pause(InputPlayback * playback, gboolean pause)
{
    g_mutex_lock(ctrl_mutex);

    if (!stop_flag)
        playback->output->pause(pause);

    g_mutex_unlock(ctrl_mutex);
}

static void ffaudio_seek (InputPlayback * playback, gint time)
{
    g_mutex_lock(ctrl_mutex);

    if (!stop_flag)
    {
        seek_value = time;
        playback->output->abort_write();
        g_cond_signal(ctrl_cond);
        g_cond_wait(ctrl_cond, ctrl_mutex);
    }

    g_mutex_unlock(ctrl_mutex);
}

static const gchar *ffaudio_fmts[] = {
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

    /* monkey's audio */
    "ape",

    /* VQF */
    "vqf",

    /* Apple Lossless */
    "m4a",

    /* WAV (there are some WAV formats sndfile can't handle) */
    "wav",

    /* Handle OGG streams (FLAC/Vorbis etc.) */
    "ogg",

    /* Speex */
    "spx",

    /* end of table */
    NULL
};

static void
ffaudio_about(void)
{
    static GtkWidget *aboutbox = NULL;

    if (aboutbox == NULL)
    {
        gchar *description = g_strdup_printf(
        _("Multi-format audio decoding plugin for Audacious based on\n"
        "FFmpeg multimedia framework (http://www.ffmpeg.org/)\n"
        "Copyright (c) 2000-2009 Fabrice Bellard, et al.\n"
        "\n"
        "FFmpeg libavformat %d.%d.%d, libavcodec %d.%d.%d\n"
        "\n"
        "Audacious plugin by:\n"
        "            William Pitcock <nenolod@nenolod.net>,\n"
        "            Matti Hämäläinen <ccr@tnsp.org>\n"),
        LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO,
        LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);

        audgui_simple_message (& aboutbox, GTK_MESSAGE_INFO,
         _("About FFaudio Plugin"), description);

        g_free(description);
    }
}

InputPlugin ffaudio_ip = {
    .init = ffaudio_init,
    .cleanup = ffaudio_cleanup,
    .is_our_file_from_vfs = ffaudio_probe,
    .probe_for_tuple = ffaudio_probe_for_tuple,
    .play = ffaudio_play,
    .stop = ffaudio_stop,
    .pause = ffaudio_pause,
    .mseek = ffaudio_seek,
    .about = ffaudio_about,
    .description = "FFaudio Plugin",
    .vfs_extensions = ffaudio_fmts,
#ifdef FFAUDIO_USE_AUDTAG
    .update_song_tuple = ffaudio_write_tag,
#endif

    /* FFMPEG probing takes forever on network files, so try everything else
     * first. -jlindgren */
    .priority = 10,
};

static InputPlugin *ffaudio_iplist[] = { &ffaudio_ip, NULL };

SIMPLE_INPUT_PLUGIN (ffaudio, ffaudio_iplist)
