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

#include "drawing.h"
#include "skins_cfg.h"
#include "surface.h"
#include "ui_skin.h"
#include "ui_vis.h"

static const float vis_afalloff_speeds[] = {0.34, 0.5, 1.0, 1.3, 1.6};
static const float vis_pfalloff_speeds[] = {1.2, 1.3, 1.4, 1.5, 1.6};
static const int vis_scope_colors[16] = {22, 22, 21, 21, 20, 10, 19, 19, 18,
 19, 19, 20, 20, 21, 21, 22};

static uint32_t vis_voice_color[256];
static uint32_t vis_voice_color_fire[256];
static uint32_t vis_voice_color_ice[256];
static uint32_t pattern_fill[76 * 2];

static struct {
    gboolean active;
    float data[75], peak[75], peak_speed[75];
    unsigned char voiceprint_data[76 * 16];
    gboolean voiceprint_advance;
} vis;

#define RGB_SEEK(x,y) (set = rgb + 76 * (y) + (x))
#define RGB_SET(c) (* set ++ = (c))
#define RGB_SET_Y(c) do {* set = (c); set += 76;} while (0)
#define RGB_SET_INDEX(c) RGB_SET (active_skin->vis_colors[c])
#define RGB_SET_INDEX_Y(c) RGB_SET_Y (active_skin->vis_colors[c])

void ui_vis_set_colors (void)
{
    g_return_if_fail (active_skin != nullptr);

    uint32_t fgc = active_skin->colors[SKIN_TEXTFG];
    uint32_t bgc = active_skin->colors[SKIN_TEXTBG];
    int fg[3] = {COLOR_R (fgc), COLOR_G (fgc), COLOR_B (fgc)};
    int bg[3] = {COLOR_R (bgc), COLOR_G (bgc), COLOR_B (bgc)};

    for (int x = 0; x < 256; x ++)
    {
        unsigned char c[3];
        for (int n = 0; n < 3; n ++)
            c[n] = bg[n] + (fg[n] - bg[n]) * x / 255;
        vis_voice_color[x] = COLOR (c[0], c[1], c[2]);
    }

    for (int x = 0; x < 256; x ++)
    {
        unsigned char r = aud::min (x, 127) * 2;
        unsigned char g = aud::clamp (x - 64, 0, 127) * 2;
        unsigned char b = aud::max (x - 128, 0) * 2;
        vis_voice_color_fire[x] = COLOR (r, g, b);
    }

    for (int x = 0; x < 256; x ++)
        vis_voice_color_ice[x] = COLOR (x / 2, x, aud::min (x * 2, 255));

    uint32_t * set = pattern_fill;
    uint32_t * end = set + 76;

    while (set < end)
        RGB_SET_INDEX (0);

    end = set + 76;

    while (set < end)
    {
        RGB_SET_INDEX (1);
        RGB_SET_INDEX (0);
    }
}

DRAW_FUNC_BEGIN (ui_vis_draw, void)
    uint32_t rgb[76 * 16];
    uint32_t * set;

    if (config.vis_type != VIS_VOICEPRINT)
    {
        for (set = rgb; set < rgb + 76 * 16; set += 76 * 2)
            memcpy (set, pattern_fill, sizeof pattern_fill);
    }

    switch (config.vis_type)
    {
    case VIS_ANALYZER:
    {
        gboolean bars = (config.analyzer_type == ANALYZER_BARS);

        for (int x = 0; x < 75; x ++)
        {
            if (bars && (x & 3) == 3)
                continue;

            int h = vis.data[bars ? (x >> 2) : x];
            h = aud::clamp (h, 0, 16);
            RGB_SEEK (x, 16 - h);

            switch (config.analyzer_mode)
            {
            case ANALYZER_NORMAL:
                for (int y = 0; y < h; y ++)
                    RGB_SET_INDEX_Y (18 - h + y);
                break;
            case ANALYZER_FIRE:
                for (int y = 0; y < h; y ++)
                    RGB_SET_INDEX_Y (2 + y);
                break;
            default: /* ANALYZER_VLINES */
                for (int y = 0; y < h; y ++)
                    RGB_SET_INDEX_Y (18 - h);
                break;
            }

            if (config.analyzer_peaks)
            {
                int h = vis.peak[bars ? (x >> 2) : x];
                h = aud::clamp (h, 0, 16);

                if (h)
                {
                    RGB_SEEK (x, 16 - h);
                    RGB_SET_INDEX (23);
                }
            }
        }

        break;
    }
    case VIS_VOICEPRINT:
    {
        if (vis.voiceprint_advance)
        {
            vis.voiceprint_advance = FALSE;
            memmove (vis.voiceprint_data, vis.voiceprint_data + 1, sizeof
             vis.voiceprint_data - 1);

            for (int y = 0; y < 16; y ++)
                vis.voiceprint_data[76 * y + 75] = vis.data[y];
        }

        unsigned char * get = vis.voiceprint_data;
        uint32_t * colors = (config.voiceprint_mode == VOICEPRINT_NORMAL) ?
         vis_voice_color : (config.voiceprint_mode == VOICEPRINT_FIRE) ?
         vis_voice_color_fire : /* VOICEPRINT_ICE */ vis_voice_color_ice;
        set = rgb;

        for (int y = 0; y < 16; y ++)
        for (int x = 0; x < 76; x ++)
            RGB_SET (colors[* get ++]);
        break;
    }
    case VIS_SCOPE:
        if (! vis.active)
            goto DRAW;

        switch (config.scope_mode)
        {
        case SCOPE_DOT:
            for (int x = 0; x < 75; x ++)
            {
                int h = aud::clamp ((int) vis.data[x], 0, 15);
                RGB_SEEK (x, h);
                RGB_SET_INDEX (vis_scope_colors[h]);
            }
            break;
        case SCOPE_LINE:
        {
            for (int x = 0; x < 74; x++)
            {
                int h = aud::clamp ((int) vis.data[x], 0, 15);
                int h2 = aud::clamp ((int) vis.data[x + 1], 0, 15);

                if (h < h2) h2 --;
                else if (h > h2) {int temp = h; h = h2 + 1; h2 = temp;}

                RGB_SEEK (x, h);

                for (int y = h; y <= h2; y ++)
                    RGB_SET_INDEX_Y (vis_scope_colors[y]);
            }

            int h = aud::clamp ((int) vis.data[74], 0, 15);
            RGB_SEEK (74, h);
            RGB_SET_INDEX (vis_scope_colors[h]);
            break;
        }
        default: /* SCOPE_SOLID */
            for (int x = 0; x < 75; x++)
            {
                int h = aud::clamp ((int) vis.data[x], 0, 15);
                int h2;

                if (h < 8) h2 = 8;
                else {h2 = h; h = 8;}

                RGB_SEEK (x, h);

                for (int y = h; y <= h2; y ++)
                    RGB_SET_INDEX_Y (vis_scope_colors[y]);
            }
            break;
        }
        break;
    }

