#ifndef FLAC_COMPAT112_H
#define FLAC_COMPAT112_H

#if !defined(FLAC_API_VERSION_CURRENT)

#define FLAC__StreamDecoder FLAC__SeekableStreamDecoder

#define FLAC__StreamDecoderReadStatus FLAC__SeekableStreamDecoderReadStatus 
#define FLAC__StreamDecoderSeekStatus FLAC__SeekableStreamDecoderSeekStatus
#define FLAC__StreamDecoderTellStatus FLAC__SeekableStreamDecoderTellStatus
#define FLAC__StreamDecoderLengthStatus FLAC__SeekableStreamDecoderLengthStatus
#define FLAC__stream_decoder_new FLAC__seekable_stream_decoder_new

#define FLAC__STREAM_DECODER_SEEK_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR
#define FLAC__STREAM_DECODER_SEEK_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK
#define FLAC__STREAM_DECODER_TELL_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR
#define FLAC__STREAM_DECODER_TELL_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK
#define FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR
#define FLAC__STREAM_DECODER_LENGTH_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK

FLAC__stream_decoder_init_stream(
    FLAC__SeekableStreamDecoder*  decoder,
    FLAC__SeekableStreamDecoderReadCallback read_callback,
    FLAC__SeekableStreamDecoderSeekCallback     seek_callback,
    FLAC__SeekableStreamDecoderTellCallback     tell_callback,
    FLAC__SeekableStreamDecoderLengthCallback   length_callback,
    FLAC__SeekableStreamDecoderEofCallback      eof_callback,
    FLAC__SeekableStreamDecoderWriteCallback    write_callback,
    FLAC__SeekableStreamDecoderMetadataCallback     metadata_callback,
    FLAC__SeekableStreamDecoderErrorCallback    error_callback,
    void *      client_data);

#endif

#endif
