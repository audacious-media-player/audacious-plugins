/*
 *  Copyright 2000 Martin Strauss <mys@faveve.uni-stuttgart.de>
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

#include <stdio.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#define MIN_BPM         1
#define MAX_BPM         512
#define TACT_ID_MAX     12
#define TACT_FORM_MAX   8

#define AUDIO_FREQ      (44100)
#define BUF_SAMPLES     512
#define BUF_BYTES       (BUF_SAMPLES * 2)
#define MAX_AMPL        0x7fff

class Metronome : public InputPlugin
{
public:
    static const char about[];
    static const char *const schemes[];

    static constexpr PluginInfo info = {
        N_("Tact Generator"),
        PACKAGE,
        about
    };

    constexpr Metronome() : InputPlugin(info, InputInfo()
        .with_schemes(schemes)) {}

    bool is_our_file(const char *filename, VFSFile &);
    bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image);
    bool play(const char *filename, VFSFile &);
};

EXPORT Metronome aud_plugin_instance;

typedef struct
{
    int bpm;
    int num;
    int den;
    int id;
} metronom_t;

int tact_id[TACT_ID_MAX][2] = {
    {1, 1},
    {2, 2},
    {3, 2},
    {4, 2},
    {2, 4},
    {3, 4},
    {4, 4},
    {6, 4},
    {2, 8},
    {3, 8},
    {4, 8},
    {6, 8}
};

double tact_form[TACT_ID_MAX][TACT_FORM_MAX] = {
    {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.6, 0.5, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.6, 0.5, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.6, 0.5, 0.5, 0.0, 0.0},
    {1.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.6, 0.5, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.6, 0.5, 0.5, 0.0, 0.0}
};

bool Metronome::is_our_file(const char *filename, VFSFile &)
{
    if (!strncmp(filename, "tact://", 7))
        return true;
    return false;
}

static bool metronom_get_cp(const char *filename, metronom_t *pmetronom, String & str)
{
    int count;

    count = sscanf(filename, "tact://%d*%d/%d",
        &pmetronom->bpm, &pmetronom->num, &pmetronom->den);

    if (count != 1 && count != 3)
        return false;

    if (pmetronom->bpm < MIN_BPM || pmetronom->bpm > MAX_BPM)
        return false;

    if (count == 1)
    {
        pmetronom->num = 1;
        pmetronom->den = 1;
        pmetronom->id = 0;
    }
    else
    {
        bool flag;
        int id;

        if (pmetronom->num == 0 || pmetronom->den == 0)
            return false;

        flag = false;
        for (id = 0; id < TACT_ID_MAX && !flag; id++)
        {
            if (pmetronom->num == tact_id[id][0] && pmetronom->den == tact_id[id][1])
                flag = true;
        }

        if (!flag)
            return false;
        else
            pmetronom->id = id;
    }

    if (pmetronom->num == 1 && pmetronom->den == 1)
        str = String (str_printf (_("Tact generator: %d bpm"), pmetronom->bpm));
    else
        str = String (str_printf (_("Tact generator: %d bpm %d/%d"),
         pmetronom->bpm, pmetronom->num, pmetronom->den));

    return true;
}

bool Metronome::play (const char * filename, VFSFile &)
{
    metronom_t pmetronom;
    int16_t data[BUF_SAMPLES];
    int t = 0, tact, num;
    int datagoal = 0;
    int datamiddle = 0;
    int datacurrent = datamiddle;
    int datalast = datamiddle;
    int data_form[TACT_FORM_MAX];
    String desc;

    set_stream_bitrate(sizeof(data[0]) * 8 * AUDIO_FREQ);
    open_audio(FMT_S16_NE, AUDIO_FREQ, 1);

    if (!metronom_get_cp(filename, &pmetronom, desc))
    {
        AUDERR ("Invalid metronom tact parameters in URI %s", filename);
        return false;
    }

    tact = 60 * AUDIO_FREQ / pmetronom.bpm;

    /* prepare weighted amplitudes */
    for (num = 0; num < pmetronom.num; num++)
    {
        data_form[num] = MAX_AMPL * tact_form[pmetronom.id][num];
    }

    num = 0;
    while (!check_stop())
    {
        int i;

        for (i = 0; i < BUF_SAMPLES; i++)
        {
            if (t == tact)
            {
                t = 0;
                datagoal = data_form[num];
            }
            else if (t == 10)
            {
                datagoal = -data_form[num];
            }
            else if (t == 25)
            {
                datagoal = data_form[num];
                /* circle through weighted amplitudes */
                num++;
                if (num >= pmetronom.num)
                    num = 0;
            }
            /* makes curve a little bit smoother  */
            data[i] = (datalast + datacurrent + datagoal) / 3;
            datalast = datacurrent;
            datacurrent = data[i];
            if (t > 35)
                datagoal = (datamiddle + 7 * datagoal) / 8;
            t++;
        }

        write_audio(data, BUF_BYTES);
    }

    return true;
}

bool Metronome::read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image)
{
    metronom_t metronom;
    String desc;

    if (!metronom_get_cp(filename, &metronom, desc))
        return false;

    tuple.set_str(Tuple::Title, desc);
    return true;
}

const char Metronome::about[] =
 N_("A Tact Generator by Martin Strauss <mys@faveve.uni-stuttgart.de>\n\n"
    "To use it, add a URL: tact://beats*num/den\n"
    "e.g. tact://77 to play 77 beats per minute\n"
    "or tact://60*3/4 to play 60 bpm in 3/4 tacts");

const char *const Metronome::schemes[] = {"tact", nullptr};
