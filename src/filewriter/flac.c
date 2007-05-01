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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "plugins.h"
#include <FLAC/stream_encoder.h>
#include <stdlib.h>

static gint flac_open(void);
static void flac_write(gpointer data, gint length);
static void flac_close(void);
static gint flac_free(void);
static gint flac_playing(void);
static gint flac_get_written_time(void);

FileWriter flac_plugin =
{
    NULL,
    NULL,
    flac_open,
    flac_write,
    flac_close,
    flac_free,
    flac_playing,
    flac_get_written_time
};

static FLAC__StreamEncoder *flac_encoder;
static guint64 olen = 0;

static FLAC__StreamEncoderWriteStatus flac_write_cb(const FLAC__StreamEncoder *encoder,
    const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, gpointer data)
{
    written += vfs_fwrite(buffer, bytes, 1, (VFSFile *) data);

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

static gint flac_open(void)
{
    written = 0;
    olen = 0;

    flac_encoder = FLAC__stream_encoder_new();

    FLAC__stream_encoder_set_channels(flac_encoder, input.channels);
    FLAC__stream_encoder_set_sample_rate(flac_encoder, input.frequency);
    FLAC__stream_encoder_init_stream(flac_encoder, flac_write_cb, flac_seek_cb, flac_tell_cb,
				     NULL, output_file);

    return 1;
}

static void flac_write(gpointer data, gint length)
{
#if 0
    FLAC__int32 *encbuffer[2];
    short int *tmpdata = data;
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

    FLAC__stream_encoder_process(flac_encoder, encbuffer, length / (input.channels * 2));
    olen += length;

    g_free(encbuffer[0]);
    g_free(encbuffer[1]);
#endif

    FLAC__int32 *encbuffer;
    gint16_t *tmpdata = data;
    int i;

    encbuffer = g_new0(FLAC__int32, length);

    for (i=0; i < (length/2); i++) {
        encbuffer[i] = tmpdata[i];
    }

    FLAC__stream_encoder_process_interleaved(flac_encoder, encbuffer, (length/2));
    olen += length;

    g_free(encbuffer);
}

static void flac_close(void)
{
    FLAC__stream_encoder_finish(flac_encoder);
    FLAC__stream_encoder_delete(flac_encoder);
}

static gint flac_free(void)
{
    return 1000000;
}

static gint flac_playing(void)
{
    return 0;
}

static gint flac_get_written_time(void)
{
    if (input.frequency && input.channels)
        return (gint) ((olen * 1000) / (input.frequency * 2 * input.channels));

    return 0;
}
