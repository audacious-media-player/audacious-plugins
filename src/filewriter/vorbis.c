/*  FileWriter Vorbis Plugin
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
#include <vorbis/vorbisenc.h>
#include <stdlib.h>

static void vorbis_init(void);
static gint vorbis_open(void);
static void vorbis_write(gpointer data, gint length);
static void vorbis_close(void);
static gint vorbis_free(void);
static gint vorbis_playing(void);
static gint vorbis_get_written_time(void);

FileWriter vorbis_plugin =
{
    vorbis_init,
    NULL,
    vorbis_open,
    vorbis_write,
    vorbis_close,
    vorbis_free,
    vorbis_playing,
    vorbis_get_written_time
};

static float v_base_quality = 0.3;

static ogg_stream_state os;
static ogg_page og;
static ogg_packet op;

static vorbis_dsp_state vd;
static vorbis_block vb;
static vorbis_info vi;
static vorbis_comment vc;

static float **encbuffer;
static guint64 olen = 0;

static void vorbis_init(void)
{
    ConfigDb *db = bmp_cfg_db_open();

    bmp_cfg_db_get_float(db, "filewriter_vorbis", "base_quality", &v_base_quality);

    bmp_cfg_db_close(db);
}

static gint vorbis_open(void)
{
    gint result;
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_init();

    written = 0;
    olen = 0;

    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);

    if (tuple)
    {
        vorbis_comment_add_tag(&vc, "title", tuple->track_name);
        vorbis_comment_add_tag(&vc, "artist", tuple->performer);
        vorbis_comment_add_tag(&vc, "album", tuple->album_name);
        vorbis_comment_add_tag(&vc, "genre", tuple->genre);
    }

    if ((result = vorbis_encode_init_vbr(&vi, (long)input.channels, (long)input.frequency, v_base_quality)) != 0)
    {
        vorbis_info_clear(&vi);
        return 0;
    }

    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    srand(time(NULL));
    ogg_stream_init(&os, rand());

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    while((result = ogg_stream_flush(&os, &og)))
    {
        if (result == 0)
            break;

        written += vfs_fwrite(og.header, 1, og.header_len, output_file);
        written += vfs_fwrite(og.body, 1, og.body_len, output_file);
    }

    return 1;
}

static void vorbis_write(gpointer data, gint length)
{
    int i;
    int result;
    short int *tmpdata;

    /* ask vorbisenc for a buffer to fill with pcm data */
    encbuffer = vorbis_analysis_buffer(&vd, length);
    tmpdata = data;

    /*
     * deinterleave the audio signal, 32768.0 is the highest peak level allowed
     * in a 16-bit PCM signal.
     */
    if (input.channels == 1)
    {
        for (i = 0; i < (length / 2); i++)
        {
            encbuffer[0][i] = tmpdata[i] / 32768.0;
            encbuffer[1][i] = tmpdata[i] / 32768.0;
        }
    }
    else
    {
        for (i = 0; i < (length / 4); i++)
        {
            encbuffer[0][i] = tmpdata[2 * i] / 32768.0;
            encbuffer[1][i] = tmpdata[2 * i + 1] / 32768.0;
        }
    }

    vorbis_analysis_wrote(&vd, i);

    while(vorbis_analysis_blockout(&vd, &vb) == 1)
    {
        vorbis_analysis(&vb, &op);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op))
        {
            ogg_stream_packetin(&os, &op);

            while ((result = ogg_stream_pageout(&os, &og)))
            {
                if (result == 0)
                    break;

                written += vfs_fwrite(og.header, 1, og.header_len, output_file);
                written += vfs_fwrite(og.body, 1, og.body_len, output_file);
            }
        }
    }

    olen += length;
}

static void vorbis_close(void)
{
    ogg_stream_clear(&os);

    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_info_clear(&vi);
}

static gint vorbis_free(void)
{
    return 1000000;
}

static gint vorbis_playing(void)
{
    return 0;
}

static gint vorbis_get_written_time(void)
{
    if (input.frequency && input.channels)
        return (gint) ((olen * 1000) / (input.frequency * 2 * input.channels));

    return 0;
}
