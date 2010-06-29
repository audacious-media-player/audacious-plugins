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

#include <FLAC/all.h>
#include "flac_compat112.h"

#if !defined(FLAC_API_VERSION_CURRENT)

FLAC__SeekableStreamDecoderState FLAC__stream_decoder_init_stream(
        FLAC__SeekableStreamDecoder*  decoder,
        FLAC__SeekableStreamDecoderReadCallback read_callback,
        FLAC__SeekableStreamDecoderSeekCallback     seek_callback,
        FLAC__SeekableStreamDecoderTellCallback     tell_callback,
        FLAC__SeekableStreamDecoderLengthCallback   length_callback,
        FLAC__SeekableStreamDecoderEofCallback      eof_callback,
        FLAC__SeekableStreamDecoderWriteCallback    write_callback,
        FLAC__SeekableStreamDecoderMetadataCallback     metadata_callback,
        FLAC__SeekableStreamDecoderErrorCallback    error_callback,
        void *      client_data) {

        FLAC__seekable_stream_decoder_set_read_callback(decoder, read_callback);
        FLAC__seekable_stream_decoder_set_seek_callback(decoder, seek_callback);
        FLAC__seekable_stream_decoder_set_tell_callback(decoder, tell_callback);
        FLAC__seekable_stream_decoder_set_length_callback(decoder, length_callback);
        FLAC__seekable_stream_decoder_set_eof_callback(decoder, eof_callback);
        FLAC__seekable_stream_decoder_set_write_callback(decoder, write_callback);
        FLAC__seekable_stream_decoder_set_metadata_callback(decoder, metadata_callback);
        FLAC__seekable_stream_decoder_set_error_callback(decoder, error_callback);
        FLAC__seekable_stream_decoder_set_client_data(decoder, client_data);

        return FLAC__seekable_stream_decoder_init(decoder);
}

#endif
