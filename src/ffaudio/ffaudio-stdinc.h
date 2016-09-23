/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
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

#ifndef __FFAUDIO_STDINC_H__GUARD
#define __FFAUDIO_STDINC_H__GUARD

#define __STDC_CONSTANT_MACROS
#include <libaudcore/plugin.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

/* FFmpeg and libav remain mostly compatible but have different version numbers.
 * The first three numbers are the required FFmpeg version; the last three are for libav. */
#ifdef HAVE_FFMPEG
#define CHECK_LIBAVCODEC_VERSION(a, b, c, a2, b2, c2) (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT (a, b, c))
#define CHECK_LIBAVFORMAT_VERSION(a, b, c, a2, b2, c2) (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT (a, b, c))
#define CHECK_LIBAVUTIL_VERSION(a, b, c, a2, b2, c2) (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT (a, b, c))
#elif defined HAVE_LIBAV
#define CHECK_LIBAVCODEC_VERSION(a, b, c, a2, b2, c2) (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT (a2, b2, c2))
#define CHECK_LIBAVFORMAT_VERSION(a, b, c, a2, b2, c2) (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT (a2, b2, c2))
#define CHECK_LIBAVUTIL_VERSION(a, b, c, a2, b2, c2) (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT (a2, b2, c2))
#else
#error Please define either HAVE_FFMPEG or HAVE_LIBAV
#endif

AVIOContext * io_context_new (VFSFile & file);
void io_context_free (AVIOContext * context);

#endif
