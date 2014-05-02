/*  FileWriter FLAC Plugin
 *  Copyright (c) 2007 William Pitcock <nenolod@sacredspiral.co.uk>
 *
 *  Partially derived from Og(g)re - Ogg-Output-Plugin:
 *  Copyright (c) 2002 Lars Siebold <khandha5@gmx.net>
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

#include "plugins.h"

#ifdef FILEWRITER_FLAC

#include <FLAC/all.h>
#include <stdlib.h>

static FLAC__StreamEncoder *flac_encoder;
static FLAC__StreamMetadata *flac_metadata;

static FLAC__StreamEncoderWriteStatus flac_write_cb(const FLAC__StreamEncoder *encoder,
    const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, gpointer data)
{
    VFSFile *file = (VFSFile *) data;

    if (vfs_fwrite (buffer, 1, bytes, file) != (int64_t) bytes)
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus flac_seek_cb(const FLAC__StreamEncoder *encoder,
    FLAC__uint64 absolute_byte_offset, gpointer data)
{
    VFSFile *file = (VFSFile *) data;

    if (vfs_fseek(file, absolute_byte_offset, SEEK_SET) < 0)
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;

    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus flac_tell_cb(const FLAC__StreamEncoder *encoder,
    FLAC__uint64 *absolute_byte_offset, gpointer data)
{
    VFSFile *file = (VFSFile *) data;

    *absolute_byte_offset = vfs_ftell(file);

    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static void insert_vorbis_comment (FLAC__StreamMetadata * meta,
 const char * name, const Tuple * tuple, int field)
{
    TupleValueType type = tuple_field_get_type (field);
    if (tuple_get_value_type (tuple, field) != type)
        return;

    char * temp;

    switch (type)
    {
    case TUPLE_INT:
    {
        int ival = tuple_get_int (tuple, field);
        temp = g_strdup_printf ("%s=%d", name, ival);
        break;
    }
    case TUPLE_STRING:
    {
        String sval = tuple_get_str (tuple, field);
        temp = g_strdup_printf ("%s=%s", name, (const char *) sval);
        break;
    }
    default:
        return;
    }

    FLAC__StreamMetadata_VorbisComment_Entry comment;
    comment.length = strlen (temp);
    comment.entry = (unsigned char *) temp;

    FLAC__metadata_object_vorbiscomment_insert_comment (meta,
     meta->data.vorbis_comment.num_comments, comment, TRUE);

    g_free (temp);
}

static gint flac_open(void)
{
    flac_encoder = FLAC__stream_encoder_new();

    FLAC__stream_encoder_set_channels(flac_encoder, input.channels);
    FLAC__stream_encoder_set_sample_rate(flac_encoder, input.frequency);

    if (tuple)
    {
        flac_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

        insert_vorbis_comment (flac_metadata, "TITLE", tuple, FIELD_TITLE);
        insert_vorbis_comment (flac_metadata, "ARTIST", tuple, FIELD_ARTIST);
        insert_vorbis_comment (flac_metadata, "ALBUM", tuple, FIELD_ALBUM);
        insert_vorbis_comment (flac_metadata, "GENRE", tuple, FIELD_GENRE);
        insert_vorbis_comment (flac_metadata, "COMMENT", tuple, FIELD_COMMENT);
        insert_vorbis_comment (flac_metadata, "DATE", tuple, FIELD_DATE);
        insert_vorbis_comment (flac_metadata, "YEAR", tuple, FIELD_YEAR);
        insert_vorbis_comment (flac_metadata, "TRACKNUMBER", tuple, FIELD_TRACK_NUMBER);

        FLAC__stream_encoder_set_metadata(flac_encoder, &flac_metadata, 1);
    }

    FLAC__stream_encoder_init_stream(flac_encoder, flac_write_cb, flac_seek_cb,
     flac_tell_cb, NULL, output_file);

    return 1;
}

static void flac_write(gpointer data, gint length)
{
#if 1
    FLAC__int32 *encbuffer[2];
    int16_t *tmpdata = (int16_t *) data;
    int i;

    encbuffer[0] = g_new0(FLAC__int32, length / input.channels);
    encbuffer[1] = g_new0(FLAC__int32, length / input.channels);

    if (input.channels == 1)
    {
        for (i = 0; i < (length / 2); i++)
        {
            encbuffer[0][i] = tmpdata[i];
            encbuffer[1][i] = tmpdata[i];
        }
    }
    else
    {
        for (i = 0; i < (length / 4); i++)
        {
            encbuffer[0][i] = tmpdata[2 * i];
            encbuffer[1][i] = tmpdata[2 * i + 1];
        }
    }

    FLAC__stream_encoder_process(flac_encoder, (const FLAC__int32 **)encbuffer, length / (input.channels * 2));

    g_free(encbuffer[0]);
    g_free(encbuffer[1]);
#else
    FLAC__int32 *encbuffer;
    gint16 *tmpdata = data;
    int i;

    encbuffer = g_new0(FLAC__int32, length);

    for (i=0; i < length; i++) {
        encbuffer[i] = tmpdata[i];
    }

    FLAC__stream_encoder_process_interleaved(flac_encoder, encbuffer, length);

    g_free(encbuffer);
#endif
}

static void flac_close(void)
{
    if (flac_encoder)
    {
        FLAC__stream_encoder_finish(flac_encoder);
        FLAC__stream_encoder_delete(flac_encoder);
        flac_encoder = NULL;
    }

    if (flac_metadata)
    {
        FLAC__metadata_object_delete(flac_metadata);
        flac_metadata = NULL;
    }
}

static int flac_format_required (int fmt)
{
    return FMT_S16_NE;
}

FileWriter flac_plugin =
{
    .init = NULL,
    .configure = NULL,
    .open = flac_open,
    .write = flac_write,
    .close = flac_close,
    .format_required = flac_format_required,
};

#endif
