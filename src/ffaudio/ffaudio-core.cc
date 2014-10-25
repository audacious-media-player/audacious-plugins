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

#include <audacious/audtag.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/input.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>

typedef struct
{
    int stream_idx;
    AVStream * stream;
    AVCodecContext * context;
    AVCodec * codec;
}
CodecInfo;

static SimpleHash<String, AVInputFormat *> extension_dict;

static void create_extension_dict ();

static int lockmgr (void * * mutexp, enum AVLockOp op)
{
    switch (op)
    {
    case AV_LOCK_CREATE:
        * mutexp = g_slice_new (pthread_mutex_t);
        pthread_mutex_init ((pthread_mutex_t *) * mutexp, nullptr);
        break;
    case AV_LOCK_OBTAIN:
        pthread_mutex_lock ((pthread_mutex_t *) * mutexp);
        break;
    case AV_LOCK_RELEASE:
        pthread_mutex_unlock ((pthread_mutex_t *) * mutexp);
        break;
    case AV_LOCK_DESTROY:
        pthread_mutex_destroy ((pthread_mutex_t *) * mutexp);
        g_slice_free (pthread_mutex_t, * mutexp);
        break;
    }

    return 0;
}

static void ffaudio_log_cb (void * avcl, int av_level, const char * fmt, va_list va)
{
    audlog::Level level = audlog::Debug;
    char message [2048];

    switch (av_level)
    {
    case AV_LOG_QUIET:
        return;
    case AV_LOG_PANIC:
    case AV_LOG_FATAL:
    case AV_LOG_ERROR:
        level = audlog::Error;
        break;
    case AV_LOG_WARNING:
        level = audlog::Warning;
        break;
    case AV_LOG_INFO:
        level = audlog::Info;
        break;
    default:
        break;
    }

    AVClass * avc = avcl ? * (AVClass * *) avcl : nullptr;

    vsnprintf (message, sizeof message, fmt, va);

    audlog::log (level, __FILE__, __LINE__, avc ? avc->item_name(avcl) : __FUNCTION__,
                 "<%p> %s", avcl, message);
}

static bool ffaudio_init (void)
{
    av_register_all();
    av_lockmgr_register (lockmgr);

    create_extension_dict ();

    av_log_set_callback (ffaudio_log_cb);

    return true;
}

static void
ffaudio_cleanup(void)
{
    extension_dict.clear ();

    av_lockmgr_register (nullptr);
}

static const char * ffaudio_strerror (int error)
{
    static char buf[256];
    return (! av_strerror (error, buf, sizeof buf)) ? buf : "unknown error";
}

static void create_extension_dict ()
{
    AVInputFormat * f;
    for (f = av_iformat_next (nullptr); f; f = av_iformat_next (f))
    {
        if (! f->extensions)
            continue;

        char * exts = g_ascii_strdown (f->extensions, -1);

        char * parse, * next;
        for (parse = exts; parse; parse = next)
        {
            next = strchr (parse, ',');
            if (next)
            {
                * next = 0;
                next ++;
            }

            extension_dict.add (String (parse), std::move (f));
        }

        g_free (exts);
    }
}

static AVInputFormat * get_format_by_extension (const char * name)
{
    const char * ext0, * sub;
    uri_parse (name, nullptr, & ext0, & sub, nullptr);

    if (ext0 == sub)
        return nullptr;

    char * ext = g_ascii_strdown (ext0 + 1, sub - ext0 - 1);

    AUDDBG ("Get format by extension: %s\n", name);
    AVInputFormat * * f = extension_dict.lookup (String (ext));

    if (f && * f)
        AUDDBG ("Format %s.\n", (* f)->name);
    else
        AUDDBG ("Format unknown.\n");

    g_free (ext);
    return f ? * f : nullptr;
}

