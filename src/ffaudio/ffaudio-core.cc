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

#include <pthread.h>

#undef FFAUDIO_DOUBLECHECK  /* Doublecheck probing result for debugging purposes */
#undef FFAUDIO_NO_BLACKLIST /* Don't blacklist any recognized codecs/formats */

#include "ffaudio-stdinc.h"

#include <audacious/audtag.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>

class FFaudio : public InputPlugin
{
public:
    static const char about[];
    static const char * const exts[], * const mimes[];

    static constexpr PluginInfo info = {
        N_("FFmpeg Plugin"),
        PACKAGE,
        about
    };

    static constexpr auto iinfo = InputInfo (FlagWritesTag)
        .with_priority (10) /* lowest priority fallback */
        .with_exts (exts)
        .with_mimes (mimes);

    constexpr FFaudio () : InputPlugin (info, iinfo) {}

    bool init ();
    void cleanup ();

    bool is_our_file (const char * filename, VFSFile & file);
    Tuple read_tuple (const char * filename, VFSFile & file);
    Index<char> read_image (const char * filename, VFSFile & file);
    bool write_tuple (const char * filename, VFSFile & file, const Tuple & tuple);
    bool play (const char * filename, VFSFile & file);
};

EXPORT FFaudio aud_plugin_instance;

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
        * mutexp = new pthread_mutex_t;
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
        delete (pthread_mutex_t *) * mutexp;
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

bool FFaudio::init ()
{
    av_register_all();
    av_lockmgr_register (lockmgr);

    create_extension_dict ();

    av_log_set_callback (ffaudio_log_cb);

    return true;
}

void FFaudio::cleanup ()
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

        StringBuf exts = str_tolower (f->extensions);
        Index<String> extlist = str_list_to_index (exts, ",");

        for (auto & ext : extlist)
            extension_dict.add (ext, std::move (f));
    }
}

static AVInputFormat * get_format_by_extension (const char * name)
{
    StringBuf ext = uri_get_extension (name);
    if (! ext)
        return nullptr;

    AUDDBG ("Get format by extension: %s\n", name);
    AVInputFormat * * f = extension_dict.lookup (String (str_tolower (ext)));

    if (f && * f)
        AUDDBG ("Format %s.\n", (* f)->name);
    else
        AUDDBG ("Format unknown.\n");

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

bool FFaudio::is_our_file (const char * filename, VFSFile & file)
{
    return (bool) get_format (filename, file);
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

Tuple FFaudio::read_tuple (const char * filename, VFSFile & file)
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

    if (tuple && ! file.fseek (0, VFS_SEEK_SET))
        audtag::tuple_read (tuple, file);

    return tuple;
}

bool FFaudio::write_tuple (const char * filename, VFSFile & file, const Tuple & tuple)
{
    if (str_has_suffix_nocase (filename, ".ape"))
        return audtag::tuple_write (tuple, file, audtag::TagType::APE);

    return audtag::tuple_write (tuple, file, audtag::TagType::None);
}

Index<char> FFaudio::read_image (const char * filename, VFSFile & file)
{
    if (str_has_suffix_nocase (filename, ".m4a") || str_has_suffix_nocase (filename, ".mp4"))
        return read_itunes_cover (filename, file);

    return Index<char> ();
}

bool FFaudio::play (const char * filename, VFSFile & file)
{
    AUDDBG ("Playing %s.\n", filename);

    AVPacket pkt = AVPacket();
    int errcount;
    bool codec_opened = false;
    int out_fmt;
    bool planar;
    bool error = false;

    Index<char> buf;

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

    set_stream_bitrate(ic->bit_rate);
    open_audio(out_fmt, cinfo.context->sample_rate, cinfo.context->channels);

    errcount = 0;

    while (! check_stop ())
    {
        int seek_value = check_seek ();

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
        while (tmp.size > 0 && ! check_stop ())
        {
            /* Check for seek request and bail out if we have one */
            if (seek_value < 0)
                seek_value = check_seek ();

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
                if (size > buf.len ())
                    buf.resize (size);

                audio_interlace ((const void * *) frame->data, out_fmt,
                 cinfo.context->channels, buf.begin (), frame->nb_samples);
                write_audio (buf.begin (), size);
            }
            else
                write_audio (frame->data[0], size);

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

    return ! error;
}

const char FFaudio::about[] =
 N_("Multi-format audio decoding plugin for Audacious using\n"
    "FFmpeg multimedia framework (http://www.ffmpeg.org/)\n"
    "\n"
    "Audacious plugin by:\n"
    "William Pitcock <nenolod@nenolod.net>\n"
    "Matti Hämäläinen <ccr@tnsp.org>");

const char * const FFaudio::exts[] = {
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

const char * const FFaudio::mimes[] = {"application/ogg", nullptr};
