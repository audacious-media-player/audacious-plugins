/*  VTX Input Plugin for Audacious
 *
 *  Copyright (C) 2002-2004 Sashnov Alexander
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


#include <libaudcore/i18n.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>

#include "vtx.h"
#include "ayemu.h"

#define SNDBUFSIZE 1024
static char sndbuf[SNDBUFSIZE];
static int freq = 44100;
static int chans = 2;
static int bits = 16;

ayemu_ay_t ay;
ayemu_vtx_t vtx;

static const char *vtx_fmts[] = { "vtx", nullptr };

bool vtx_is_our_fd(const char * filename, VFSFile * fp)
{
    char buf[2];
    if (vfs_fread(buf, 1, 2, fp) < 2)
        return FALSE;
    return (!g_ascii_strncasecmp(buf, "ay", 2) || !g_ascii_strncasecmp(buf, "ym", 2));
}

Tuple vtx_get_song_tuple_from_vtx(const char * filename, ayemu_vtx_t * in)
{
    Tuple tuple;
    tuple.set_filename (filename);

    tuple.set_str (FIELD_ARTIST, in->hdr.author);
    tuple.set_str (FIELD_TITLE, in->hdr.title);

    tuple.set_int (FIELD_LENGTH, in->hdr.regdata_size / 14 * 1000 / 50);

    tuple.set_str (FIELD_GENRE, (in->hdr.chiptype == AYEMU_AY) ? "AY chiptunes" : "YM chiptunes");
    tuple.set_str (FIELD_ALBUM, in->hdr.from);

    tuple.set_str (FIELD_QUALITY, _("sequenced"));
    tuple.set_str (FIELD_CODEC, in->hdr.tracker);

    tuple.set_int (FIELD_YEAR, in->hdr.year);

    return tuple;
}

Tuple vtx_probe_for_tuple(const char *filename, VFSFile *fd)
{
    ayemu_vtx_t tmp;

    if (ayemu_vtx_open(&tmp, filename))
    {
        Tuple ti = vtx_get_song_tuple_from_vtx(filename, &tmp);
        ayemu_vtx_free(&tmp);
        return ti;
    }

    return Tuple ();
}

static bool vtx_play(const char * filename, VFSFile * file)
{
    gboolean eof = FALSE;
    void *stream;               /* pointer to current position in sound buffer */
    unsigned char regs[14];
    int need;
    int left;                   /* how many sound frames can play with current AY register frame */
    int donow;
    int rate;

    left = 0;
    rate = chans * (bits / 8);

    memset(&ay, 0, sizeof(ay));

    if (!ayemu_vtx_open(&vtx, filename))
    {
        g_print("libvtx: Error read vtx header from %s\n", filename);
        return FALSE;
    }
    else if (!ayemu_vtx_load_data(&vtx))
    {
        g_print("libvtx: Error read vtx data from %s\n", filename);
        return FALSE;
    }

    ayemu_init(&ay);
    ayemu_set_chip_type(&ay, vtx.hdr.chiptype, nullptr);
    ayemu_set_chip_freq(&ay, vtx.hdr.chipFreq);
    ayemu_set_stereo(&ay, (ayemu_stereo_t) vtx.hdr.stereo, nullptr);

    if (aud_input_open_audio(FMT_S16_NE, freq, chans) == 0)
    {
        g_print("libvtx: output audio error!\n");
        return FALSE;
    }

    aud_input_set_bitrate(14 * 50 * 8);

    while (!aud_input_check_stop() && !eof)
    {
        /* (time in sec) * 50 = offset in AY register data frames */
        int seek_value = aud_input_check_seek();
        if (seek_value >= 0)
            vtx.pos = seek_value / 20;

        /* fill sound buffer */
        stream = sndbuf;

        for (need = SNDBUFSIZE / rate; need > 0; need -= donow)
        {
            if (left > 0)
            {                   /* use current AY register frame */
                donow = (need > left) ? left : need;
                left -= donow;
                stream = ayemu_gen_sound(&ay, (char *)stream, donow * rate);
            }
            else
            {                   /* get next AY register frame */
                if (ayemu_vtx_get_next_frame(&vtx, (char *)regs) == 0)
                {
                    donow = need;
                    memset(stream, 0, donow * rate);
                    eof = TRUE;
                }
                else
                {
                    left = freq / vtx.hdr.playerFreq;
                    ayemu_set_regs(&ay, regs);
                    donow = 0;
                }
            }
        }

        aud_input_write_audio(sndbuf, SNDBUFSIZE);
    }

    ayemu_vtx_free(&vtx);

    return TRUE;
}

static const char vtx_about[] =
 N_("Vortex file format player by Sashnov Alexander <sashnov@ngs.ru>\n"
    "Based on in_vtx.dll by Roman Sherbakov <v_soft@microfor.ru>\n"
    "Audacious plugin by Pavel Vymetalek <pvymetalek@seznam.cz>");

#define AUD_PLUGIN_NAME        N_("VTX Decoder")
#define AUD_PLUGIN_ABOUT       vtx_about
#define AUD_INPUT_PLAY         vtx_play
#define AUD_INPUT_INFOWIN      vtx_file_info
#define AUD_INPUT_READ_TUPLE   vtx_probe_for_tuple
#define AUD_INPUT_IS_OUR_FILE  vtx_is_our_fd
#define AUD_INPUT_EXTS         vtx_fmts

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