static AVInputFormat * get_format_by_content (const char * name, VFSFile & file)
{
    AUDDBG ("Get format by content: %s\n", name);

    AVInputFormat * f = nullptr;

    unsigned char buf[16384 + AVPROBE_PADDING_SIZE];
    int size = 16;
    int filled = 0;
    int target = 100;
    int score = 0;

    while (1)
    {
        if (filled < size)
            filled += file.fread (buf + filled, 1, size - filled);

        memset (buf + filled, 0, AVPROBE_PADDING_SIZE);
        AVProbeData d = {name, buf, filled};
        score = target;

        f = av_probe_input_format2 (& d, true, & score);
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

    if (file.fseek (0, VFS_SEEK_SET) < 0)
        ; /* ignore errors here */

    return f;
}

static AVInputFormat * get_format (const char * name, VFSFile & file)
{
    AVInputFormat * f = get_format_by_extension (name);
    return f ? f : get_format_by_content (name, file);
}

static AVFormatContext * open_input_file (const char * name, VFSFile & file)
{
    AVInputFormat * f = get_format (name, file);

    if (! f)
    {
        AUDERR ("Unknown format for %s.\n", name);
        return nullptr;
    }

    AVFormatContext * c = avformat_alloc_context ();
    AVIOContext * io = io_context_new (file);
    c->pb = io;

    int ret = avformat_open_input (& c, name, f, nullptr);

    if (ret < 0)
    {
        AUDERR ("avformat_open_input failed for %s: %s.\n", name, ffaudio_strerror (ret));
        io_context_free (io);
        return nullptr;
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

static bool find_codec (AVFormatContext * c, CodecInfo * cinfo)
{
    avformat_find_stream_info (c, nullptr);

    for (unsigned i = 0; i < c->nb_streams; i++)
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

                return true;
            }
        }
    }

    return false;
}

static bool ffaudio_probe (const char * filename, VFSFile & file)
{
    return get_format (filename, file) ? true : false;
}

static const struct {
    Tuple::ValueType ttype;  /* Tuple field value type */
    Tuple::Field field;      /* Tuple field constant */
    const char * keys[5];    /* Keys to match (case-insensitive), ended by nullptr */
} metaentries[] = {
    {Tuple::String, Tuple::Artist, {"author", "hor", "artist", nullptr}},
    {Tuple::String, Tuple::Title, {"title", "le", nullptr}},
    {Tuple::String, Tuple::Album, {"album", "WM/AlbumTitle", nullptr}},
    {Tuple::String, Tuple::Performer, {"performer", nullptr}},
    {Tuple::String, Tuple::Copyright, {"copyright", nullptr}},
    {Tuple::String, Tuple::Genre, {"genre", "WM/Genre", nullptr}},
    {Tuple::String, Tuple::Comment, {"comment", nullptr}},
    {Tuple::String, Tuple::Composer, {"composer", nullptr}},
    {Tuple::Int, Tuple::Year, {"year", "WM/Year", "date", nullptr}},
    {Tuple::Int, Tuple::Track, {"track", "WM/TrackNumber", nullptr}},
};

static void read_metadata_dict (Tuple & tuple, AVDictionary * dict)
{
    for (auto & meta : metaentries)
    {
        AVDictionaryEntry * entry = nullptr;

        for (int j = 0; ! entry && meta.keys[j]; j ++)
            entry = av_dict_get (dict, meta.keys[j], nullptr, 0);

        if (entry && entry->value)
        {
            if (meta.ttype == Tuple::String)
                tuple.set_str (meta.field, entry->value);
            else if (meta.ttype == Tuple::Int)
                tuple.set_int (meta.field, atoi (entry->value));
        }
    }
}

static Tuple read_tuple (const char * filename, VFSFile & file)
{
    Tuple tuple;
    AVFormatContext * ic = open_input_file (filename, file);

    if (ic)
    {
        CodecInfo cinfo;

        if (find_codec (ic, & cinfo))
        {
            tuple.set_filename (filename);

            tuple.set_int (Tuple::Length, ic->duration / 1000);
            tuple.set_int (Tuple::Bitrate, ic->bit_rate / 1000);

            if (cinfo.codec->long_name)
                tuple.set_str (Tuple::Codec, cinfo.codec->long_name);

            if (ic->metadata)
                read_metadata_dict (tuple, ic->metadata);
            if (cinfo.stream->metadata)
                read_metadata_dict (tuple, cinfo.stream->metadata);
        }

        close_input_file (ic);
    }

    return tuple;
}

static Tuple
ffaudio_probe_for_tuple(const char *filename, VFSFile &fd)
{
    Tuple t = read_tuple (filename, fd);

    if (t && ! fd.fseek (0, VFS_SEEK_SET))
        audtag::tuple_read (t, fd);

    return t;
}

static bool ffaudio_write_tag (const char * filename, VFSFile & file, const Tuple & tuple)
{
    if (str_has_suffix_nocase (filename, ".ape"))
        return audtag::tuple_write (tuple, file, audtag::TagType::APE);

    return audtag::tuple_write (tuple, file, audtag::TagType::None);
}

static Index<char> ffaudio_read_image (const char * filename, VFSFile & file)
{
    if (str_has_suffix_nocase (filename, ".m4a") || str_has_suffix_nocase (filename, ".mp4"))
        return read_itunes_cover (filename, file);
    
    return Index<char> ();
}

