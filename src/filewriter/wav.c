/*  FileWriter-Plugin
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

#include "plugins.h"

static gint wav_open(void);
static void wav_write(void *ptr, gint length);
static void wav_close(void);

FileWriter wav_plugin =
{
    .init = NULL,
    .configure = NULL,
    .open = wav_open,
    .write = wav_write,
    .close = wav_close,
    .format_required = FMT_S16_LE,
};

#pragma pack(push) /* must be byte-aligned */
#pragma pack(1)
struct wavhead
{
    guint32 main_chunk;
    guint32 length;
    guint32 chunk_type;
    guint32 sub_chunk;
    guint32 sc_len;
    guint16 format;
    guint16 modus;
    guint32 sample_fq;
    guint32 byte_p_sec;
    guint16 byte_p_spl;
    guint16 bit_p_spl;
    guint32 data_chunk;
    guint32 data_length;
};
#pragma pack(pop)

static struct wavhead header;

static guint64 written;

static gint wav_open(void)
{
    memcpy(&header.main_chunk, "RIFF", 4);
    header.length = GUINT32_TO_LE(0);
    memcpy(&header.chunk_type, "WAVE", 4);
    memcpy(&header.sub_chunk, "fmt ", 4);
    header.sc_len = GUINT32_TO_LE(16);
    header.format = GUINT16_TO_LE(1);
    header.modus = GUINT16_TO_LE(input.channels);
    header.sample_fq = GUINT32_TO_LE(input.frequency);
    if (input.format == FMT_U8 || input.format == FMT_S8)
        header.bit_p_spl = GUINT16_TO_LE(8);
    else
        header.bit_p_spl = GUINT16_TO_LE(16);
    header.byte_p_sec = GUINT32_TO_LE(input.frequency * header.modus * (GUINT16_FROM_LE(header.bit_p_spl) / 8));
    header.byte_p_spl = GUINT16_TO_LE((GUINT16_FROM_LE(header.bit_p_spl) / (8 / input.channels)));
    memcpy(&header.data_chunk, "data", 4);
    header.data_length = GUINT32_TO_LE(0);

    if (vfs_fwrite (& header, 1, sizeof header, output_file) != sizeof header)
        return 0;

    written = 0;

    return 1;
}

static void wav_write (void * data, gint len)
{
    written += len;
    if (vfs_fwrite (data, 1, len, output_file) != len)
        fprintf (stderr, "Error while writing to .wav output file.\n");
}

static void wav_close(void)
{
    if (output_file)
    {
        header.length = GUINT32_TO_LE(written + sizeof (struct wavhead) - 8);
        header.data_length = GUINT32_TO_LE(written);

        if (vfs_fseek (output_file, 0, SEEK_SET) || vfs_fwrite (& header, 1,
         sizeof header, output_file) != sizeof header)
            fprintf (stderr, "Error while writing to .wav output file.\n");
    }
}
