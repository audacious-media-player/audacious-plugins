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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "filewriter.h"

#ifdef FILEWRITER_VORBIS

#include <stdlib.h>
#include <vorbis/vorbisenc.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

static ogg_stream_state os;
static ogg_page og;
static ogg_packet op;

static vorbis_dsp_state vd;
static vorbis_block vb;
static vorbis_info vi;
static vorbis_comment vc;

static const char * const vorbis_defaults[] = {
 "base_quality", "0.5",
 nullptr};

#define GET_DOUBLE(n) aud_get_double("filewriter_vorbis", n)

static int channels;

static void vorbis_init ()
{
    aud_config_set_defaults ("filewriter_vorbis", vorbis_defaults);
}

static void add_string_from_tuple (vorbis_comment * vc, const char * name,
 const Tuple & tuple, Tuple::Field field)
{
    String val = tuple.get_str (field);
    if (val)
        vorbis_comment_add_tag (vc, name, val);
}

static bool vorbis_open (VFSFile & file, const format_info & info, const Tuple & tuple)
{
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_init();

    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);

    int scrint;

    add_string_from_tuple (& vc, "title", tuple, Tuple::Title);
    add_string_from_tuple (& vc, "artist", tuple, Tuple::Artist);
    add_string_from_tuple (& vc, "album", tuple, Tuple::Album);
    add_string_from_tuple (& vc, "genre", tuple, Tuple::Genre);
    add_string_from_tuple (& vc, "date", tuple, Tuple::Date);
    add_string_from_tuple (& vc, "comment", tuple, Tuple::Comment);

    if ((scrint = tuple.get_int (Tuple::Track)) > 0)
        vorbis_comment_add_tag(&vc, "tracknumber", int_to_str (scrint));

    if ((scrint = tuple.get_int (Tuple::Year)) > 0)
        vorbis_comment_add_tag(&vc, "year", int_to_str (scrint));

    if (vorbis_encode_init_vbr(& vi, info.channels, info.frequency, GET_DOUBLE("base_quality")))
    {
        vorbis_info_clear(&vi);
        return false;
    }

    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    ogg_stream_init(&os, rand());

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    while (ogg_stream_flush (& os, & og))
    {
        if (file.fwrite (og.header, 1, og.header_len) != og.header_len ||
         file.fwrite (og.body, 1, og.body_len) != og.body_len)
            AUDERR ("write error\n");
    }

    channels = info.channels;
    return true;
}

static void vorbis_write_real (VFSFile & file, const void * data, int length)
{
    int samples = length / sizeof (float);
    int channel;
    float * end = (float *) data + samples;
    float * * buffer = vorbis_analysis_buffer (& vd, samples / channels);
    float * from, * to;

    for (channel = 0; channel < channels; channel ++)
    {
        to = buffer[channel];

        for (from = (float *) data + channel; from < end; from += channels)
            * to ++ = * from;
    }

    vorbis_analysis_wrote (& vd, samples / channels);

    while(vorbis_analysis_blockout(&vd, &vb) == 1)
    {
        vorbis_analysis(&vb, &op);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op))
        {
            ogg_stream_packetin(&os, &op);

            while (ogg_stream_pageout(&os, &og))
            {
                if (file.fwrite (og.header, 1, og.header_len) != og.header_len ||
                 file.fwrite (og.body, 1, og.body_len) != og.body_len)
                    AUDERR ("write error\n");
            }
        }
    }
}

static void vorbis_write (VFSFile & file, const void * data, int length)
{
    if (length > 0) /* don't signal end of file yet */
        vorbis_write_real (file, data, length);
}

static void vorbis_close (VFSFile & file)
{
    vorbis_write_real (file, nullptr, 0); /* signal end of file */

    while (ogg_stream_flush (& os, & og))
    {
        if (file.fwrite (og.header, 1, og.header_len) != og.header_len ||
         file.fwrite (og.body, 1, og.body_len) != og.body_len)
            AUDERR ("write error\n");
    }

    ogg_stream_clear(&os);

    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_info_clear(&vi);
}

static int vorbis_format_required (int fmt)
{
    return FMT_FLOAT;
}

FileWriterImpl vorbis_plugin = {
    vorbis_init,
    vorbis_open,
    vorbis_write,
    vorbis_close,
    vorbis_format_required,
};

#endif
