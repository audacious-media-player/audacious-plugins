/*
 * Audacious
 * Copyright (c) 2006-2007 Audacious development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include <math.h>

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>

#include "skins_cfg.h"
#include "main.h"
#include "vis-callbacks.h"
#include "skin.h"
#include "button.h"
#include "hslider.h"
#include "number.h"
#include "playstatus.h"
#include "textbox.h"
#include "vis.h"
#include "util.h"

class VisCallbacks : public Visualizer
{
public:
    constexpr VisCallbacks () :
        Visualizer (MonoPCM | MultiPCM | Freq) {}

    void clear ();
    void render_mono_pcm (const float * pcm);
    void render_multi_pcm (const float * pcm, int channels);
    void render_freq (const float * freq);
};

void VisCallbacks::clear ()
{
    mainwin_vis->clear ();
    mainwin_svis->clear ();
}

void VisCallbacks::render_mono_pcm (const float * pcm)
{
    unsigned char data[512];

    if (config.vis_type != VIS_SCOPE)
        return;

    for (int i = 0; i < 75; i ++)
    {
        /* the signal is amplified by 2x */
        /* output values are in the range 0 to 16 */
        int val = 8 + roundf (16 * pcm[i * 512 / 75]);
        data[i] = aud::clamp (val, 0, 16);
    }

    if (aud_get_bool ("skins", "player_shaded"))
        mainwin_svis->render (data);
    else
        mainwin_vis->render (data);
}

/* calculate peak dB level, where 1 is 0 dB */
static float calc_peak_level (const float * pcm, int step)
{
    float peak = 0.0001; /* must be > 0, or level = -inf */

    for (int i = 0; i < 512; i ++)
    {
        peak = aud::max (peak, * pcm);
        pcm += step;
    }

    return 20 * log10 (peak);
}

void VisCallbacks::render_multi_pcm (const float * pcm, int channels)
{
    /* "VU meter" */
    if (config.vis_type != VIS_VOICEPRINT || ! aud_get_bool ("skins", "player_shaded"))
        return;

    unsigned char data[512];

    int level = 38 + calc_peak_level (pcm, channels);
    data[0] = aud::clamp (level, 0, 38);

    if (channels >= 2)
    {
        level = 38 + calc_peak_level (pcm + 1, channels);
        data[1] = aud::clamp (level, 0, 38);
    }
    else
        data[1] = data[0];

    mainwin_svis->render (data);
}

/* convert linear frequency graph to logarithmic one */
static void make_log_graph (const float * freq, int bands, int db_range,
 int int_range, unsigned char * graph)
{
    static int last_bands = 0;
    static Index<float> xscale;

    /* conversion table for the x-axis */
    if (bands != last_bands)
    {
        xscale.resize (bands + 1);
        for (int i = 0; i <= bands; i ++)
            xscale[i] = powf (256, (float) i / bands) - 0.5f;

        last_bands = bands;
    }

    for (int i = 0; i < bands; i ++)
    {
        /* sum up values in freq array between xscale[i] and xscale[i + 1],
           including fractional parts */
        int a = ceilf (xscale[i]);
        int b = floorf (xscale[i + 1]);
        float sum = 0;

        if (b < a)
            sum += freq[b] * (xscale[i + 1] - xscale[i]);
        else
        {
            if (a > 0)
                sum += freq[a - 1] * (a - xscale[i]);
            for (; a < b; a ++)
                sum += freq[a];
            if (b < 256)
                sum += freq[b] * (xscale[i + 1] - b);
        }

        /* fudge factor to make the graph have the same overall height as a
           12-band one no matter how many bands there are */
        sum *= (float) bands / 12;

        /* convert to dB */
        float val = 20 * log10f (sum);

        /* scale (-db_range, 0.0) to (0.0, int_range) */
        val = (1 + val / db_range) * int_range;

        graph[i] = aud::clamp ((int) val, 0, int_range);
    }
}

void VisCallbacks::render_freq (const float * freq)
{
    bool shaded = aud_get_bool ("skins", "player_shaded");

    unsigned char data[512];

    if (config.vis_type == VIS_ANALYZER)
    {
        if (config.analyzer_type == ANALYZER_BARS)
        {
            if (shaded)
                make_log_graph (freq, 13, 40, 8, data);
            else
                make_log_graph (freq, 19, 40, 16, data);
        }
        else
        {
            if (shaded)
                make_log_graph (freq, 37, 40, 8, data);
            else
                make_log_graph (freq, 75, 40, 16, data);
        }
    }
    else if (config.vis_type == VIS_VOICEPRINT && ! shaded)
        make_log_graph (freq, 17, 40, 255, data);
    else
        return;

    if (shaded)
        mainwin_svis->render (data);
    else
        mainwin_vis->render (data);
}

void start_stop_visual (bool exiting)
{
    static VisCallbacks callbacks;
    static bool started = false;

    if (config.vis_type != VIS_OFF && ! exiting && aud_ui_is_shown ())
    {
        if (! started)
        {
            aud_visualizer_add (& callbacks);
            started = true;
        }
    }
    else
    {
        if (started)
        {
            aud_visualizer_remove (& callbacks);
            started = false;
        }
    }
}
