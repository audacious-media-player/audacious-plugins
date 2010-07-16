/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *  Copyright (C) 2010 John Lindgren
 *  Copyright (C) 2010 Michał Lipski <tallica@o2.pl>
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

#include <audacious/debug.h>

#include "tools.h"

callback_info *init_callback_info(void)
{
    callback_info *info;

    if ((info = g_slice_new0(callback_info)) == NULL)
    {
        ERROR("Could not allocate memory for callback structure!");
        return NULL;
    }

    if ((info->output_buffer = g_malloc0(BUFFER_SIZE_BYTE)) == NULL)
    {
        ERROR("Could not allocate memory for output buffer!");
        return NULL;
    }

    reset_info(info);

    info->mutex = g_mutex_new();

    AUDDBG("Playback buffer allocated for %d samples, %d bytes\n", BUFFER_SIZE_SAMP, BUFFER_SIZE_BYTE);

    return info;
}

void clean_callback_info(callback_info *info)
{
    g_mutex_free(info->mutex);
    g_free(info->output_buffer);
    g_slice_free(callback_info, info);
}

void reset_info(callback_info *info)
{
    info->buffer_free = BUFFER_SIZE_SAMP;
    info->buffer_used = 0;
    info->write_pointer = info->output_buffer;

    /* Clear the stream and frame information */
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

    g_free(info->replaygain.track_gain);
    g_free(info->replaygain.track_peak);
    g_free(info->replaygain.album_gain);
    g_free(info->replaygain.album_peak);
    memset(&(info->replaygain), 0, sizeof(info->replaygain));

    info->metadata_changed = FALSE;
}

gboolean read_metadata(FLAC__StreamDecoder *decoder, callback_info *info)
{
    FLAC__StreamDecoderState ret;

    reset_info(info);

    /* Reset the decoder */
    if (FLAC__stream_decoder_reset(decoder) == false)
    {
        ERROR("Could not reset the decoder!\n");
        return FALSE;
    }

    /* Try to decode the metadata */
    if (FLAC__stream_decoder_process_until_end_of_metadata(decoder) == false)
    {
        ret = FLAC__stream_decoder_get_state(decoder);
        AUDDBG("Could not read the metadata: %s(%d)!\n",
            FLAC__StreamDecoderStateString[ret], ret);
        reset_info(info);
        return FALSE;
    }

    return TRUE;
}

static void parse_gain_text(const gchar *text, gint *value, gint *unit)
{
    gint sign = 1;

    *value = 0;
    *unit = 1;

    if (*text == '-')
    {
        sign = -1;
        text++;
    }

    while (*text >= '0' && *text <= '9')
    {
        *value = *value * 10 + (*text - '0');
        text++;
    }

    if (*text == '.')
    {
        text++;

        while (*text >= '0' && *text <= '9' && *value < G_MAXINT / 10)
        {
            *value = *value * 10 + (*text - '0');
            *unit = *unit * 10;
            text++;
        }
    }

    *value = *value * sign;
}

static void set_gain_info(Tuple *tuple, gint field, gint unit_field, const gchar *text)
{
    gint value, unit;

    parse_gain_text(text, &value, &unit);

    if (tuple_get_value_type(tuple, unit_field, NULL) == TUPLE_INT)
        value = value * (gint64) tuple_get_int(tuple, unit_field, NULL) / unit;
    else
        tuple_associate_int(tuple, unit_field, NULL, unit);

    tuple_associate_int(tuple, field, NULL, value);
}

