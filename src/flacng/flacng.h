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

#include <FLAC/all.h>

#include <libaudcore/audio.h>
#include <libaudcore/tuple.h>
#include <libaudcore/vfs.h>

#define FLACNG_ERROR(...) do { \
    printf("flacng: " __VA_ARGS__); \
} while (0)

#define OUTPUT_BLOCK_SIZE (1024u)
#define MAX_SUPPORTED_CHANNELS (2u)
#define BUFFER_SIZE_SAMP (FLAC__MAX_BLOCK_SIZE * FLAC__MAX_CHANNELS)
#define BUFFER_SIZE_BYTE (BUFFER_SIZE_SAMP * (FLAC__MAX_BITS_PER_SAMPLE/8))

#define SAMPLE_SIZE(a) (a == 8 ? 1 : (a == 16 ? 2 : 4))
#define SAMPLE_FMT(a) (a == 8 ? FMT_S8 : (a == 16 ? FMT_S16_NE : (a == 24 ? FMT_S24_NE : FMT_S32_NE)))

/*
 * Audio information about the stream as a whole, gathered from
 * the metadata header
 */
struct stream_info {
    unsigned bits_per_sample;
    unsigned samplerate;
    unsigned channels;
    unsigned long samples;
    bool_t has_seektable;
};

/*
 * Information about the last decoded audio frame.
 * Also describes the format of the audio currently contained
 * in the stream buffer.
 */
struct frame_info {
    unsigned bits_per_sample;
    unsigned samplerate;
    unsigned channels;
};

typedef struct callback_info {
    int32_t* output_buffer;
    int32_t* write_pointer;
    unsigned buffer_free;
    unsigned buffer_used;
    VFSFile* fd;
    struct stream_info stream;
    bool_t metadata_changed;
    struct frame_info frame;
    int bitrate;
} callback_info;

/* metadata.c */
bool_t flac_update_song_tuple(const Tuple *tuple, VFSFile *fd);
bool_t flac_get_image(const char *filename, VFSFile *fd, void **data, int64_t *length);
Tuple *flac_probe_for_tuple(const char *filename, VFSFile *fd);

/* seekable_stream_callbacks.c */
FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data);
FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);

/* tools.c */
callback_info* init_callback_info(void);
void clean_callback_info(callback_info* info);
void reset_info(callback_info* info);
bool_t read_metadata(FLAC__StreamDecoder* decoder, callback_info* info);

#endif
