/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
 *                  Matti Hämäläinen <ccr@tnsp.org>
 * Copyright © 2011 John Lindgren <john.lindgren@tds.net>
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

/*
 * NOTE: Using libav with this code is entirely unsupported.  Do so at your own
 * risk.  Any bugs filed against this plugin on systems using libav will be rejected
 * by us.
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
#include <libaudcore/strpool.h>

#if ! CHECK_LIBAVFORMAT_VERSION (53, 5, 0)
#define avformat_find_stream_info(i, o) av_find_stream_info (i)
#endif

#if ! CHECK_LIBAVCODEC_VERSION (53, 8, 0)
#define avcodec_open2(a, c, o) avcodec_open (a, c)
#endif

static GMutex *ctrl_mutex = NULL;
static GCond *ctrl_cond = NULL;
static gint64 seek_value = -1;
static gboolean stop_flag = FALSE;

static GStaticMutex data_mutex = G_STATIC_MUTEX_INIT;
static GHashTable * extension_dict = NULL;

static gint lockmgr (void * * mutexp, enum AVLockOp op)
{
    switch (op)
    {
    case AV_LOCK_CREATE:
        * mutexp = g_mutex_new ();
        break;
    case AV_LOCK_OBTAIN:
        g_mutex_lock (* mutexp);
        break;
    case AV_LOCK_RELEASE:
        g_mutex_unlock (* mutexp);
        break;
    case AV_LOCK_DESTROY:
        g_mutex_free (* mutexp);
        break;
    }

    return 0;
}

static gboolean ffaudio_init (void)
{
    av_register_all();
    av_lockmgr_register (lockmgr);

    ctrl_mutex = g_mutex_new();
    ctrl_cond = g_cond_new();

    return TRUE;
}

static void
ffaudio_cleanup(void)
{
    AUDDBG("cleaning up\n");
    g_mutex_free(ctrl_mutex);
    g_cond_free(ctrl_cond);

    if (extension_dict)
        g_hash_table_destroy (extension_dict);

    av_lockmgr_register (NULL);
}

static const gchar * ffaudio_strerror (gint error)
{
    static gchar buf[256];
    return (! av_strerror (error, buf, sizeof buf)) ? buf : "unknown error";
}

static GHashTable * create_extension_dict (void)
{
    GHashTable * dict = g_hash_table_new_full (g_str_hash, g_str_equal,
     (GDestroyNotify) str_unref, NULL);

    AVInputFormat * f;
    for (f = av_iformat_next (NULL); f; f = av_iformat_next (f))
    {
        if (! f->extensions)
            continue;

        gchar * exts = g_ascii_strdown (f->extensions, -1);

        gchar * parse, * next;
        for (parse = exts; parse; parse = next)
        {
            next = strchr (parse, ',');
            if (next)
            {
                * next = 0;
                next ++;
            }

            g_hash_table_insert (dict, str_get(parse), f);
        }

        g_free (exts);
    }

    return dict;
}

static AVInputFormat * get_format_by_extension (const gchar * name)
{
    gchar * ext = uri_get_extension (name);
    if (! ext)
        return NULL;

    AUDDBG ("Get format by extension: %s\n", name);
    g_static_mutex_lock (& data_mutex);

    if (! extension_dict)
        extension_dict = create_extension_dict ();

    AVInputFormat * f = g_hash_table_lookup (extension_dict, ext);
    g_static_mutex_unlock (& data_mutex);

    if (f)
        AUDDBG ("Format %s.\n", f->name);
    else
        AUDDBG ("Format unknown.\n");

    g_free (ext);
    return f;
}

static AVInputFormat * get_format_by_content (const gchar * name, VFSFile * file)
{
    AUDDBG ("Get format by content: %s\n", name);

    AVInputFormat * f = NULL;

    guchar buf[16384 + AVPROBE_PADDING_SIZE];
    gint size = 16;
    gint filled = 0;
    gint target = 100;
    gint score = 0;

    while (1)
    {
        if (filled < size)
            filled += vfs_fread (buf + filled, 1, size - filled, file);
        if (filled < size)
            break;

        memset (buf + size, 0, AVPROBE_PADDING_SIZE);
        AVProbeData d = {name, buf, size};
        score = target;

        f = av_probe_input_format2 (& d, TRUE, & score);
        if (f)
            break;

        if (size < 16384)
            size *= 4;
        else if (target > 10)
            target = 10;
        else
            break;
    }

    if (f)
        AUDDBG ("Format %s, buffer size %d, score %d.\n", f->name, size, score);
    else
        AUDDBG ("Format unknown.\n");

    if (vfs_fseek (file, 0, SEEK_SET) < 0)
        ; /* ignore errors here */

    return f;
}

