/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
 *                  Matti Hämäläinen <ccr@tnsp.org>
 * Copyright © 2011-2016 John Lindgren <john.lindgren@aol.com>
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

#undef FFAUDIO_DOUBLECHECK  /* Doublecheck probing result for debugging purposes */
#undef FFAUDIO_NO_BLACKLIST /* Don't blacklist any recognized codecs/formats */

#include "ffaudio-stdinc.h"

#include <limits.h>
#include <pthread.h>

#include <audacious/audtag.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>

#if CHECK_LIBAVFORMAT_VERSION (57, 33, 100)
#define ALLOC_CONTEXT 1
#endif

#if CHECK_LIBAVCODEC_VERSION (57, 37, 100)
#define SEND_PACKET 1
#endif

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

    constexpr FFaudio () : InputPlugin (info, InputInfo (FlagWritesTag)
        .with_priority (10) /* lowest priority fallback */
        .with_exts (exts)
        .with_mimes (mimes)) {}

    bool init ();
    void cleanup ();

    bool is_our_file (const char * filename, VFSFile & file);
    bool read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool write_tuple (const char * filename, VFSFile & file, const Tuple & tuple);
    bool play (const char * filename, VFSFile & file);
};

EXPORT FFaudio aud_plugin_instance;

typedef struct
{
    int stream_idx;
    AVStream * stream;
    AVCodec * codec;
}
CodecInfo;

struct ScopedContext
{
    AVCodecContext * ptr;
    AVCodecContext * operator-> () { return ptr; }

    ScopedContext (const CodecInfo & cinfo)
    {
#ifdef ALLOC_CONTEXT
        ptr = avcodec_alloc_context3 (cinfo.codec);
        avcodec_parameters_to_context (ptr, cinfo.stream->codecpar);
#else
        ptr = cinfo.stream->codec;
#endif
    }

#ifdef ALLOC_CONTEXT
    ~ScopedContext () { avcodec_free_context (& ptr); }
#else
    ~ScopedContext () { avcodec_close (ptr); }
#endif
};

struct ScopedPacket
{
    AVPacket * ptr;
    AVPacket * operator-> () { return ptr; }

#if CHECK_LIBAVCODEC_VERSION(57, 12, 100)
    ScopedPacket () { ptr = av_packet_alloc (); }
    ~ScopedPacket () { av_packet_free (& ptr); }

    void clear ()
    {
        av_packet_free (& ptr);
        ptr = av_packet_alloc ();
    }
#else
    ScopedPacket ()
    {
        ptr = new AVPacket ();
        av_init_packet (ptr);
    }

    ~ScopedPacket ()
    {
        av_packet_unref (ptr);
        delete ptr;
    }

    void clear ()
    {
        av_packet_unref (ptr);
        * ptr = AVPacket ();
        av_init_packet (ptr);
    }
#endif
};

struct ScopedFrame
{
    AVFrame * ptr = av_frame_alloc ();
    AVFrame * operator-> () { return ptr; }
    ~ScopedFrame () { av_frame_free (& ptr); }
};

static SimpleHash<String, AVInputFormat *> extension_dict;

static void create_extension_dict ();

#if ! CHECK_LIBAVCODEC_VERSION(58, 9, 100)
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
#endif

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
#if ! CHECK_LIBAVFORMAT_VERSION(58, 9, 100)
    av_register_all();
#endif
#if ! CHECK_LIBAVCODEC_VERSION(58, 9, 100)
    av_lockmgr_register (lockmgr);
#endif

    create_extension_dict ();

    av_log_set_callback (ffaudio_log_cb);

    return true;
}

void FFaudio::cleanup ()
{
    extension_dict.clear ();

#if ! CHECK_LIBAVCODEC_VERSION(58, 9, 100)
    av_lockmgr_register (nullptr);
#endif
}

static int log_result (const char * func, int ret)
{
    if (ret < 0 && ret != (int) AVERROR_EOF && ret != AVERROR (EAGAIN))
    {
        static char buf[256];
        if (! av_strerror (ret, buf, sizeof buf))
            AUDERR ("%s failed: %s\n", func, buf);
        else
            AUDERR ("%s failed\n", func);
    }

    return ret;
}

#define LOG(function, ...) log_result (#function, function (__VA_ARGS__))

