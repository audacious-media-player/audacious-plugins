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

#include "filewriter.h"

#ifdef FILEWRITER_FLAC

#include <FLAC/all.h>

#include <libaudcore/audstrings.h>

static int channels;
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
 const char * name, const Tuple & tuple, Tuple::Field field)
{
    Tuple::ValueType type = Tuple::field_get_type (field);
    if (tuple.get_value_type (field) != type)
        return;

    StringBuf temp;

    switch (type)
    {
    case Tuple::Int:
    {
        int ival = tuple.get_int (field);
        temp.steal (str_printf ("%s=%d", name, ival));
        break;
    }
    case Tuple::String:
    {
        String sval = tuple.get_str (field);
        temp.steal (str_printf ("%s=%s", name, (const char *) sval));
        break;
    }
    default:
        return;
    }

    FLAC__StreamMetadata_VorbisComment_Entry comment;
    comment.length = temp.len ();
    comment.entry = (unsigned char *) (char *) temp;

    FLAC__metadata_object_vorbiscomment_insert_comment (meta,
     meta->data.vorbis_comment.num_comments, comment, true);
}

static bool flac_open (VFSFile & file, const format_info & info, const Tuple & tuple)
{
    flac_encoder = FLAC__stream_encoder_new();

    FLAC__stream_encoder_set_channels(flac_encoder, info.channels);
    FLAC__stream_encoder_set_sample_rate(flac_encoder, info.frequency);

    flac_metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

    insert_vorbis_comment (flac_metadata, "TITLE", tuple, Tuple::Title);
    insert_vorbis_comment (flac_metadata, "ARTIST", tuple, Tuple::Artist);
    insert_vorbis_comment (flac_metadata, "ALBUM", tuple, Tuple::Album);
    insert_vorbis_comment (flac_metadata, "GENRE", tuple, Tuple::Genre);
    insert_vorbis_comment (flac_metadata, "COMMENT", tuple, Tuple::Comment);
    insert_vorbis_comment (flac_metadata, "DATE", tuple, Tuple::Date);
    insert_vorbis_comment (flac_metadata, "YEAR", tuple, Tuple::Year);
    insert_vorbis_comment (flac_metadata, "TRACKNUMBER", tuple, Tuple::Track);

    FLAC__stream_encoder_set_metadata(flac_encoder, &flac_metadata, 1);

    FLAC__stream_encoder_init_stream(flac_encoder, flac_write_cb, flac_seek_cb,
     flac_tell_cb, nullptr, &file);

    channels = info.channels;
    return true;
}

static void flac_write (VFSFile & file, const void * data, int length)
{
#if 1
    FLAC__int32 *encbuffer[2];
    int16_t *tmpdata = (int16_t *) data;
    int i;

    encbuffer[0] = new FLAC__int32[length / channels];
    encbuffer[1] = new FLAC__int32[length / channels];

    if (channels == 1)
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

    FLAC__stream_encoder_process(flac_encoder, (const FLAC__int32 **)encbuffer, length / (channels * 2));

    delete[] encbuffer[0];
    delete[] encbuffer[1];
#else
    FLAC__int32 *encbuffer;
    int16_t *tmpdata = (int16_t *) data;
    int i;

    encbuffer = new FLAC__int32[length];

    for (i=0; i < length; i++) {
        encbuffer[i] = tmpdata[i];
    }

    FLAC__stream_encoder_process_interleaved(flac_encoder, encbuffer, length);

    delete[] encbuffer;
#endif
}

static void flac_close (VFSFile & file)
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
    flac_open,
    flac_write,
    flac_close,
    flac_format_required,
};

#endif