static AVInputFormat * get_format (const gchar * name, VFSFile * file)
{
    AVInputFormat * f = get_format_by_extension (name);
    return f ? f : get_format_by_content (name, file);
}

static AVFormatContext * open_input_file (const gchar * name, VFSFile * file)
{
    AVInputFormat * f = get_format (file->uri, file);

    if (! f)
    {
        fprintf (stderr, "ffaudio: Unknown format for %s.\n", name);
        return NULL;
    }

    AVFormatContext * c = avformat_alloc_context ();
    AVIOContext * io = io_context_new (file);
    c->pb = io;

    gint ret = avformat_open_input (& c, name, f, NULL);

    if (ret < 0)
    {
        fprintf (stderr, "ffaudio: avformat_open_input failed for %s: %s.\n", name, ffaudio_strerror (ret));
        io_context_free (io);
        return NULL;
    }

    return c;
}

static void close_input_file (AVFormatContext * c)
{
    AVIOContext * io = c->pb;
    av_close_input_file (c);
    io_context_free (io);
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

static gboolean ffaudio_probe (const gchar * filename, VFSFile * file)
{
    if (! file)
        return FALSE;

    return get_format (filename, file) ? TRUE : FALSE;
}

typedef struct {
    TupleValueType ttype;   /* Tuple field value type */
    gint field;             /* Tuple field constant */
    gchar *keys[5];         /* Keys to match (case-insensitive), ended by NULL */
} ffaudio_meta_t;

static const ffaudio_meta_t metaentries[] = {
 {TUPLE_STRING, FIELD_ARTIST,       {"author", "hor", "artist", NULL}},
 {TUPLE_STRING, FIELD_TITLE,        {"title", "le", NULL}},
 {TUPLE_STRING, FIELD_ALBUM,        {"album", "WM/AlbumTitle", NULL}},
 {TUPLE_STRING, FIELD_PERFORMER,    {"performer", NULL}},
 {TUPLE_STRING, FIELD_COPYRIGHT,    {"copyright", NULL}},
 {TUPLE_STRING, FIELD_GENRE,        {"genre", "WM/Genre", NULL}},
 {TUPLE_STRING, FIELD_COMMENT,      {"comment", NULL}},
 {TUPLE_STRING, FIELD_COMPOSER,     {"composer", NULL}},
 {TUPLE_INT,    FIELD_YEAR,         {"year", "WM/Year", "date", NULL}},
 {TUPLE_INT,    FIELD_TRACK_NUMBER, {"track", "WM/TrackNumber", NULL}},
};

static const gint n_metaentries = sizeof(metaentries) / sizeof(metaentries[0]);

static void
ffaudio_get_meta(Tuple *tuple, AVFormatContext *ic, const ffaudio_meta_t *m)
{
    if (! ic->metadata)
        return;

    AVDictionaryEntry *tag = NULL;

    for (gint i = 0; ! tag && m->keys[i]; i ++)
        tag = av_dict_get (ic->metadata, m->keys[i], NULL, 0);

    if (tag != NULL)
    {
        switch (m->ttype) {
        case TUPLE_STRING:
            tuple_copy_str (tuple, m->field, NULL, tag->value);
            break;

        case TUPLE_INT:
            tuple_set_int (tuple, m->field, NULL, atoi(tag->value));
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

        tuple_set_int(tuple, FIELD_LENGTH, NULL, ic->duration / 1000);
        tuple_set_int(tuple, FIELD_BITRATE, NULL, ic->bit_rate / 1000);
    }

    if (codec != NULL && codec->long_name != NULL)
    {
        tuple_copy_str(tuple, FIELD_CODEC, NULL, codec->long_name);
    }
}

static Tuple * read_tuple (const gchar * filename, VFSFile * file)
{
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVStream *s = NULL;
    gint i;

    AVFormatContext * ic = open_input_file (filename, file);
    if (! ic)
        return NULL;

    for (i = 0; i < ic->nb_streams; i++)
    {
        s = ic->streams[i];
        c = s->codec;
        if (c->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            avformat_find_stream_info (ic, NULL);
            codec = avcodec_find_decoder(c->codec_id);
            if (codec != NULL)
                break;
        }
    }

    Tuple *tuple = tuple_new_from_filename(filename);
    ffaudio_get_tuple_data(tuple, ic, c, codec);
    close_input_file (ic);

    return tuple;
}

static Tuple *
ffaudio_probe_for_tuple(const gchar *filename, VFSFile *fd)
{
    if (! fd)
        return NULL;

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
    if (! file)
        return FALSE;

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
    AUDDBG ("Playing %s.\n", filename);
    if (! file)
        return FALSE;

    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
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

    AVFormatContext * ic = open_input_file (filename, file);
    if (! ic)
        return FALSE;

    for (i = 0; i < ic->nb_streams; i++)
    {
        s = ic->streams[i];
        c = s->codec;
        if (c->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            avformat_find_stream_info (ic, NULL);
            codec = avcodec_find_decoder(c->codec_id);
            stream_id = i;
            if (codec != NULL)
                break;
        }
    }

    if (codec == NULL)
    {
        fprintf (stderr, "ffaudio: No codec found for %s.\n", filename);
        goto error_exit;
    }

    AUDDBG("got codec %s for stream index %d, opening\n", codec->name, stream_id);

    if (avcodec_open2 (c, codec, NULL) < 0)
        goto error_exit;

    codec_opened = TRUE;

    /* Determine if audio conversion or resampling is needed */
    in_sample_size = av_get_bytes_per_sample (c->sample_fmt);
    out_sample_size = av_get_bytes_per_sample (SAMPLE_FMT_S16);

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
            len = avcodec_decode_audio3(c, (gint16 *)outbuf, &out_size, &tmp);
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
        close_input_file(ic);

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

static gchar * version_string (gint version)
{
    gint major = version >> 16;
    gint minor = (version >> 8) & 0xff;
    gint micro = version & 0xff;
    return g_strdup_printf ("%d.%d.%d", major, minor, micro);
}

static void
ffaudio_about(void)
{
    static GtkWidget *aboutbox = NULL;

    if (aboutbox == NULL)
    {
        gchar * avcodec_local = version_string (avcodec_version ());
        gchar * avcodec_build = version_string (LIBAVCODEC_VERSION_INT);
        gchar * avformat_local = version_string (avformat_version ());
        gchar * avformat_build = version_string (LIBAVFORMAT_VERSION_INT);
        gchar * avutil_local = version_string (avutil_version ());
        gchar * avutil_build = version_string (LIBAVUTIL_VERSION_INT);

        gchar *description = g_strdup_printf(
        _("Multi-format audio decoding plugin for Audacious based on\n"
        "FFmpeg multimedia framework (http://www.ffmpeg.org/)\n"
        "Copyright (c) 2000-2009 Fabrice Bellard, et al.\n"
        "\n"
        "Audacious plugin by:\n"
        "            William Pitcock <nenolod@nenolod.net>,\n"
        "            Matti Hämäläinen <ccr@tnsp.org>\n"
        "\n"
        "libavcodec %s (%s)\n"
        "libavformat %s (%s)\n"
        "libavutil %s (%s)\n"),
         avcodec_local, avcodec_build, avformat_local, avformat_build, avutil_local, avutil_build);

        audgui_simple_message (& aboutbox, GTK_MESSAGE_INFO,
         _("About FFaudio Plugin"), description);

        g_free(description);
        g_free (avcodec_local);
        g_free (avcodec_build);
        g_free (avformat_local);
        g_free (avformat_build);
        g_free (avutil_local);
        g_free (avutil_build);
    }
}

AUD_INPUT_PLUGIN
(
    .init = ffaudio_init,
    .cleanup = ffaudio_cleanup,
    .is_our_file_from_vfs = ffaudio_probe,
    .probe_for_tuple = ffaudio_probe_for_tuple,
    .play = ffaudio_play,
    .stop = ffaudio_stop,
    .pause = ffaudio_pause,
    .mseek = ffaudio_seek,
    .about = ffaudio_about,
    .name = "FFmpeg Support",
    .extensions = ffaudio_fmts,
#ifdef FFAUDIO_USE_AUDTAG
    .update_song_tuple = ffaudio_write_tag,
#endif

    /* FFMPEG probing takes forever on network files, so try everything else
     * first. -jlindgren */
    .priority = 10,
)