static bool ffaudio_play (const char * filename, VFSFile & file)
{
    AUDDBG ("Playing %s.\n", filename);

    AVPacket pkt = AVPacket();
    int errcount;
    bool codec_opened = false;
    int out_fmt;
    bool planar;
    bool error = false;

    void *buf = nullptr;
    int bufsize = 0;

    AVFormatContext * ic = open_input_file (filename, file);
    if (! ic)
        return false;

    CodecInfo cinfo;

    if (! find_codec (ic, & cinfo))
    {
        AUDERR ("No codec found for %s.\n", filename);
        goto error_exit;
    }

    AUDDBG("got codec %s for stream index %d, opening\n", cinfo.codec->name, cinfo.stream_idx);

    if (avcodec_open2 (cinfo.context, cinfo.codec, nullptr) < 0)
        goto error_exit;

    codec_opened = true;

    switch (cinfo.context->sample_fmt)
    {
        case AV_SAMPLE_FMT_U8: out_fmt = FMT_U8; planar = false; break;
        case AV_SAMPLE_FMT_S16: out_fmt = FMT_S16_NE; planar = false; break;
        case AV_SAMPLE_FMT_S32: out_fmt = FMT_S32_NE; planar = false; break;
        case AV_SAMPLE_FMT_FLT: out_fmt = FMT_FLOAT; planar = false; break;

        case AV_SAMPLE_FMT_U8P: out_fmt = FMT_U8; planar = true; break;
        case AV_SAMPLE_FMT_S16P: out_fmt = FMT_S16_NE; planar = true; break;
        case AV_SAMPLE_FMT_S32P: out_fmt = FMT_S32_NE; planar = true; break;
        case AV_SAMPLE_FMT_FLTP: out_fmt = FMT_FLOAT; planar = true; break;

    default:
        AUDERR ("Unsupported audio format %d\n", (int) cinfo.context->sample_fmt);
        goto error_exit;
    }

    /* Open audio output */
    AUDDBG("opening audio output\n");

    if (aud_input_open_audio(out_fmt, cinfo.context->sample_rate, cinfo.context->channels) <= 0)
    {
        error = true;
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
            if (av_seek_frame (ic, -1, (int64_t) seek_value * AV_TIME_BASE /
             1000, AVSEEK_FLAG_ANY) < 0)
            {
                AUDERR ("error while seeking\n");
            } else
                errcount = 0;

            seek_value = -1;
        }

        AVPacket tmp;
        int ret;

        /* Read next frame (or more) of data */
        if ((ret = av_read_frame(ic, &pkt)) < 0)
        {
            if (ret == (int) AVERROR_EOF)
            {
                AUDDBG("eof reached\n");
                break;
            }
            else
            {
                if (++errcount > 4)
                {
                    AUDERR ("av_read_frame error %d, giving up.\n", ret);
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
                AUDERR ("decode_audio() failed, code %d\n", len);
                break;
            }

            tmp.size -= len;
            tmp.data += len;

            if (! decoded)
                continue;

            int size = FMT_SIZEOF (out_fmt) * cinfo.context->channels * frame->nb_samples;

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
    if (ic != nullptr)
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

static const char *ffaudio_fmts[] = {
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

    /* DTS */
    "dts",

    /* VQF */
    "vqf",

    /* MPEG-4 */
    "m4a", "mp4",

    /* WAV (there are some WAV formats sndfile can't handle) */
    "wav",

    /* Handle OGG streams (FLAC/Vorbis etc.) */
    "ogg", "oga",

    /* Opus */
    "opus",

    /* Speex */
    "spx",

    /* end of table */
    nullptr
};

static const char * const ffaudio_mimes[] = {"application/ogg", nullptr};

#define AUD_PLUGIN_NAME        N_("FFmpeg Plugin")
#define AUD_PLUGIN_ABOUT       ffaudio_about
#define AUD_PLUGIN_INIT        ffaudio_init
#define AUD_PLUGIN_CLEANUP     ffaudio_cleanup
#define AUD_INPUT_EXTS         ffaudio_fmts
#define AUD_INPUT_MIMES        ffaudio_mimes
#define AUD_INPUT_IS_OUR_FILE  ffaudio_probe
#define AUD_INPUT_READ_TUPLE   ffaudio_probe_for_tuple
#define AUD_INPUT_PLAY         ffaudio_play
#define AUD_INPUT_WRITE_TUPLE  ffaudio_write_tag
#define AUD_INPUT_READ_IMAGE   ffaudio_read_image

/* lowest priority fallback */
#define AUD_INPUT_PRIORITY     10

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