static void create_extension_dict ()
{
    AVInputFormat * f;
#if CHECK_LIBAVFORMAT_VERSION(58, 9, 100)
    void * iter = nullptr;
    while ((f = const_cast<AVInputFormat *> (av_demuxer_iterate (& iter))))
#else
    for (f = av_iformat_next (nullptr); f; f = av_iformat_next (f))
#endif
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

    AVInputFormat * * f = extension_dict.lookup (String (str_tolower (ext)));

    if (f && * f)
        AUDINFO ("Matched format %s by extension.\n", (* f)->name);
    else
        AUDINFO ("No format matched by extension.\n");

    return f ? * f : nullptr;
}

static AVInputFormat * get_format_by_content (const char * name, VFSFile & file)
{
    AUDDBG ("Probing content: %s\n", name);

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

        f = (AVInputFormat *) av_probe_input_format2 (& d, true, & score);
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
        AUDINFO ("Probe matched format %s, buffer size %d, score %d.\n", f->name, filled, score);
    else
        AUDINFO ("Probe did not match any known formats.\n");

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

    if (LOG (avformat_open_input, & c, name, f, nullptr) < 0)
    {
        io_context_free (io);
        return nullptr;
    }

    return c;
}

static void close_input_file (AVFormatContext * c)
{
    AVIOContext * io = c->pb;

    avformat_close_input (&c);
    io_context_free (io);
}

