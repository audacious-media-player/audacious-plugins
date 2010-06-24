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

    if (NULL == (info = g_malloc0(sizeof(callback_info)))) {
        _ERROR("Could not allocate memory for callback structure!");
        _LEAVE NULL;
    }

    if (NULL == (info->output_buffer = g_malloc(BUFFER_SIZE_BYTE))) {
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
    reset_info(info, FALSE);

    info->mutex = g_mutex_new();

    _DEBUG("Playback buffer allocated for %d samples, %d bytes", BUFFER_SIZE_SAMP, BUFFER_SIZE_BYTE);

    _LEAVE info;
}

/* --- */

void clean_callback_info(callback_info* info)
{
    g_mutex_free(info->mutex);
    g_free(info->output_buffer);
    g_free(info);
}

/* --- */

void reset_info(callback_info* info, gboolean close_fd) {

    _ENTER;

    _DEBUG("Using callback_info %s", info->name);

    if (close_fd) {
        info->input_stream = NULL;
    }

    // memset(info->output_buffer, 0, BUFFER_SIZE * sizeof(int16_t));
    info->buffer_free = BUFFER_SIZE_SAMP;
    info->buffer_used = 0;
    info->write_pointer = info->output_buffer;
    info->read_max = -1;
    info->testing = FALSE;

    /*
     * Clear the stream and frame information
     */
    memset(&(info->stream), 0, sizeof(info->stream));
    memset(&(info->frame), 0, sizeof(info->frame));

    g_free(info->comment.artist);
    g_free(info->comment.album);
    g_free(info->comment.title);
    g_free(info->comment.tracknumber);
    g_free(info->comment.genre);
    g_free(info->comment.comment);
    g_free(info->comment.date);
    memset(&(info->comment), 0, sizeof(info->comment));

    g_free(info->replaygain.ref_loud);
    g_free(info->replaygain.track_gain);
    g_free(info->replaygain.track_peak);
    g_free(info->replaygain.album_gain);
    g_free(info->replaygain.album_peak);
    memset(&(info->replaygain), 0, sizeof(info->replaygain));

    info->metadata_changed = FALSE;

    _LEAVE;
}

/* --- */

static gint get_bitrate (VFSFile * handle, callback_info * info)
{
    gsize size = aud_vfs_fsize (handle);

    if (size == -1)
        return 0;

    return 8 * size * (gint64) info->stream.samplerate / info->stream.samples;
}

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

    info->bitrate = get_bitrate (fd, info);

    _LEAVE TRUE;
}

/* --- */

Tuple* get_tuple_from_file(const gchar *filename, VFSFile *fd, callback_info *info)
{
    Tuple *out;

    _ENTER;

    _DEBUG("Using callback_info %s", info->name);

    out = aud_tuple_new_from_filename(filename);

    aud_tuple_associate_string(out, FIELD_CODEC, NULL, "Free Lossless Audio Codec (FLAC)");
    aud_tuple_associate_string(out, FIELD_QUALITY, NULL, "lossless");

    aud_tuple_associate_string(out, FIELD_ARTIST, NULL, info->comment.artist);
    aud_tuple_associate_string(out, FIELD_TITLE, NULL, info->comment.title);
    aud_tuple_associate_string(out, FIELD_ALBUM, NULL, info->comment.album);
    aud_tuple_associate_string(out, FIELD_GENRE, NULL, info->comment.genre);
    aud_tuple_associate_string(out, FIELD_COMMENT, NULL, info->comment.comment);

    if (info->comment.tracknumber != NULL)
        aud_tuple_associate_int(out, FIELD_TRACK_NUMBER, NULL, atoi(info->comment.tracknumber));

    if (info->comment.date != NULL)
        aud_tuple_associate_int(out, FIELD_YEAR, NULL, atoi(info->comment.date));

    /*
     * Calculate the stream length (milliseconds)
     */
    if (0 == info->stream.samplerate) {
        _ERROR("Invalid sample rate for stream!");
        aud_tuple_associate_int(out, FIELD_LENGTH, NULL, -1);
    } else {
        aud_tuple_associate_int(out, FIELD_LENGTH, NULL, (info->stream.samples / info->stream.samplerate) * 1000);
        _DEBUG("Stream length: %d seconds", aud_tuple_get_int(out, FIELD_LENGTH, NULL));
    }

    if (info->bitrate > 0)
        aud_tuple_associate_int (out, FIELD_BITRATE, NULL, (info->bitrate + 500)
         / 1000);


    if (info->replaygain.has_rg)
    {
        if (info->replaygain.album_gain != NULL)
            aud_tuple_associate_int (out, FIELD_GAIN_ALBUM_GAIN, NULL, atof
             (info->replaygain.album_gain) * 100);

        if (info->replaygain.album_peak != NULL)
            aud_tuple_associate_int (out, FIELD_GAIN_ALBUM_PEAK, NULL, atof
             (info->replaygain.album_peak) * 100);

        if (info->replaygain.track_gain != NULL)
            aud_tuple_associate_int (out, FIELD_GAIN_TRACK_GAIN, NULL, atof
             (info->replaygain.track_gain) * 100);

        if (info->replaygain.track_peak != NULL)
            aud_tuple_associate_int (out, FIELD_GAIN_TRACK_PEAK, NULL, atof
             (info->replaygain.track_peak) * 100);

        aud_tuple_associate_int(out, FIELD_GAIN_GAIN_UNIT, NULL, 100);
        aud_tuple_associate_int(out, FIELD_GAIN_PEAK_UNIT, NULL, 100);
    }

    _DEBUG("Tuple created: [%p]", out);

    _LEAVE out;
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

    if (0 == strcasecmp(key, "COMMENT")) {
        _DEBUG("Found key COMMENT");
        destination = &(info->comment.comment);
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
        g_free(*destination);

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
