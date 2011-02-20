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

#ifndef __FFAUDIO_STDINC_H__GUARD
#define __FFAUDIO_STDINC_H__GUARD

#define _ERROR(...) printf ("ffaudio: " __VA_ARGS__)

#include <audacious/plugin.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

#define VERSION_INT(maj, min, mic) ((maj) * 1000000 + (min) * 1000 + (mic))
#define CHECK_LIBAVCODEC_VERSION(maj, min, mic) (VERSION_INT \
 (LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO) \
 >= VERSION_INT (maj, min, mic))
#define CHECK_LIBAVFORMAT_VERSION(maj, min, mic) (VERSION_INT \
 (LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, \
 LIBAVFORMAT_VERSION_MICRO) >= VERSION_INT (maj, min, mic))

URLProtocol audvfsptr_protocol;

#endif
