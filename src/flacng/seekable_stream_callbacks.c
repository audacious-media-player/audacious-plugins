/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *  Copyright (C) 2010-2012 Michał Lipski
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

#include <string.h>
#include <FLAC/all.h>

#include <libaudcore/debug.h>

#include "flacng.h"

FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    callback_info* info = (callback_info*) client_data;
    size_t read;

    if (info->fd == NULL)
    {
        FLACNG_ERROR("Trying to read data from an uninitialized file!\n");
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    if (*bytes == 0)
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

    read = vfs_fread(buffer, 1, *bytes, info->fd);
    *bytes = read;

    switch (read)
    {
        case -1:
            FLACNG_ERROR("Error while reading from stream!\n");
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

        case 0:
            AUDDBG("Stream reached EOF\n");
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

        default:
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
}

FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 offset, void *client_data)
{
    callback_info *info = (callback_info*) client_data;

    if (vfs_fseek(info->fd, offset, SEEK_SET) != 0)
    {
        FLACNG_ERROR("Could not seek to %ld!\n", (long)offset);
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }

    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *offset, void *client_data)
{
    callback_info *info = (callback_info*) client_data;

    if ((*offset = vfs_ftell(info->fd)) == -1)
    {
        FLACNG_ERROR("Could not tell current position!\n");
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }

    AUDDBG ("Current position: %d\n", (int) * offset);

    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
    callback_info *info = (callback_info*) client_data;
    return vfs_feof(info->fd);
}

FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *length, void *client_data)
{
    callback_info *info = (callback_info*) client_data;

    if ((*length = vfs_fsize(info->fd)) == -1)
    {
        /*
         * Could not get the stream size. This is not necessarily an
         * error, maybe the stream has no fixed size (think streaming
         * audio)
         */
        AUDDBG("Stream length is unknown.\n");
        *length = 0;
        return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
    }

    AUDDBG ("Stream length is %d bytes\n", (int) * length);

    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
    callback_info *info = (callback_info*) client_data;

    if (info->channels != frame->header.channels ||
        info->sample_rate != frame->header.sample_rate)
    {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    for (long sample = 0; sample < frame->header.blocksize; sample++)
    {
        for (short channel = 0; channel < frame->header.channels; channel++)
        {
            *(info->write_pointer++) = buffer[channel][sample];
            info->buffer_used += 1;
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    FLACNG_ERROR("FLAC decoder error callback was called: %d\n", status);
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
    callback_info *info = (callback_info*) client_data;
    int64_t size;

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
    {
        info->total_samples = metadata->data.stream_info.total_samples;
        AUDDBG("total_total_samples=%ld\n", (long) metadata->data.stream_info.total_samples);

        info->bits_per_sample = metadata->data.stream_info.bits_per_sample;
        AUDDBG("bits_per_sample=%d\n", metadata->data.stream_info.bits_per_sample);

        info->channels = metadata->data.stream_info.channels;
        AUDDBG("channels=%d\n", metadata->data.stream_info.channels);

        info->sample_rate = metadata->data.stream_info.sample_rate;
        AUDDBG("sample_rate=%d\n", metadata->data.stream_info.sample_rate);

        size = vfs_fsize(info->fd);

        if (size == -1 || info->total_samples == 0)
            info->bitrate = 0;
        else
            info->bitrate = 8 * size * (int64_t) info->sample_rate / info->total_samples;

        AUDDBG("bitrate=%d\n", info->bitrate);
    }
}
