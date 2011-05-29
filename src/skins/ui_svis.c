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

#include "draw-compat.h"
#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_vis.h"

static gint svis_scope_colors[] = {20, 19, 18, 19, 20};
static gint svis_vu_normal_colors[] = {17, 17, 17, 12, 12, 12, 2, 2};

static guint32 svis_color[24];

static gint svis_data[75];

void ui_svis_set_colors (void)
{
    g_return_if_fail (aud_active_skin != NULL);

    for (gint i = 0; i < 24; i ++)
        svis_color[i] = (aud_active_skin->vis_color[i][0] << 16) |
         (aud_active_skin->vis_color[i][1] << 8) |
         aud_active_skin->vis_color[i][2];
}

#define RGB_SEEK(x,y) (set = rgb + 38 * (y) + (x))
#define RGB_SET(c) (* set ++ = (c))
#define RGB_SET_Y(c) do {* set = (c); set += 38;} while (0)
#define RGB_SET_INDEX(c) RGB_SET (svis_color[c])
#define RGB_SET_INDEX_Y(c) RGB_SET_Y (svis_color[c])

DRAW_FUNC_BEGIN (ui_svis_draw)
    guint32 rgb[38 * 5];
    guint32 * set;

    RGB_SEEK (0, 0);
    for (gint x = 0; x < 38 * 5; x ++)
        RGB_SET_INDEX (0);

    switch (config.vis_type)
    {
    case VIS_ANALYZER:;
        gboolean bars = (config.analyzer_type == ANALYZER_BARS);

        for (gint x = 0; x < 38; x ++)
        {
            if (bars && (x % 3) == 2)
                continue;

            gint h = svis_data[bars ? (x / 3) : x] / 2;
            h = CLAMP (h, 0, 5);
            RGB_SEEK (x, 5 - h);

            for (gint y = 0; y < h; y ++)
                RGB_SET_INDEX_Y (23);
        }

        break;
    case VIS_VOICEPRINT:
        switch (config.vu_mode)
        {
        case VU_NORMAL:
            for (gint y = 0; y < 5; y ++)
            {
                if (y == 2)
                    continue;

                gint h = (svis_data[y / 3] * 8 + 19) / 38;
                h = CLAMP (h, 0, 8);
                RGB_SEEK (0, y);

                for (gint x = 0; x < h; x ++)
                {
                    RGB_SET_INDEX (svis_vu_normal_colors[x]);
                    RGB_SET_INDEX (svis_vu_normal_colors[x]);
                    RGB_SET_INDEX (svis_vu_normal_colors[x]);
                    set += 2;
                }
            }
            break;
        default: /* VU_SMOOTH */
            for (gint y = 0; y < 5; y ++)
            {
                if (y == 2)
                    continue;

                gint h = svis_data[y / 3];
                h = CLAMP (h, 0, 38);
                RGB_SEEK (0, y);

                for (gint x = 0; x < h; x ++)
                    RGB_SET_INDEX (17 - (x * 16) / 38);
            }
            break;
        }
        break;
    case VIS_SCOPE:
        for (gint x = 0; x < 38; x ++)
        {
            gint h = svis_data[x << 1] / 3;
            h = CLAMP (h, 0, 4);
            RGB_SEEK (x, 4 - h);
            RGB_SET_INDEX (svis_scope_colors[h]);
        }
        break;
    }

    cairo_surface_t * surf = cairo_image_surface_create_for_data ((void *) rgb,
     CAIRO_FORMAT_RGB24, 38, 5, 4 * 38);
    cairo_set_source_surface (cr, surf, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (surf);
DRAW_FUNC_END

GtkWidget * ui_svis_new (void)
{
    GtkWidget * wid = gtk_drawing_area_new ();
    gtk_widget_set_size_request (wid, 38, 5);
    gtk_widget_add_events (wid, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect (wid, DRAW_SIGNAL, (GCallback) ui_svis_draw, NULL);
    return wid;
}

void ui_svis_clear_data (GtkWidget * widget)
{
    memset (svis_data, 0, sizeof svis_data);
    gtk_widget_queue_draw (widget);
}

void ui_svis_timeout_func (GtkWidget * widget, guchar * data)
{
    if (config.vis_type == VIS_VOICEPRINT)
    {
        for (gint i = 0; i < 2; i ++)
            svis_data[i] = data[i];
    }
    else
    {
        for (gint i = 0; i < 75; i ++)
            svis_data[i] = data[i];
    }

    gtk_widget_queue_draw (widget);
}