DRAW:;
    cairo_surface_t * surf = cairo_image_surface_create_for_data
     ((unsigned char *) rgb, CAIRO_FORMAT_RGB24, 76, 16, 4 * 76);
    cairo_scale (cr, config.scale, config.scale);
    cairo_set_source_surface (cr, surf, 0, 0);
    cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
    cairo_paint (cr);
    cairo_surface_destroy (surf);
DRAW_FUNC_END

GtkWidget * ui_vis_new (void)
{
    GtkWidget * wid = drawing_area_new ();
    gtk_widget_set_size_request (wid, 76 * config.scale, 16 * config.scale);
    DRAW_CONNECT (wid, ui_vis_draw, nullptr);
    return wid;
}

void ui_vis_clear_data (GtkWidget * wid)
{
    memset (& vis, 0, sizeof vis);
    gtk_widget_queue_draw (wid);
}

void ui_vis_timeout_func (GtkWidget * widget, unsigned char * data)
{
    if (config.vis_type == VIS_ANALYZER)
    {
        const int n = (config.analyzer_type == ANALYZER_BARS) ? 19 : 75;

        for (int i = 0; i < n; i++)
        {
            if (data[i] > vis.data[i])
            {
                vis.data[i] = data[i];
                if (vis.data[i] > vis.peak[i])
                {
                    vis.peak[i] = vis.data[i];
                    vis.peak_speed[i] = 0.01;

                }
                else if (vis.peak[i] > 0.0)
                {
                    vis.peak[i] -= vis.peak_speed[i];
                    vis.peak_speed[i] *=
                        vis_pfalloff_speeds[config.peaks_falloff];
                    if (vis.peak[i] < vis.data[i])
                        vis.peak[i] = vis.data[i];
                    if (vis.peak[i] < 0.0)
                        vis.peak[i] = 0.0;
                }
            }
            else
            {
                if (vis.data[i] > 0.0)
                {
                    vis.data[i] -=
                        vis_afalloff_speeds[config.analyzer_falloff];
                    if (vis.data[i] < 0.0)
                        vis.data[i] = 0.0;
                }
                if (vis.peak[i] > 0.0)
                {
                    vis.peak[i] -= vis.peak_speed[i];
                    vis.peak_speed[i] *=
                        vis_pfalloff_speeds[config.peaks_falloff];
                    if (vis.peak[i] < vis.data[i])
                        vis.peak[i] = vis.data[i];
                    if (vis.peak[i] < 0.0)
                        vis.peak[i] = 0.0;
                }
            }
        }
    }
    else if (config.vis_type == VIS_VOICEPRINT)
    {
        for (int i = 0; i < 16; i++)
            vis.data[i] = data[15 - i];

        vis.voiceprint_advance = TRUE;
    }
    else
    {
        for (int i = 0; i < 75; i++)
            vis.data[i] = data[i];
    }

    vis.active = TRUE;
    gtk_widget_queue_draw (widget);
}