Tuple* get_tuple_from_file(const gchar *filename, callback_info *info)
{
    Tuple *tuple;

    tuple = tuple_new_from_filename(filename);

    tuple_associate_string(tuple, FIELD_CODEC, NULL, "Free Lossless Audio Codec (FLAC)");
    tuple_associate_string(tuple, FIELD_QUALITY, NULL, "lossless");

    tuple_associate_string(tuple, FIELD_ARTIST, NULL, info->comment.artist);
    tuple_associate_string(tuple, FIELD_TITLE, NULL, info->comment.title);
    tuple_associate_string(tuple, FIELD_ALBUM, NULL, info->comment.album);
    tuple_associate_string(tuple, FIELD_GENRE, NULL, info->comment.genre);
    tuple_associate_string(tuple, FIELD_COMMENT, NULL, info->comment.comment);

    if (info->comment.tracknumber != NULL)
        tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi(info->comment.tracknumber));

    if (info->comment.date != NULL)
        tuple_associate_int(tuple, FIELD_YEAR, NULL, atoi(info->comment.date));

    /* Calculate the stream length (milliseconds) */
    if (info->stream.samplerate == 0)
    {
        ERROR("Invalid sample rate for stream!\n");
        tuple_associate_int(tuple, FIELD_LENGTH, NULL, -1);
    }
    else
    {
        tuple_associate_int(tuple, FIELD_LENGTH, NULL, (info->stream.samples / info->stream.samplerate) * 1000);
        AUDDBG("Stream length: %d seconds\n", tuple_get_int(tuple, FIELD_LENGTH, NULL));
    }

    if (info->bitrate > 0)
        tuple_associate_int(tuple, FIELD_BITRATE, NULL, (info->bitrate + 500) / 1000);


    if (info->replaygain.has_rg)
    {
        if (info->replaygain.album_gain != NULL)
            set_gain_info(tuple, FIELD_GAIN_ALBUM_GAIN, FIELD_GAIN_GAIN_UNIT,
                info->replaygain.album_gain);

        if (info->replaygain.album_peak != NULL)
            set_gain_info(tuple, FIELD_GAIN_ALBUM_PEAK, FIELD_GAIN_PEAK_UNIT,
                info->replaygain.album_peak);

        if (info->replaygain.track_gain != NULL)
            set_gain_info(tuple, FIELD_GAIN_TRACK_GAIN, FIELD_GAIN_GAIN_UNIT,
                info->replaygain.track_gain);

        if (info->replaygain.track_peak != NULL)
            set_gain_info(tuple, FIELD_GAIN_TRACK_PEAK, FIELD_GAIN_PEAK_UNIT,
                info->replaygain.track_peak);
    }

    return tuple;
}

void add_comment(callback_info *info, gchar *key, gchar *value)
{
    gchar **destination = NULL;
    gboolean rg = FALSE;

    if (strcasecmp(key, "ARTIST") == 0)
    {
        AUDDBG("Found key ARTIST <%s>\n", value);
        destination = &(info->comment.artist);
    }

    if (strcasecmp(key, "ALBUM") == 0)
    {
        AUDDBG("Found key ALBUM <%s>\n", value);
        destination = &(info->comment.album);
    }

    if (strcasecmp(key, "TITLE") == 0)
    {
        AUDDBG("Found key TITLE <%s>\n", value);
        destination = &(info->comment.title);
    }

    if (strcasecmp(key, "TRACKNUMBER") == 0)
    {
        AUDDBG("Found key TRACKNUMBER <%s>\n", value);
        destination = &(info->comment.tracknumber);
    }

    if (strcasecmp(key, "COMMENT") == 0)
    {
        AUDDBG("Found key COMMENT <%s>\n", value);
        destination = &(info->comment.comment);
    }

    if (strcasecmp(key, "DATE") == 0)
    {
        AUDDBG("Found key DATE <%s>\n", value);
        destination = &(info->comment.date);
    }

    if (strcasecmp(key, "GENRE") == 0)
    {
        AUDDBG("Found key GENRE <%s>\n", value);
        destination = &(info->comment.genre);
    }

#if 0
    if (strcasecmp(key, "REPLAYGAIN_REFERENCE_LOUDNESS") == 0)
    {
        AUDDBG("Found key REPLAYGAIN_REFERENCE_LOUDNESS <%s>\n", value);
        destination = &(info->replaygain.ref_loud);
        rg = TRUE;
    }
#endif

    if (strcasecmp(key, "REPLAYGAIN_TRACK_GAIN") == 0)
    {
        AUDDBG("Found key REPLAYGAIN_TRACK_GAIN <%s>\n", value);
        destination = &(info->replaygain.track_gain);
        rg = TRUE;
    }

    if (strcasecmp(key, "REPLAYGAIN_TRACK_PEAK") == 0)
    {
        AUDDBG("Found key REPLAYGAIN_TRACK_PEAK <%s>\n", value);
        destination = &(info->replaygain.track_peak);
        rg = TRUE;
    }

    if (strcasecmp(key, "REPLAYGAIN_ALBUM_GAIN") == 0)
    {
        AUDDBG("Found key REPLAYGAIN_ALBUM_GAIN <%s>\n", value);
        destination = &(info->replaygain.album_gain);
        rg = TRUE;
    }

    if (strcasecmp(key, "REPLAYGAIN_ALBUM_PEAK") == 0)
    {
        AUDDBG("Found key REPLAYGAIN_ALBUM_PEAK <%s>\n", value);
        destination = &(info->replaygain.album_peak);
        rg = TRUE;
    }

    if (destination != NULL)
    {
        g_free(*destination);

        if ((*destination = g_strdup(value)) == NULL)
        {
            ERROR("Could not allocate memory for comment!\n");
            return;
        }
    }

    if (rg)
        info->replaygain.has_rg = TRUE;

    return;
}
