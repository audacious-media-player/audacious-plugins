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

#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include "vtx.h"
#include "ayemu.h"

class VTXPlugin : public InputPlugin
{
public:
    static const char about[];
    static const char *const exts[];

    static constexpr PluginInfo info = {
        N_("VTX Decoder"),
        PACKAGE,
        about
    };

    constexpr VTXPlugin() : InputPlugin(info, InputInfo()
        .with_exts(exts)) {}

    bool is_our_file(const char *filename, VFSFile &file);
    bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image);
    bool play(const char *filename, VFSFile &file);

#ifdef USE_GTK
    bool file_info_box(const char *filename, VFSFile &file)
        { vtx_file_info(filename, file); return true; }
#endif
};

EXPORT VTXPlugin aud_plugin_instance;

#define SNDBUFSIZE 1024
static char sndbuf[SNDBUFSIZE];
static int freq = 44100;
static int chans = 2;
static int bits = 16;

const char *const VTXPlugin::exts[] = { "vtx", nullptr };

bool VTXPlugin::is_our_file(const char *filename, VFSFile &file)
{
    char buf[2];
    if (file.fread (buf, 1, 2) < 2)
        return false;
    return (!strcmp_nocase(buf, "ay", 2) || !strcmp_nocase(buf, "ym", 2));
}

bool VTXPlugin::read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image)
{
    ayemu_vtx_t tmp;

    if (!tmp.read_header(file))
        return false;

    tuple.set_str(Tuple::Artist, tmp.hdr.author);
    tuple.set_str(Tuple::Title, tmp.hdr.title);

    tuple.set_int(Tuple::Length, tmp.hdr.regdata_size / 14 * 1000 / 50);

    tuple.set_str(Tuple::Genre, (tmp.hdr.chiptype == AYEMU_AY) ? "AY chiptunes" : "YM chiptunes");
    tuple.set_str(Tuple::Album, tmp.hdr.from);

    tuple.set_str(Tuple::Quality, _("sequenced"));
    tuple.set_str(Tuple::Codec, tmp.hdr.tracker);

    tuple.set_int(Tuple::Year, tmp.hdr.year);

    return true;
}

bool VTXPlugin::play(const char *filename, VFSFile &file)
{
    ayemu_ay_t ay;
    ayemu_vtx_t vtx;

    bool eof = false;
    void *stream;               /* pointer to current position in sound buffer */
    unsigned char regs[14];
    int need;
    int left;                   /* how many sound frames can play with current AY register frame */
    int donow;
    int rate;

    left = 0;
    rate = chans * (bits / 8);

    memset(&ay, 0, sizeof(ay));

    if (!vtx.read_header(file))
    {
        AUDERR("Error read vtx header from %s\n", filename);
        return false;
    }
    else if (!vtx.load_data(file))
    {
        AUDERR("Error read vtx data from %s\n", filename);
        return false;
    }

    ayemu_init(&ay);
    ayemu_set_chip_type(&ay, vtx.hdr.chiptype, nullptr);
    ayemu_set_chip_freq(&ay, vtx.hdr.chipFreq);
    ayemu_set_stereo(&ay, (ayemu_stereo_t) vtx.hdr.stereo, nullptr);

    set_stream_bitrate(14 * 50 * 8);
    open_audio(FMT_S16_NE, freq, chans);

    while (!check_stop() && !eof)
    {
        /* (time in sec) * 50 = offset in AY register data frames */
        int seek_value = check_seek();
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
                if (!vtx.get_next_frame(regs))
                {
                    donow = need;
                    memset(stream, 0, donow * rate);
                    eof = true;
                }
                else
                {
                    left = freq / vtx.hdr.playerFreq;
                    ayemu_set_regs(&ay, regs);
                    donow = 0;
                }
            }
        }

        write_audio(sndbuf, SNDBUFSIZE);
    }

    return true;
}

const char VTXPlugin::about[] =
 N_("Vortex file format player by Sashnov Alexander <sashnov@ngs.ru>\n"
    "Based on in_vtx.dll by Roman Sherbakov <v_soft@microfor.ru>\n"
    "Audacious plugin by Pavel Vymetalek <pvymetalek@seznam.cz>");
