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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef FLACNG_H
#define FLACNG_H

#include "config.h"
#include <glib.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <FLAC/all.h>

#define ERROR(...) do { \
    printf("flacng: " __VA_ARGS__); \
} while (0)

#define OUTPUT_BLOCK_SIZE (1024u)
#define MAX_SUPPORTED_CHANNELS (2u)
#define BUFFER_SIZE_SAMP (FLAC__MAX_BLOCK_SIZE * FLAC__MAX_CHANNELS)
#define BUFFER_SIZE_BYTE (BUFFER_SIZE_SAMP * (FLAC__MAX_BITS_PER_SAMPLE/8))

#define SAMPLE_SIZE(a) (a == 8 ? sizeof(guint8) : (a == 16 ? sizeof(guint16) : sizeof(guint32)))
#define SAMPLE_FMT(a) (a == 8 ? FMT_S8 : (a == 16 ? FMT_S16_NE : (a == 24 ? FMT_S24_NE : FMT_S32_NE)))

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

typedef struct callback_info {
    gint32* output_buffer;
    gint32* write_pointer;
    guint buffer_free;
    guint buffer_used;
    VFSFile* fd;
    struct stream_info stream;
    gboolean metadata_changed;
    struct frame_info frame;
    gint bitrate;
} callback_info;

/* metadata.c */
gboolean flac_update_song_tuple(const Tuple *tuple, VFSFile *fd);
gboolean flac_get_image(const gchar *filename, VFSFile *fd, void **data, gint *length);
Tuple *flac_probe_for_tuple(const gchar *filename, VFSFile *fd);

#endif
