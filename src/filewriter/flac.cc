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
    const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void * data)
{
    VFSFile *file = (VFSFile *) data;

    if (file->fwrite (buffer, 1, bytes) != (int64_t) bytes)
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus flac_seek_cb(const FLAC__StreamEncoder *encoder,
    FLAC__uint64 absolute_byte_offset, void * data)
{
    VFSFile *file = (VFSFile *) data;

    if (file->fseek (absolute_byte_offset, VFS_SEEK_SET) < 0)
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;

    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus flac_tell_cb(const FLAC__StreamEncoder *encoder,
    FLAC__uint64 *absolute_byte_offset, void * data)
{
    VFSFile *file = (VFSFile *) data;

    *absolute_byte_offset = file->ftell ();

    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static void insert_vorbis_comment (FLAC__StreamMetadata * meta,
 const char * name, const Tuple & tuple, int field)
{
    TupleValueType type = Tuple::field_get_type (field);
    if (tuple.get_value_type (field) != type)
        return;

    char * temp;

    switch (type)
    {
    case TUPLE_INT:
    {
        int ival = tuple.get_int (field);
        temp = g_strdup_printf ("%s=%d", name, ival);
        break;
    }
    case TUPLE_STRING:
    {
        String sval = tuple.get_str (field);
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

static int flac_open(void)
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
     flac_tell_cb, nullptr, &output_file);

    return 1;
}

static void flac_write(void * data, int length)
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
    int16_t *tmpdata = data;
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
        flac_encoder = nullptr;
    }

    if (flac_metadata)
    {
        FLAC__metadata_object_delete(flac_metadata);
        flac_metadata = nullptr;
    }
}

static int flac_format_required (int fmt)
{
    return FMT_S16_NE;
}

FileWriterImpl flac_plugin = {
    nullptr,  // init
    nullptr,  // configure
    flac_open,
    flac_write,
    flac_close,
    flac_format_required,
};

#endif
