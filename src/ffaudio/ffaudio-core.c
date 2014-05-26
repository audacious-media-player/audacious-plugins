/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
 *                  Matti Hämäläinen <ccr@tnsp.org>
 * Copyright © 2011 John Lindgren <john.lindgren@tds.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 */

#include <glib.h>
#include <pthread.h>

#undef FFAUDIO_DOUBLECHECK  /* Doublecheck probing result for debugging purposes */
#undef FFAUDIO_NO_BLACKLIST /* Don't blacklist any recognized codecs/formats */

#include "ffaudio-stdinc.h"
#include <audacious/i18n.h>
#include <audacious/input.h>
#include <audacious/debug.h>
#include <audacious/audtag.h>
#include <libaudcore/audstrings.h>

static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static GHashTable * extension_dict = NULL;

typedef struct
{
    int stream_idx;
    AVStream * stream;
    AVCodecContext * context;
    AVCodec * codec;
}
CodecInfo;

static gint lockmgr (void * * mutexp, enum AVLockOp op)
{
    switch (op)
    {
    case AV_LOCK_CREATE:
        * mutexp = g_slice_new (pthread_mutex_t);
        pthread_mutex_init (* mutexp, NULL);
        break;
    case AV_LOCK_OBTAIN:
        pthread_mutex_lock (* mutexp);
        break;
    case AV_LOCK_RELEASE:
        pthread_mutex_unlock (* mutexp);
        break;
    case AV_LOCK_DESTROY:
        pthread_mutex_destroy (* mutexp);
        g_slice_free (pthread_mutex_t, * mutexp);
        break;
    }

    return 0;
}

static gboolean ffaudio_init (void)
{
    av_register_all();
    av_lockmgr_register (lockmgr);

    return TRUE;
}

static void
ffaudio_cleanup(void)
{
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
    const gchar * ext0, * sub;
    uri_parse (name, NULL, & ext0, & sub, NULL);

    if (ext0 == sub)
        return NULL;

    gchar * ext = g_ascii_strdown (ext0 + 1, sub - ext0 - 1);

    AUDDBG ("Get format by extension: %s\n", name);
    pthread_mutex_lock (& data_mutex);

    if (! extension_dict)
        extension_dict = create_extension_dict ();

    AVInputFormat * f = g_hash_table_lookup (extension_dict, ext);
    pthread_mutex_unlock (& data_mutex);

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

        memset (buf + filled, 0, AVPROBE_PADDING_SIZE);
        AVProbeData d = {name, buf, filled};
        score = target;

        f = av_probe_input_format2 (& d, TRUE, & score);
        if (f)
            break;

        if (size < 16384 && filled == size)
            size *= 4;
        else if (target > 10)
            target = 10;
        else
            break;
    }

    if (f)
        AUDDBG ("Format %s, buffer size %d, score %d.\n", f->name, filled, score);
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
    AVInputFormat * f = get_format (name, file);

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

#if CHECK_LIBAVFORMAT_VERSION (53, 25, 0, 53, 17, 0)
    avformat_close_input (&c);
#else
    av_close_input_file (c);
#endif

    io_context_free (io);
}

