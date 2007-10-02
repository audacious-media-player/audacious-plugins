/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef FLACNG_H
#define FLACNG_H

#include "config.h"
#include <glib.h>
#include <audacious/plugin.h>
#include <audacious/main.h>
#include <audacious/i18n.h>

#define OUTPUT_BLOCK_SIZE (8192u)
#define MAX_SUPPORTED_CHANNELS (2u)
#define BUFFER_SIZE_SAMP (FLAC__MAX_BLOCK_SIZE * FLAC__MAX_CHANNELS)
#define BUFFER_SIZE_BYTE (BUFFER_SIZE_SAMP * (FLAC__MAX_BITS_PER_SAMPLE/8))

/*
 * Audio information about the stream as a whole, gathered from
 * the metadata header
 */
struct stream_info {
    guint bits_per_sample;
    guint samplerate;
    guint channels;
    gulong samples;
    gboolean has_seektable;
};

/*
 * Information about the last decoded audio frame.
 * Also describes the format of the audio currently contained
 * in the stream buffer.
 */
struct frame_info {
    guint bits_per_sample;
    guint samplerate;
    guint channels;
};

/*
 * Information about the stream content, from the metadata
 */
struct stream_comment {
    gchar* artist;
    gchar* album;
    gchar* title;
    gchar* tracknumber;
    gchar* genre;
    gchar* date;
};

/*
 * Replaygain information, from the metadata
 */
struct stream_replaygain {
    gboolean has_rg;
    gchar* ref_loud;
    gchar* track_gain;
    gchar* track_peak;
    gchar* album_gain;
    gchar* album_peak;
};


typedef struct callback_info {
    GMutex* mutex;
    gint32* output_buffer;
    gint32* write_pointer;
    guint buffer_free;
    guint buffer_used;
    VFSFile* input_stream;
    struct stream_info stream;
    struct stream_comment comment;
    struct stream_replaygain replaygain;
    gboolean metadata_changed;
    struct frame_info frame;
    glong read_max;
    gboolean testing;
    gchar* name;
} callback_info;

#endif
