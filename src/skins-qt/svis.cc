/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007-2011  Audacious development team.
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
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

#include <string.h>
#include <libaudcore/objects.h>

#include "skins_cfg.h"
#include "skin.h"
#include "vis.h"

static const float vis_afalloff_speeds[] = {0.34, 0.5, 1.0, 1.3, 1.6};
static const float vis_pfalloff_speeds[] = {1.2, 1.3, 1.4, 1.5, 1.6};
static const int svis_analyzer_colors[] = {17, 14, 11, 8, 4};
static const int svis_vu_normal_colors[] = {16, 14, 12, 10, 8, 6, 4, 2};

#define RGB_SEEK(x,y) (set = rgb + 38 * (y) + (x))
#define RGB_SET(c) (* set ++ = (c))
#define RGB_SET_Y(c) do {* set = (c); set += 38;} while (0)
#define RGB_SET_INDEX(c) RGB_SET (skin.vis_colors[c])
#define RGB_SET_INDEX_Y(c) RGB_SET_Y (skin.vis_colors[c])

void SmallVis::draw (QPainter & cr)
{
    uint32_t rgb[38 * 5];
    uint32_t * set;

    RGB_SEEK (0, 0);
    for (int x = 0; x < 38 * 5; x ++)
        RGB_SET_INDEX (0);

    switch (config.vis_type)
    {
    case VIS_ANALYZER:
    {
        bool bars = (config.analyzer_type == ANALYZER_BARS);

        for (int x = 0; x < 37; x ++)
        {
            if (bars && (x % 3) == 2)
                continue;

            int h = m_falloff[bars ? (x / 3) : x];
            h = aud::clamp (h, 0, 5);
            RGB_SEEK (x, 5 - h);

            switch (config.analyzer_mode)
            {
                case ANALYZER_NORMAL:
                    for (int y = 0; y < h; y ++)
                        RGB_SET_INDEX_Y (svis_analyzer_colors[h - 1 - y]);
                break;
                case ANALYZER_FIRE:
                    for (int y = 0; y < h; y ++)
                        RGB_SET_INDEX_Y (svis_analyzer_colors[4-y]);
                break;
                default: /* ANALYZER_VLINES */
                    for (int y = 0; y < h; y ++)
                        RGB_SET_INDEX_Y (svis_analyzer_colors[h-1]);
                break;
            }

            if (config.analyzer_peaks)
            {
                int h = m_peak[bars ? (x / 3) : x];
                h = aud::clamp (h, 0, 5);

                if (h >= 5)
                    continue;
                RGB_SEEK (x, 4 - h);
                RGB_SET_INDEX (23);

            }
        }

        break;
    }
    case VIS_VOICEPRINT:
        switch (config.vu_mode)
        {
        case VU_NORMAL:
            for (int y = 0; y < 5; y ++)
            {
                if (y == 2)
                    continue;

                int h = (m_falloff[y / 3] * 8 + 19) / 38;
                h = aud::clamp (h, 0, 8);
                RGB_SEEK (0, y);

                for (int x = 0; x < h; x ++)
                {
                    RGB_SET_INDEX (svis_vu_normal_colors[x]);
                    RGB_SET_INDEX (svis_vu_normal_colors[x]);
                    RGB_SET_INDEX (svis_vu_normal_colors[x]);
                    set += 2;
                }
            }
            break;
        default: /* VU_SMOOTH */
            for (int y = 0; y < 5; y ++)
            {
                if (y == 2)
                    continue;

                int h = m_falloff[y / 3];
                h = aud::clamp (h, 0, 38);
                RGB_SEEK (0, y);

                for (int x = 0; x < h; x ++)
                    RGB_SET_INDEX (17 - (x * 16) / 38);
            }
            break;
        }
        break;
    case VIS_SCOPE:
    {
        static const int scale[17] = {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3,
         3, 3, 3, 4, 4};

        if (! m_active)
            goto DRAW;

        switch (config.scope_mode)
        {
        case SCOPE_DOT:
            for (int x = 0; x < 38; x ++)
            {
                int h = scale[aud::clamp ((int) m_data[2 * x], 0, 16)];
                RGB_SEEK (x, h);
                RGB_SET_INDEX (18);
            }
            break;
        case SCOPE_LINE:
        {
            for (int x = 0; x < 37; x ++)
            {
                int h = scale[aud::clamp ((int) m_data[2 * x], 0, 16)];
                int h2 = scale[aud::clamp ((int) m_data[2 * (x + 1)], 0, 16)];

                if (h > h2) {int temp = h; h = h2; h2 = temp;}

                RGB_SEEK (x, h);
                for (int y = h; y <= h2; y ++)
                    RGB_SET_INDEX_Y (18);
            }

            int h = scale[aud::clamp ((int) m_data[74], 0, 16)];
            RGB_SEEK (37, h);
            RGB_SET_INDEX (18);
            break;
        }
        default: /* SCOPE_SOLID */
            for (int x = 0; x < 38; x ++)
            {
                int h = scale[aud::clamp ((int) m_data[2 * x], 0, 16)];
                int h2;

                if (h < 2)
                    h2 = 2;
                else
                {
                    h2 = h;
                    h = 2;
                }

                RGB_SEEK (x, h);
                for (int y = h; y <= h2; y ++)
                    RGB_SET_INDEX_Y (18);
            }
            break;
        }
        break;
    }
    }

DRAW:
    QImage image ((unsigned char *) rgb, 38, 5, 4 * 38, QImage::Format_RGB32);
    cr.drawImage (0, 0, image);
}

