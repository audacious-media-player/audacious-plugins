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

static const float vis_afalloff_speeds[] = {0.19, 0.422, 0.75, 1.0, 2.0};
static const float vis_pfalloff_speeds[] = {1.1, 1.15, 1.25, 1.4, 1.75};
static const int svis_analyzer_colors[] = {17, 14, 11, 8, 5};
static const int svis_scope_colors[] = {20, 19, 18, 19, 20};
static const int svis_vu_normal_colors[] = {17, 17, 17, 13, 13, 13, 2, 2};

#define RGB_SEEK(x,y) (set = rgb + 38 * (y) + (x))
#define RGB_SET(c) (* set ++ = (c))
#define RGB_SET_Y(c) do {* set = (c); set += 38;} while (0)
#define RGB_SET_INDEX(c) RGB_SET (skin.vis_colors[c])
#define RGB_SET_INDEX_Y(c) RGB_SET_Y (skin.vis_colors[c])

bool SmallVis::button_press (QMouseEvent * event)
{
    return press ? press (event) : false;
}

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
            if (bars && (x % 4) == 3)
                continue;

            int h = m_datafalloff[bars ? (x / 4) : x];
            h = aud::clamp (h, 0, 5);
            RGB_SEEK (x, 5 - h);

            for (int y = 0; y < h; y ++)
                RGB_SET_INDEX_Y (svis_analyzer_colors[h - 1 - y]);

        if (config.analyzer_peaks)
            {
                int h;
                h = m_truepeaks[bars ? (x / 4) : x];
                h = aud::clamp (h, 0, 16);

                if (h)
                {
                    RGB_SEEK (x, 5 - h);
                    RGB_SET_INDEX (23);
                }
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

                int h = (m_data[y / 3] * 8 + 19) / 38;
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

                int h = m_data[y / 3];
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
        static const int scale[17] = {0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 3, 4,
         4, 4, 4, 4, 4};

        if (! m_active)
            goto DRAW;

        switch (config.scope_mode)
        {
        case SCOPE_DOT:
            for (int x = 0; x < 38; x ++)
            {
                int h = scale[aud::clamp ((int)m_data[x], 0, 16)];
                RGB_SEEK (x, h);
                RGB_SET_INDEX (svis_scope_colors[2]);
            }
            break;
        case SCOPE_LINE:
        {
            for (int x = 0; x < 38; x ++)
            {
                int h = scale[aud::clamp ((int)m_data[x], 0, 16)];

                    if (x == 0) {
                        last_h = h;
                    }

                    top = h;
                    bottom = last_h;
                    last_h = h;

                    if (bottom < top) {
                        int temp = bottom;
                        bottom = top;
                        top = temp;
                    }

                for (int y = top; y <= bottom; y ++){
                    RGB_SEEK (x, y);
                    RGB_SET_INDEX_Y (svis_scope_colors[2]);
                }
            }
            break;
        }
        default: /* SCOPE_SOLID */
            for (int x = 0; x < 38; x ++)
            {
                int h = scale[aud::clamp ((int)m_data[x], 0, 16)];
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
                    RGB_SET_INDEX_Y (svis_scope_colors[2]);
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
    memset (m_datafalloff, 0, sizeof m_datafalloff);
    memset (m_truepeaks, 0, sizeof m_truepeaks);
    memset (m_data, 0, sizeof m_data);
    queue_draw ();
}

void SmallVis::render (const unsigned char * data)
{
    if (config.vis_type == VIS_ANALYZER)
    {
    const int n = (config.analyzer_type == ANALYZER_BARS) ? 10 : 38;
        for (int i = 0; i < n; i ++)
        {
            m_data[i] = data[i];
            m_data[i] = aud::clamp(static_cast<int>(m_data[i]), 0, 5);
            m_datafalloff[i] -= (vis_afalloff_speeds[config.analyzer_falloff]) * 2;

            if (m_datafalloff[i] <= m_data[i]) {
                m_datafalloff[i] = m_data[i];
            }

            if (m_peak[i] <= (int)((m_datafalloff[i] + 1) * 256)){
                m_peak[i] = (int)((m_datafalloff[i] + 1) * 256);
                m_data2[i] = 3.0f;
            }

            m_truepeaks[i] = m_peak[i]/256;

            m_peak[i] -= (int)m_data2[i];
            m_data2[i] *= (vis_pfalloff_speeds[config.peaks_falloff]);
        }
    }
    else if (config.vis_type == VIS_VOICEPRINT)
    {
        for (int i = 0; i < 2; i ++)
            m_data[i] = data[i];
    }
    else
    {
        for (int i = 0; i < 75; i ++)
            m_data[i] = data[i];
    }

    m_active = true;
    draw_now ();
}
