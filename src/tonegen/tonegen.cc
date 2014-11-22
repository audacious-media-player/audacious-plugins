/*
 *  Copyright 2000, 2001 Håvard Kvålen <havardk@sol.no>
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

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIN_FREQ        10
#define MAX_FREQ        20000
#define OUTPUT_FREQ     44100
#define BUF_SAMPLES     512
#define BUF_BYTES       (BUF_SAMPLES * sizeof(float))

#ifndef PI
#define PI              3.14159265358979323846
#endif

static bool tone_is_our_fd(const char *filename, VFSFile &fd)
{
    if (!strncmp(filename, "tone://", 7))
        return true;
    return false;
}

static Index<double> tone_filename_parse(const char *filename)
{
    Index<double> frequencies;

    if (strncmp(filename, "tone://", 7))
        return frequencies;

    auto strings = str_list_to_index(filename + 7, ";");

    for (const char *str : strings)
    {
        double freq = strtod(str, nullptr);
        if (freq >= MIN_FREQ && freq <= MAX_FREQ)
            frequencies.append(freq);
    }

    return frequencies;
}

static StringBuf tone_title(const char *filename)
{
    auto freqs = tone_filename_parse(filename);
    if (!freqs.len())
        return StringBuf();

    auto title = str_printf(_("%s %.1f Hz"), _("Tone Generator: "), freqs[0]);
    for (int i = 1; i < freqs.len(); i++)
        title.combine(str_printf(";%.1f Hz", freqs[i]));

    return title;
}

struct tone_t
{
    double wd;
    unsigned period, t;
};

static bool tone_play(const char *filename, VFSFile &file)
{
    float data[BUF_SAMPLES];

    auto frequencies = tone_filename_parse(filename);
    if (!frequencies.len())
        return false;

    aud_input_set_bitrate(16 * OUTPUT_FREQ);

    if (!aud_input_open_audio(FMT_FLOAT, OUTPUT_FREQ, 1))
        return false;

    Index<tone_t> tone;
    tone.resize(frequencies.len());
    for (int i = 0; i < frequencies.len(); i++)
    {
        double f = frequencies[i];
        tone[i].wd = 2 * PI * f / OUTPUT_FREQ;
        tone[i].period = (INT_MAX * 2U / OUTPUT_FREQ) * (OUTPUT_FREQ / f);
        tone[i].t = 0;
    }

    while (!aud_input_check_stop())
    {
        for (int i = 0; i < BUF_SAMPLES; i++)
        {
            double sum_sines = 0;

            for (int j = 0; j < frequencies.len(); j++)
            {
                sum_sines += sin(tone[j].wd * tone[j].t);
                if (tone[j].t > tone[j].period)
                    tone[j].t -= tone[j].period;
                tone[j].t++;
            }
            /* dithering can cause a little bit of clipping */
            data[i] = (sum_sines * 0.999 / (double)frequencies.len());
        }

        aud_input_write_audio(data, BUF_BYTES);
    }

    return true;
}

static Tuple tone_probe_for_tuple(const char *filename, VFSFile &fd)
{
    Tuple tuple;
    tuple.set_filename(filename);
    tuple.set_str(Tuple::Title, tone_title(filename));
    return tuple;
}

static const char tone_about[] =
 N_("Sine tone generator by Håvard Kvålen <havardk@xmms.org>\n"
    "Modified by Daniel J. Peng <danielpeng@bigfoot.com>\n\n"
    "To use it, add a URL: tone://frequency1;frequency2;frequency3;...\n"
    "e.g. tone://2000;2005 to play a 2000 Hz tone and a 2005 Hz tone");

static const char * const schemes[] = {"tone", nullptr};

#define AUD_PLUGIN_NAME        N_("Tone Generator")
#define AUD_PLUGIN_ABOUT       tone_about
#define AUD_INPUT_SCHEMES      schemes
#define AUD_INPUT_IS_OUR_FILE  tone_is_our_fd
#define AUD_INPUT_PLAY         tone_play
#define AUD_INPUT_READ_TUPLE   tone_probe_for_tuple

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
