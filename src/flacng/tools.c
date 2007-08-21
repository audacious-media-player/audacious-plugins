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

#include <string.h>
#include <strings.h>
#include "tools.h"
#include "debug.h"

/* === */

callback_info* init_callback_info(gchar* name) {

    callback_info* info;

    _ENTER;

    if (NULL == name) {
        _ERROR("Can not allocate callback structure without a name");
        _LEAVE NULL;
    }

    if (NULL == (info = malloc(sizeof(callback_info)))) {
        _ERROR("Could not allocate memory for callback structure!");
        _LEAVE NULL;
    }

    if (NULL == (info->output_buffer = malloc(BUFFER_SIZE_BYTE))) {
        _ERROR("Could not allocate memory for output buffer!");
        _LEAVE NULL;
    }

    /*
     * We need to set this manually to NULL because
     * reset_info will try to close the file pointed to
     * by input_stream if it is non-NULL.
     * Same for info->comment.x
     */
    info->name = name;
    info->input_stream = NULL;
    info->comment.artist = NULL;
    info->comment.album = NULL;
    info->comment.title = NULL;
    info->comment.tracknumber = NULL;
    info->comment.genre = NULL;
    info->comment.date = NULL;
    info->replaygain.ref_loud = NULL;
    info->replaygain.track_gain = NULL;
    info->replaygain.track_peak = NULL;
    info->replaygain.album_gain = NULL;
    info->replaygain.album_peak = NULL;
    reset_info(info, FALSE);

    info->mutex = g_mutex_new();

    _DEBUG("Playback buffer allocated for %d samples, %d bytes", BUFFER_SIZE_SAMP, BUFFER_SIZE_BYTE);

    _LEAVE info;
}

/* --- */

void reset_info(callback_info* info, gboolean close_fd) {

    _ENTER;

    _DEBUG("Using callback_info %s", info->name);

    if (close_fd && (NULL != info->input_stream)) {
        _DEBUG("Closing fd");
        vfs_fclose(info->input_stream);
    }
    info->input_stream = NULL;

    // memset(info->output_buffer, 0, BUFFER_SIZE * sizeof(int16_t));
    info->stream.samplerate = 0;
    info->stream.bits_per_sample = 0;
    info->stream.channels = 0;
    info->stream.samples = 0;
    info->stream.has_seektable = FALSE;
    info->buffer_free = BUFFER_SIZE_SAMP;
    info->buffer_used = 0;
    info->write_pointer = info->output_buffer;
    info->read_max = -1;
    info->testing = FALSE;

    /*
     * Clear the stream and frame information
     */
    info->stream.bits_per_sample = 0;
    info->stream.samplerate = 0;
    info->stream.channels = 0;
    info->stream.samples = 0;

    if (NULL != info->comment.artist) {
        free(info->comment.artist);
        info->comment.artist = NULL;
    }

    if (NULL != info->comment.album) {
        free(info->comment.album);
        info->comment.album = NULL;
    }

    if (NULL != info->comment.title) {
        free(info->comment.title);
        info->comment.title = NULL;
    }

    if (NULL != info->comment.tracknumber) {
        free(info->comment.tracknumber);
        info->comment.tracknumber = NULL;
    }

    if (NULL != info->comment.genre) {
        free(info->comment.genre);
        info->comment.genre = NULL;
    }

    if (NULL != info->replaygain.ref_loud) {
        free(info->replaygain.ref_loud);
        info->replaygain.ref_loud = NULL;
    }

    if (NULL != info->replaygain.track_gain) {
        free(info->replaygain.track_gain);
        info->replaygain.track_gain = NULL;
    }

    if (NULL != info->replaygain.track_peak) {
        free(info->replaygain.track_peak);
        info->replaygain.track_peak = NULL;
    }

    if (NULL != info->replaygain.album_gain) {
        free(info->replaygain.album_gain);
        info->replaygain.album_gain = NULL;
    }

    if (NULL != info->replaygain.album_peak) {
        free(info->replaygain.album_peak);
        info->replaygain.album_peak = NULL;
    }

    info->replaygain.has_rg = FALSE;

    if (NULL != info->comment.date) {
        free(info->comment.date);
        info->comment.date = NULL;
    }

    info->frame.bits_per_sample = 0;
    info->frame.samplerate = 0;
    info->frame.channels = 0;

    info->metadata_changed = FALSE;

    _LEAVE;
}

/* --- */

gboolean read_metadata(VFSFile* fd, FLAC__StreamDecoder* decoder, callback_info* info) {

    FLAC__StreamDecoderState ret;

    _ENTER;

    _DEBUG("Using callback_info %s", info->name);

    reset_info(info, FALSE);

    info->input_stream = fd;

    /*
     * Reset the decoder
     */
    if (false == FLAC__stream_decoder_reset(decoder)) {
        _ERROR("Could not reset the decoder!");
        _LEAVE FALSE;
    }

    /*
     * Just scan the first 8k for the start of metadata
     */

    info->read_max = 8192;

    /*
     * We are not sure if this is an actual flac file, so do not
     * complain too much about errors in the stream
     */
    info->testing = TRUE;

    /*
     * Try to decode the metadata
     */
    if (false == FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
        ret = FLAC__stream_decoder_get_state(decoder);
        _DEBUG("Could not read the metadata: %s(%d)!",
                FLAC__StreamDecoderStateString[ret], ret);
        /* Do not close the filehandle, it was passed to us */
        reset_info(info, FALSE);
        _LEAVE FALSE;
    }

    /*
     * Seems to be a FLAC stream. Allow further reading
     */

    info->read_max = -1;
    info->testing = FALSE;

    _LEAVE TRUE;
}

