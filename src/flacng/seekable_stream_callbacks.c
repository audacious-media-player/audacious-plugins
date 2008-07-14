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

#include <string.h>
#include <FLAC/all.h>
#include <glib.h>
#include "flacng.h"
#include "tools.h"
#include "seekable_stream_callbacks.h"
#include "debug.h"

/* === */

FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {

    callback_info* info;
    gint to_read;
    size_t read;

    _ENTER;

    info = (callback_info*) client_data;
    _DEBUG("Using callback_info %s", info->name);

    if (NULL == info->input_stream) {
        _ERROR("Trying to read data from an uninitialized file!");
        _LEAVE FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    if (0 <= info->read_max) {
        to_read = MIN(*bytes, info->read_max);
        _DEBUG("Reading restricted to %ld bytes", info->read_max);
    } else {
        to_read = *bytes;
    }

    if (0 == to_read) {
        _LEAVE FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }

    read = aud_vfs_fread(buffer, 1, to_read, info->input_stream);

    if ((0 < read) && (0 < info->read_max)) {
        info->read_max -= read;
    }

    _DEBUG("Wanted %d bytes, got %d bytes", *bytes, read);
    *bytes = read;

    switch(read) {
        case -1:
            _ERROR("Error while reading from stream!");
            _LEAVE FLAC__STREAM_DECODER_READ_STATUS_ABORT;
            break;

        case 0:
            _DEBUG("Stream reached EOF");
            _LEAVE FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
            break;

        default:
            _LEAVE FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
}

/* --- */

FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data) {

    callback_info* info;

    _ENTER;

    info = (callback_info*) client_data;
    _DEBUG("Using callback_info %s", info->name);

    _DEBUG("Seeking to %lld", absolute_byte_offset);

    if (0 != aud_vfs_fseek(info->input_stream, absolute_byte_offset, SEEK_SET)) {
        _ERROR("Could not seek to %lld!", (long long)absolute_byte_offset);
        _LEAVE FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }

    _LEAVE FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

/* --- */

FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data) {

    callback_info* info;
    glong position;

    _ENTER;

    info = (callback_info*) client_data;
    _DEBUG("Using callback_info %s", info->name);

    if (-1 == (position = aud_vfs_ftell(info->input_stream))) {
        _ERROR("Could not tell current position!");
        _LEAVE FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }

    _DEBUG("Current position: %ld", position);

    *absolute_byte_offset = position;

    _LEAVE FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

/* --- */

FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data) {

    callback_info* info;
    gboolean eof;

    _ENTER;

    info = (callback_info*) client_data;
    _DEBUG("Using callback_info %s", info->name);

    /*
     * If we are testing a stream and use restricted reading,
     * return EOF if we have exhausted our alotted reading
     * quota
     */
    if (0 == info->read_max) {
        _DEBUG("read_max exhausted, faking EOF");
        _LEAVE TRUE;
    }

    eof = aud_vfs_feof(info->input_stream);

    _LEAVE eof;
}

/* --- */

FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data) {

    callback_info* info;
    off_t size;

    _ENTER;

    info = (callback_info*) client_data;
    _DEBUG("Using callback_info %s", info->name);

    if (-1 == (size = aud_vfs_fsize(info->input_stream))) {
        /*
         * Could not get the stream size. This is not necessarily an
         * error, maybe the stream has no fixed size (think streaming
         * audio)
         */
        _DEBUG("Stream length unknown");
        *stream_length = 0;
        _LEAVE FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
    }

    _DEBUG("Stream length is %ld bytes", size);
    *stream_length = size;

    _LEAVE FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

/* --- */

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data) {

    glong i;
    gshort j;
    callback_info* info;

    _ENTER;

    info = (callback_info*) client_data;
    _DEBUG("Using callback_info %s", info->name);

    _DEBUG("Frame decoded: %d samples per channel, %d channels, %d bps",
            frame->header.blocksize, frame->header.channels, frame->header.bits_per_sample);

    /*
     * Check if there is more data decoded than we have space
     * for. This _should_ not happen given how our buffer is sized,
     * but you never know.
     */
    if (info->buffer_free < (frame->header.blocksize * frame->header.channels)) {
        _ERROR("BUG! Too much data decoded from stream!");
        _LEAVE FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    if ((frame->header.bits_per_sample != 8) &&
        (frame->header.bits_per_sample != 16) &&
        (frame->header.bits_per_sample != 24) &&
        (frame->header.bits_per_sample != 32)) {
        _ERROR("Unsupported bitrate found in stream: %d!", frame->header.bits_per_sample);
        _LEAVE FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    /*
     * Copy the frame metadata, will be compared to stream
     * metadata later
     * This also describes the format of the current buffer content.
     */
    info->frame.channels = frame->header.channels;
    info->frame.samplerate = frame->header.sample_rate;
    info->frame.bits_per_sample = frame->header.bits_per_sample;

    for (i=0; i < frame->header.blocksize; i++) {
        for (j=0; j < frame->header.channels; j++) {
            *(info->write_pointer++) = buffer[j][i];
            info->buffer_free -= 1;
            info->buffer_used += 1;
        }
    }

    _DEBUG("free space in buffer after copying: %d samples", info->buffer_free);

    _LEAVE FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/* --- */

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {

    callback_info* info = (callback_info*) client_data;

    _ENTER;

    _DEBUG("Using callback_info %s", info->name);

    if (!info->testing) {
        _ERROR("FLAC decoder error callback was called: %d", status);
    } else {
        _DEBUG("FLAC decoder error callback was called: %d", status);
    }

    _LEAVE;

}

/* --- */

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {

    callback_info* info;
    gint i;
    FLAC__StreamMetadata_VorbisComment_Entry* entry;
    FLAC__StreamMetadata* metadata_copy;
    gchar* key;
    gchar* value;
    int artist_offset;

    _ENTER;

    info = (callback_info*) client_data;
    _DEBUG("Using callback_info %s", info->name);

    /*
     * We have found a metadata block. Enable unrestricted reading
     */
    info->read_max = -1;

    if (FLAC__METADATA_TYPE_STREAMINFO == metadata->type) {
        /*
         * Basic stream information. Sample rate, channels and stuff
         */
        _DEBUG("FLAC__METADATA_TYPE_STREAMINFO found");

        info->stream.samples = metadata->data.stream_info.total_samples;
        _DEBUG("total_samples=%lld", metadata->data.stream_info.total_samples);
        info->stream.bits_per_sample = metadata->data.stream_info.bits_per_sample;
        _DEBUG("bits_per_sample=%d", metadata->data.stream_info.bits_per_sample);
        info->stream.channels = metadata->data.stream_info.channels;
        _DEBUG("channels=%d", metadata->data.stream_info.channels);
        info->stream.samplerate = metadata->data.stream_info.sample_rate;
        _DEBUG("sample_rate=%d", metadata->data.stream_info.sample_rate);

        info->metadata_changed = TRUE;
    }

    if (FLAC__METADATA_TYPE_VORBIS_COMMENT == metadata->type) {
        /*
         * We will possibly need to modify some of the entries
         * in the metadata field, so we make a copy of it
         * first.
         * The original structure must not be modified.
         */
        metadata_copy = FLAC__metadata_object_clone(metadata);

        /*
         * A vorbis type comment field.
         */
        _DEBUG("FLAC__METADATA_TYPE_VORBIS_COMMENT found");
        _DEBUG("Vorbis comment contains %d fields", metadata_copy->data.vorbis_comment.num_comments);
        _DEBUG("Vendor string: %s", metadata_copy->data.vorbis_comment.vendor_string.entry);

        /*
         * Find an ARTIST field
         */
        if (0 <= (artist_offset = FLAC__metadata_object_vorbiscomment_find_entry_from(metadata_copy, 0, "ARTIST"))) {
            _DEBUG("ARTIST field found @ %d: %s", artist_offset,
                    metadata_copy->data.vorbis_comment.comments[artist_offset].entry);
        }


        /*
         * Enumerate the comment entries
         */
        entry = metadata_copy->data.vorbis_comment.comments;
        for (i=0; i < metadata_copy->data.vorbis_comment.num_comments; i++,entry++) {
            _DEBUG("Comment[%d]: %s", i, entry->entry);

            /*
             * Try and parse the comment.
             * If FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair() succeeds,
             * it allocates memory for the key and value which we have to take
             * care of.
             */
            if (false == FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(*entry, &key, &value)) {
                _DEBUG("Could not parse comment");
            } else {
                _DEBUG("Key: <%s>, Value <%s>", key, value);
                add_comment(info, key, value);
                free(key);
                free(value);
            }
        }

        /*
         * Free our metadata copy
         */
        FLAC__metadata_object_delete(metadata_copy);

        info->metadata_changed = TRUE;
    }

    if (FLAC__METADATA_TYPE_SEEKTABLE == metadata->type) {
        /*
         * We have found a seektable, which means that we can seek
         * without telling FLAC the length of the file (which we can not
         * do, since Audacious lacks the functions for that)
         */
        _DEBUG("FLAC__METADATA_TYPE_SEEKTABLE found");
        info->stream.has_seektable = TRUE;
    }

    _LEAVE;
}