SmallVis::SmallVis ()
{
    set_scale (config.scale);
    add_drawable (38, 5);
    clear ();
}

void SmallVis::clear ()
{
    m_active = false;
    memset (m_data, 0, sizeof m_data);
    memset (m_falloff, 0, sizeof m_falloff);
    memset (m_peak, 0, sizeof m_peak);
    memset (m_peak_speed, 0, sizeof m_peak_speed);
    queue_draw ();
}

void SmallVis::render (const unsigned char * data)
{
    if (config.vis_type == VIS_ANALYZER)
    {
        const int n = (config.analyzer_type == ANALYZER_BARS) ? 19 : 75;

        for (int i = 0; i < n; i ++)
        {
            m_data[i] = data[i];

            if (m_falloff[i] >= 5.0){
                m_falloff[i] = 5.0;
            }

            m_falloff[i] -= vis_afalloff_speeds[config.analyzer_falloff] / 4;
            if (m_falloff[i] <= m_data[i]){
                m_falloff[i] = m_data[i];
            }
            if (m_falloff[i] < 0.0)
                m_falloff[i] = 0.0;

            if (m_falloff[i] > m_peak[i])
            {
                m_peak[i] = m_falloff[i];
                m_peak_speed[i] = 0.01;

            }

            {
                m_peak[i] -= m_peak_speed[i] / 4;
                m_peak_speed[i] *= vis_pfalloff_speeds[config.peaks_falloff];
                if (m_peak[i] <= m_falloff[i])
                    m_peak[i] = m_falloff[i];
                if (m_peak[i] < 0.0)
                    m_peak[i] = 0.0;
            }
        }
    }
    else if (config.vis_type == VIS_VOICEPRINT)
    {
        for (int i = 0; i < 2; i ++) {
            m_data[i] = data[i];

            m_falloff[i] -= vis_afalloff_speeds[config.analyzer_falloff];
            if (m_falloff[i] <= m_data[i]){
                m_falloff[i] = m_data[i];
            }
            if (m_falloff[i] < 0.0)
                m_falloff[i] = 0.0;
        }
    }
    else
    {
        for (int i = 0; i < 75; i ++)
            m_data[i] = data[i];
    }

    m_active = true;
    draw_now ();
}
