/*  FileWriter-Plugin
 *  (C) copyright 2012 Michał Lipski
 *  (C) copyright 2007 merging of Disk Writer and Out-Lame by Michael Färber
 *
 *  Original Out-Lame-Plugin:
 *  (C) copyright 2002 Lars Siebold <khandha5@gmx.net>
 *  (C) copyright 2006-2007 porting to audacious by Yoshiki Yazawa <yaz@cc.rim.or.jp>
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

#include <string.h>
#include <libaudcore/runtime.h>

#pragma pack(push) /* must be byte-aligned */
#pragma pack(1)
struct wavhead
{
    uint32_t main_chunk;
    uint32_t length;
    uint32_t chunk_type;
    uint32_t sub_chunk;
    uint32_t sc_len;
    uint16_t format;
    uint16_t modus;
    uint32_t sample_fq;
    uint32_t byte_p_sec;
    uint16_t byte_p_spl;
    uint16_t bit_p_spl;
    uint32_t data_chunk;
    uint32_t data_length;
};
#pragma pack(pop)

static struct wavhead header;

static int format;
static Index<char> packbuf;

static uint64_t written;


static bool wav_open (VFSFile & file, const format_info & info, const Tuple &)
{
    memcpy(&header.main_chunk, "RIFF", 4);
    header.length = TO_LE32(0);
    memcpy(&header.chunk_type, "WAVE", 4);
    memcpy(&header.sub_chunk, "fmt ", 4);
    header.sc_len = TO_LE32(16);
    if (info.format == FMT_FLOAT)
        header.format = TO_LE16(3);
    else
        header.format = TO_LE16(1);
    header.modus = TO_LE16(info.channels);
    header.sample_fq = TO_LE32(info.frequency);
    if (info.format == FMT_S16_LE)
        header.bit_p_spl = TO_LE16(16);
    else if (info.format == FMT_S24_LE)
        header.bit_p_spl = TO_LE16(24);
    else
        header.bit_p_spl = TO_LE16(32);
    header.byte_p_sec = TO_LE32(info.frequency * header.modus * (FROM_LE16(header.bit_p_spl) / 8));
    header.byte_p_spl = TO_LE16((FROM_LE16(header.bit_p_spl) / (8 / info.channels)));
    memcpy(&header.data_chunk, "data", 4);
    header.data_length = TO_LE32(0);

    if (file.fwrite (& header, 1, sizeof header) != sizeof header)
        return false;

    format = info.format;
    written = 0;

    return true;
}

static void pack24 (const void * * data, int * len)
{
    int samples = (* len) / sizeof (int32_t);
    auto data32 = (const int32_t *) * data;
    auto end = data32 + samples;

    packbuf.resize (samples * 3);
    char * buf = packbuf.begin ();

    * data = buf;
    * len = samples * 3;

    while (data32 < end)
    {
        memcpy (buf, data32 ++, 3);
        buf += 3;
    }
}

static void wav_write (VFSFile & file, const void * data, int len)
{
    if (format == FMT_S24_LE)
        pack24 (& data, & len);

    written += len;
    if (file.fwrite (data, 1, len) != len)
        AUDERR ("Error while writing to .wav output file.\n");
}

static void wav_close (VFSFile & file)
{
    header.length = TO_LE32(written + sizeof (struct wavhead) - 8);
    header.data_length = TO_LE32(written);

    if (file.fseek (0, VFS_SEEK_SET) ||
     file.fwrite (& header, 1, sizeof header) != sizeof header)
        AUDERR ("Error while writing to .wav output file.\n");

    packbuf.clear ();
}

static int wav_format_required (int fmt)
{
    switch (fmt)
    {
        case FMT_S16_LE:
        case FMT_S24_LE:
        case FMT_S32_LE:
        case FMT_FLOAT:
            return fmt;
        default:
            return FMT_S16_LE;
    }
}

FileWriterImpl wav_plugin = {
    nullptr,  // init
    wav_open,
    wav_write,
    wav_close,
    wav_format_required,
};