static bool find_codec (AVFormatContext * c, CodecInfo * cinfo)
{
    avformat_find_stream_info (c, nullptr);

    for (unsigned i = 0; i < c->nb_streams; i++)
    {
        AVStream * stream = c->streams[i];

#ifndef ALLOC_CONTEXT
#define codecpar codec
#endif
        if (stream && stream->codecpar && stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            AVCodec * codec = (AVCodec *) avcodec_find_decoder (stream->codecpar->codec_id);

            if (codec)
            {
                cinfo->stream_idx = i;
                cinfo->stream = stream;
                cinfo->codec = codec;

                return true;
            }
        }
#undef codecpar
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
    {Tuple::String, Tuple::AlbumArtist, {"album_artist", nullptr}},
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

bool FFaudio::read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image)
{
    SmartPtr<AVFormatContext, close_input_file>
     ic (open_input_file (filename, file));

    if (! ic)
        return false;

    CodecInfo cinfo;
    if (! find_codec (ic.get (), & cinfo))
        return false;

    if (ic->duration > 0 && ic->duration / 1000 <= INT_MAX)
        tuple.set_int (Tuple::Length, ic->duration / 1000);
    if (ic->bit_rate > 0 && ic->bit_rate / 1000 <= INT_MAX)
        tuple.set_int (Tuple::Bitrate, ic->bit_rate / 1000);

    if (cinfo.codec->long_name)
        tuple.set_str (Tuple::Codec, cinfo.codec->long_name);

    if (ic->metadata)
        read_metadata_dict (tuple, ic->metadata);
    if (cinfo.stream->metadata)
        read_metadata_dict (tuple, cinfo.stream->metadata);

    if (! file.fseek (0, VFS_SEEK_SET))
        audtag::read_tag (file, tuple, image);

    if (image && ! image->len ())
    {
        for (unsigned i = 0; i < ic->nb_streams; i ++)
        {
            if (ic->streams[i]->attached_pic.size > 0)
            {
                image->insert ((char *) ic->streams[i]->attached_pic.data, 0,
                 ic->streams[i]->attached_pic.size);
                break;
            }
        }
    }

    return true;
}

bool FFaudio::write_tuple (const char * filename, VFSFile & file, const Tuple & tuple)
{
    if (str_has_suffix_nocase (filename, ".ape"))
        return audtag::write_tuple (file, tuple, audtag::TagType::APE);

    return audtag::write_tuple (file, tuple, audtag::TagType::None);
}

static bool convert_format (int ff_fmt, int & aud_fmt, bool & planar)
{
    switch (ff_fmt)
    {
        case AV_SAMPLE_FMT_U8: aud_fmt = FMT_U8; planar = false; break;
        case AV_SAMPLE_FMT_S16: aud_fmt = FMT_S16_NE; planar = false; break;
        case AV_SAMPLE_FMT_S32: aud_fmt = FMT_S32_NE; planar = false; break;
        case AV_SAMPLE_FMT_FLT: aud_fmt = FMT_FLOAT; planar = false; break;

        case AV_SAMPLE_FMT_U8P: aud_fmt = FMT_U8; planar = true; break;
        case AV_SAMPLE_FMT_S16P: aud_fmt = FMT_S16_NE; planar = true; break;
        case AV_SAMPLE_FMT_S32P: aud_fmt = FMT_S32_NE; planar = true; break;
        case AV_SAMPLE_FMT_FLTP: aud_fmt = FMT_FLOAT; planar = true; break;

    default:
        AUDERR ("Unsupported audio format %d\n", (int) ff_fmt);
        return false;
    }

    return true;
}

bool FFaudio::play (const char * filename, VFSFile & file)
{
    SmartPtr<AVFormatContext, close_input_file>
     ic (open_input_file (filename, file));

    if (! ic)
        return false;

    CodecInfo cinfo;
    if (! find_codec (ic.get (), & cinfo))
    {
        AUDERR ("No codec found for %s.\n", filename);
        return false;
    }

    AUDDBG("got codec %s for stream index %d, opening\n", cinfo.codec->name, cinfo.stream_idx);

    ScopedContext context (cinfo);
    if (LOG (avcodec_open2, context.ptr, cinfo.codec, nullptr) < 0)
        return false;

    int out_fmt; bool planar;
    if (! convert_format (context->sample_fmt, out_fmt, planar))
        return false;

#if CHECK_LIBAVCODEC_VERSION(59, 37, 100)
    int channels = context->ch_layout.nb_channels;
#else
    int channels = context->channels;
#endif

    /* Open audio output */
    set_stream_bitrate(ic->bit_rate);
    open_audio(out_fmt, context->sample_rate, channels);

    int errcount = 0;
    bool eof = false;

    Index<char> buf;

    while (! eof && ! check_stop ())
    {
        int seek_value = check_seek ();

        if (seek_value >= 0)
        {
            if (LOG (av_seek_frame, ic.get (), -1, (int64_t) seek_value *
             AV_TIME_BASE / 1000, AVSEEK_FLAG_ANY) >= 0)
                errcount = 0;

            seek_value = -1;
        }

        /* Read next frame (or more) of data */
        ScopedPacket pkt;
        int ret = LOG (av_read_frame, ic.get (), pkt.ptr);

        if (ret < 0)
        {
            if (ret == (int) AVERROR_EOF)
            {
                /* On EOF, send an empty packet to "flush" the decoder */
                pkt.clear ();
                eof = true;
            }
            else if (++ errcount > 4)
                return false;
            else
                continue;
        }
        else
        {
            errcount = 0;

            /* Ignore any other substreams */
            if (pkt->stream_index != cinfo.stream_idx)
                continue;
        }

        /* Decode and play packet/frame */
#ifdef SEND_PACKET
        if ((ret = LOG (avcodec_send_packet, context.ptr, pkt.ptr)) < 0)
            return false; /* defensive, errors not expected here */
#else
        /* Make a mutable (shallow) copy of the real packet */
        AVPacket tmp = * pkt.ptr;
#endif

        while (! check_stop ())
        {
            ScopedFrame frame;

#ifdef SEND_PACKET
            if ((ret = LOG (avcodec_receive_frame, context.ptr, frame.ptr)) < 0)
                break; /* read next packet (continue past errors) */
#else
            int decoded = 0;
            int len = LOG (avcodec_decode_audio4, context.ptr, frame.ptr, & decoded, & tmp);

            if (len < 0)
                break; /* read next packet (continue past errors) */

            tmp.size -= len;
            tmp.data += len;

            if (! decoded)
            {
                if (tmp.size > 0)
                    continue; /* process more of current packet */

                break; /* read next packet */
            }
#endif

            int size = FMT_SIZEOF (out_fmt) * channels * frame->nb_samples;

            if (planar)
            {
                if (size > buf.len ())
                    buf.resize (size);

                audio_interlace ((const void * *) frame->data, out_fmt,
                 channels, buf.begin (), frame->nb_samples);
                write_audio (buf.begin (), size);
            }
            else
                write_audio (frame->data[0], size);
        }
    }

    return true;
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
    "m4a", "m4v", "mp4",

    /* WAV (there are some WAV formats sndfile can't handle) */
    "wav",

    /* Handle OGG streams (FLAC/Vorbis etc.) */
    "ogg", "oga",

    /* Opus */
    "opus",

    /* Speex */
    "spx",

    /* True Audio */
    "tta",

    /* WebM */
    "webm",

    /* Matroska */
    "mka", "mkv",

    /* end of table */
    nullptr
};

const char * const FFaudio::mimes[] = {
    "application/ogg",
    "audio/mp4",
    "audio/ogg",
    nullptr
};