static bool_t find_codec (AVFormatContext * c, CodecInfo * cinfo)
{
    avformat_find_stream_info (c, NULL);

    for (int i = 0; i < c->nb_streams; i++)
    {
        AVStream * stream = c->streams[i];

        if (stream && stream->codec && stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            AVCodec * codec = avcodec_find_decoder (stream->codec->codec_id);

            if (codec)
            {
                cinfo->stream_idx = i;
                cinfo->stream = stream;
                cinfo->context = stream->codec;
                cinfo->codec = codec;

                return TRUE;
            }
        }
    }

    return FALSE;
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

static void read_metadata_dict (Tuple * tuple, AVDictionary * dict)
{
    for (int i = 0; i < ARRAY_LEN (metaentries); i ++)
    {
        const ffaudio_meta_t * m = & metaentries[i];
        AVDictionaryEntry * entry = NULL;

        for (int j = 0; ! entry && m->keys[j]; j ++)
            entry = av_dict_get (dict, m->keys[j], NULL, 0);

        if (entry && entry->value)
        {
            if (m->ttype == TUPLE_STRING)
                tuple_set_str (tuple, m->field, entry->value);
            else if (m->ttype == TUPLE_INT)
                tuple_set_int (tuple, m->field, atoi (entry->value));
        }
    }
}

static Tuple * read_tuple (const gchar * filename, VFSFile * file)
{
    Tuple * tuple = NULL;
    AVFormatContext * ic = open_input_file (filename, file);

    if (ic)
    {
        CodecInfo cinfo;

        if (find_codec (ic, & cinfo))
        {
            tuple = tuple_new_from_filename (filename);

            tuple_set_int (tuple, FIELD_LENGTH, ic->duration / 1000);
            tuple_set_int (tuple, FIELD_BITRATE, ic->bit_rate / 1000);

            if (cinfo.codec->long_name)
                tuple_set_str (tuple, FIELD_CODEC, cinfo.codec->long_name);

            if (ic->metadata)
                read_metadata_dict (tuple, ic->metadata);
            if (cinfo.stream->metadata)
                read_metadata_dict (tuple, cinfo.stream->metadata);
        }

        close_input_file (ic);
    }

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

    if (! vfs_fseek (fd, 0, SEEK_SET))
        tag_tuple_read (t, fd);

    return t;
}

static gboolean ffaudio_write_tag (const char * filename, VFSFile * file, const Tuple * tuple)
{
    if (! file)
        return FALSE;

    if (str_has_suffix_nocase (vfs_get_filename (file), ".ape"))
        return tag_tuple_write(tuple, file, TAG_TYPE_APE);

    return tag_tuple_write(tuple, file, TAG_TYPE_NONE);
}

static gboolean ffaudio_play (const gchar * filename, VFSFile * file)
{
    AUDDBG ("Playing %s.\n", filename);
    if (! file)
        return FALSE;

    AVPacket pkt = {.data = NULL};
    gint errcount;
    gboolean codec_opened = FALSE;
    gint out_fmt;
    gboolean planar;
    gboolean error = FALSE;

    void *buf = NULL;
    gint bufsize = 0;

    AVFormatContext * ic = open_input_file (filename, file);
    if (! ic)
        return FALSE;

    CodecInfo cinfo;

    if (! find_codec (ic, & cinfo))
    {
        fprintf (stderr, "ffaudio: No codec found for %s.\n", filename);
        goto error_exit;
    }

    AUDDBG("got codec %s for stream index %d, opening\n", cinfo.codec->name, cinfo.stream_idx);

    if (avcodec_open2 (cinfo.context, cinfo.codec, NULL) < 0)
        goto error_exit;

    codec_opened = TRUE;

    switch (cinfo.context->sample_fmt)
    {
        case AV_SAMPLE_FMT_U8: out_fmt = FMT_U8; planar = FALSE; break;
        case AV_SAMPLE_FMT_S16: out_fmt = FMT_S16_NE; planar = FALSE; break;
        case AV_SAMPLE_FMT_S32: out_fmt = FMT_S32_NE; planar = FALSE; break;
        case AV_SAMPLE_FMT_FLT: out_fmt = FMT_FLOAT; planar = FALSE; break;

        case AV_SAMPLE_FMT_U8P: out_fmt = FMT_U8; planar = TRUE; break;
        case AV_SAMPLE_FMT_S16P: out_fmt = FMT_S16_NE; planar = TRUE; break;
        case AV_SAMPLE_FMT_S32P: out_fmt = FMT_S32_NE; planar = TRUE; break;
        case AV_SAMPLE_FMT_FLTP: out_fmt = FMT_FLOAT; planar = TRUE; break;

    default:
        fprintf (stderr, "ffaudio: Unsupported audio format %d\n", (int) cinfo.context->sample_fmt);
        goto error_exit;
    }

    /* Open audio output */
    AUDDBG("opening audio output\n");

    if (aud_input_open_audio(out_fmt, cinfo.context->sample_rate, cinfo.context->channels) <= 0)
    {
        error = TRUE;
        goto error_exit;
    }

    AUDDBG("setting parameters\n");

    aud_input_set_bitrate(ic->bit_rate);

    errcount = 0;

    while (! aud_input_check_stop ())
    {
        int seek_value = aud_input_check_seek ();

        if (seek_value >= 0)
        {
            if (av_seek_frame (ic, -1, (gint64) seek_value * AV_TIME_BASE /
             1000, AVSEEK_FLAG_ANY) < 0)
            {
                _ERROR("error while seeking\n");
            } else
                errcount = 0;

            seek_value = -1;
        }

        AVPacket tmp;
        gint ret;

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
        if (pkt.stream_index != cinfo.stream_idx)
        {
            av_free_packet(&pkt);
            continue;
        }

        /* Decode and play packet/frame */
        memcpy(&tmp, &pkt, sizeof(tmp));
        while (tmp.size > 0 && ! aud_input_check_stop ())
        {
            /* Check for seek request and bail out if we have one */
            if (seek_value < 0)
                seek_value = aud_input_check_seek ();

            if (seek_value >= 0)
                break;

#if CHECK_LIBAVCODEC_VERSION (55, 45, 101, 55, 28, 1)
            AVFrame * frame = av_frame_alloc ();
#else
            AVFrame * frame = avcodec_alloc_frame ();
#endif
            int decoded = 0;
            int len = avcodec_decode_audio4 (cinfo.context, frame, & decoded, & tmp);

            if (len < 0)
            {
                fprintf (stderr, "ffaudio: decode_audio() failed, code %d\n", len);
                break;
            }

            tmp.size -= len;
            tmp.data += len;

            if (! decoded)
                continue;

            gint size = FMT_SIZEOF (out_fmt) * cinfo.context->channels * frame->nb_samples;

            if (planar)
            {
                if (bufsize < size)
                {
                    buf = g_realloc (buf, size);
                    bufsize = size;
                }

                audio_interlace ((const void * *) frame->data, out_fmt,
                 cinfo.context->channels, buf, frame->nb_samples);
                aud_input_write_audio (buf, size);
            }
            else
                aud_input_write_audio (frame->data[0], size);

#if CHECK_LIBAVCODEC_VERSION (55, 45, 101, 55, 28, 1)
            av_frame_free (& frame);
#elif CHECK_LIBAVCODEC_VERSION (54, 59, 100, 54, 28, 0)
            avcodec_free_frame (& frame);
#else
            av_free (frame);
#endif
        }

        if (pkt.data)
            av_free_packet(&pkt);
    }

error_exit:
    if (pkt.data)
        av_free_packet(&pkt);
    if (codec_opened)
        avcodec_close(cinfo.context);
    if (ic != NULL)
        close_input_file(ic);

    g_free (buf);

    return ! error;
}

static const char ffaudio_about[] =
 N_("Multi-format audio decoding plugin for Audacious using\n"
    "FFmpeg multimedia framework (http://www.ffmpeg.org/)\n"
    "\n"
    "Audacious plugin by:\n"
    "William Pitcock <nenolod@nenolod.net>\n"
    "Matti Hämäläinen <ccr@tnsp.org>");

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
    "ogg", "oga",

    /* Opus */
    "opus",

    /* Speex */
    "spx",

    /* end of table */
    NULL
};

static const char * const ffaudio_mimes[] = {"application/ogg", NULL};

AUD_INPUT_PLUGIN
(
    .name = N_("FFmpeg Plugin"),
    .domain = PACKAGE,
    .about_text = ffaudio_about,
    .init = ffaudio_init,
    .cleanup = ffaudio_cleanup,
    .extensions = ffaudio_fmts,
    .mimes = ffaudio_mimes,
    .is_our_file_from_vfs = ffaudio_probe,
    .probe_for_tuple = ffaudio_probe_for_tuple,
    .play = ffaudio_play,
    .update_song_tuple = ffaudio_write_tag,

    /* lowest priority fallback */
    .priority = 10,
)