/* --- */

Tuple* get_tuple(const gchar* filename, callback_info* info) {

    Tuple *out;

    _ENTER;

    _DEBUG("Using callback_info %s", info->name);

    out = tuple_new_from_filename(filename);

    tuple_associate_string(out, "codec", "Free Lossless Audio Codec (FLAC)");
    tuple_associate_string(out, "quality", "lossless");

    tuple_associate_string(out, "artist", info->comment.artist);
    tuple_associate_string(out, "title", info->comment.title);
    tuple_associate_string(out, "album", info->comment.album);
    tuple_associate_string(out, "genre", info->comment.genre);

    if (info->comment.tracknumber != NULL)
        tuple_associate_int(out, "track-number", atoi(info->comment.tracknumber));

    if (info->comment.date != NULL)
        tuple_associate_int(out, "year", atoi(info->comment.date));

    /*
     * Calculate the stream length (milliseconds)
     */
    if (0 == info->stream.samplerate) {
        _ERROR("Invalid sample rate for stream!");
        tuple_associate_int(out, "length", -1);
    } else {
        tuple_associate_int(out, "length", (info->stream.samples / info->stream.samplerate) * 1000);
        _DEBUG("Stream length: %d seconds", tuple_get_int(out, "length"));
    }

    _DEBUG("Tuple created: [%p]", out);

    _LEAVE out;
}

/* --- */

gchar* get_title(const gchar* filename, callback_info* info) {

    Tuple *input;
    gchar *title;

    _ENTER;

    _DEBUG("Using callback_info %s", info->name);

    input = get_tuple(filename, info);

    title = tuple_formatter_make_title_string(input, get_gentitle_format());

    tuple_free(input);

    _DEBUG("Title created: <%s>", title);

    _LEAVE title;
}

/* --- */

void add_comment(callback_info* info, gchar* key, gchar* value) {

    /*
     * We are currently interested in comments of type
     * - ARTIST
     * - ALBUM
     * - TITLE
     * - TRACKNUMBER
     *
     * We have to create copies of the value string.
     */

    gchar** destination = NULL;
    gboolean rg = FALSE;

     _ENTER;

     _DEBUG("Using callback_info %s", info->name);

    if (0 == strcasecmp(key, "ARTIST")) {
        _DEBUG("Found key ARTIST");
        destination = &(info->comment.artist);
    }

    if (0 == strcasecmp(key, "ALBUM")) {
        _DEBUG("Found key ALBUM");
        destination = &(info->comment.album);
    }

    if (0 == strcasecmp(key, "TITLE")) {
        _DEBUG("Found key TITLE");
        destination = &(info->comment.title);
    }

    if (0 == strcasecmp(key, "TRACKNUMBER")) {
        _DEBUG("Found key TRACKNUMBER");
        destination = &(info->comment.tracknumber);
    }

    if (0 == strcasecmp(key, "REPLAYGAIN_REFERENCE_LOUDNESS")) {
        _DEBUG("Found key REPLAYGAIN_REFERENCE_LOUDNESS");
        destination = &(info->replaygain.ref_loud);
        rg = TRUE;
    }

    if (0 == strcasecmp(key, "REPLAYGAIN_TRACK_GAIN")) {
        _DEBUG("Found key REPLAYGAIN_TRACK_GAIN");
        destination = &(info->replaygain.track_gain);
        rg = TRUE;
    }

    if (0 == strcasecmp(key, "REPLAYGAIN_TRACK_PEAK")) {
        _DEBUG("Found key REPLAYGAIN_TRACK_PEAK");
        destination = &(info->replaygain.track_peak);
        rg = TRUE;
    }

    if (0 == strcasecmp(key, "REPLAYGAIN_ALBUM_GAIN")) {
        _DEBUG("Found key REPLAYGAIN_ALBUM_GAIN");
        destination = &(info->replaygain.album_gain);
        rg = TRUE;
    }

    if (0 == strcasecmp(key, "REPLAYGAIN_ALBUM_PEAK")) {
        _DEBUG("Found key REPLAYGAIN_ALBUM_PEAK");
        destination = &(info->replaygain.album_peak);
        rg = TRUE;
    }

    if (0 == strcasecmp(key, "DATE")) {
        _DEBUG("Found key DATE");
        destination = &(info->comment.date);
    }

    if (0 == strcasecmp(key, "GENRE")) {
        _DEBUG("Found key GENRE");
        destination = &(info->comment.genre);
    }

    if (NULL != destination) {
        if (NULL != *destination) {
            g_free(*destination);
        }

        if (NULL == (*destination = g_strdup(value))) {
            _ERROR("Could not allocate memory for comment!");
            _LEAVE;
        }
    }

    if (rg) {
        info->replaygain.has_rg = TRUE;
    }

    _LEAVE;
}
