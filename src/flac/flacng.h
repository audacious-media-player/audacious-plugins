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

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

class FLACng : public InputPlugin
{
public:
    static const char about[];
    static const char *const exts[];

    static constexpr PluginInfo info = {
        N_("FLAC Decoder"),
        PACKAGE,
        about
    };

    constexpr FLACng() : InputPlugin(info, InputInfo(FlagWritesTag)
        .with_priority(1)
        .with_exts(exts)) {}

    bool init();
    void cleanup();

    bool is_our_file(const char *filename, VFSFile &file);
    bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image);
    bool write_tuple(const char *filename, VFSFile &file, const Tuple &tuple);
    bool play(const char *filename, VFSFile &file);
};

#define BUFFER_SIZE_SAMP (FLAC__MAX_BLOCK_SIZE * FLAC__MAX_CHANNELS)
#define BUFFER_SIZE_BYTE (BUFFER_SIZE_SAMP * (FLAC__MAX_BITS_PER_SAMPLE/8))

#define SAMPLE_SIZE(a) (a == 8 ? 1 : (a == 16 ? 2 : 4))
#define SAMPLE_FMT(a) (a == 8 ? FMT_S8 : (a == 16 ? FMT_S16_NE : (a == 24 ? FMT_S24_NE : FMT_S32_NE)))

struct callback_info
{
    unsigned bits_per_sample = 0;
    unsigned sample_rate = 0;
    unsigned channels = 0;
    unsigned long total_samples = 0;
    Index<int32_t> output_buffer;
    int32_t *write_pointer = nullptr;
    unsigned buffer_used = 0;
    VFSFile *fd = nullptr;
    int bitrate = 0;

    void alloc()
    {
        output_buffer.resize(BUFFER_SIZE_SAMP);
        reset();
    }

    void reset()
    {
        buffer_used = 0;
        write_pointer = output_buffer.begin();
    }
};

/* metadata.c */
bool flac_update_song_tuple(const char *filename, VFSFile &fd, const Tuple &tuple);
Index<char> flac_get_image(const char *filename, VFSFile &fd);
Tuple flac_probe_for_tuple(const char *filename, VFSFile &fd);

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
bool read_metadata(FLAC__StreamDecoder* decoder, callback_info* info);

#endif
