/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
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
 * along with this program;  If not, see <http://www.gnu.org/licenses>.
 */

#include <audacious/audconfig.h>

#include "draw-compat.h"
#include "ui_skin.h"
#include "ui_skinned_equalizer_graph.h"

static void init_spline (const gfloat * x, const gfloat * y, gint n, gfloat * y2)
{
    gint i, k;
    gfloat p, qn, sig, un, *u;

    u = (gfloat *) g_malloc(n * sizeof(gfloat));

    y2[0] = u[0] = 0.0;

    for (i = 1; i < n - 1; i++) {
        sig = ((gfloat) x[i] - x[i - 1]) / ((gfloat) x[i + 1] - x[i - 1]);
        p = sig * y2[i - 1] + 2.0;
        y2[i] = (sig - 1.0) / p;
        u[i] =
            (((gfloat) y[i + 1] - y[i]) / (x[i + 1] - x[i])) -
            (((gfloat) y[i] - y[i - 1]) / (x[i] - x[i - 1]));
        u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }
    qn = un = 0.0;

    y2[n - 1] = (un - qn * u[n - 2]) / (qn * y2[n - 2] + 1.0);
    for (k = n - 2; k >= 0; k--)
        y2[k] = y2[k] * y2[k + 1] + u[k];
    g_free(u);
}

gfloat eval_spline (const gfloat * xa, const gfloat * ya, const gfloat * y2a,
 gint n, gfloat x)
{
    gint klo, khi, k;
    gfloat h, b, a;

    klo = 0;
    khi = n - 1;
    while (khi - klo > 1) {
        k = (khi + klo) >> 1;
        if (xa[k] > x)
            khi = k;
        else
            klo = k;
    }
    h = xa[khi] - xa[klo];
    a = (xa[khi] - x) / h;
    b = (x - xa[klo]) / h;
    return (a * ya[klo] + b * ya[khi] +
            ((a * a * a - a) * y2a[klo] +
             (b * b * b - b) * y2a[khi]) * (h * h) / 6.0);
}

DRAW_FUNC_BEGIN (eq_graph_draw)
    static const gfloat x[10] = {0, 11, 23, 35, 47, 59, 71, 83, 97, 109};

    skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 294, 0, 0, 113, 19);
    skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 314, 0, 9 + (aud_cfg->equalizer_preamp
     * 9 + EQUALIZER_MAX_GAIN / 2) / EQUALIZER_MAX_GAIN, 113, 1);

    guint32 cols[19];
    skin_get_eq_spline_colors(active_skin, cols);

    gfloat yf[10];
    init_spline(x, aud_cfg->equalizer_bands, 10, yf);

    /* now draw a pixelated line with vector graphics ... -- jlindgren */
    gint py = 0;
    for (gint i = 0; i < 109; i ++)
    {
        gint y = 9.5 - eval_spline (x, aud_cfg->equalizer_bands, yf, 10, i) * 9 /
         EQUALIZER_MAX_GAIN;
        y = CLAMP (y, 0, 18);

        if (!i)
            py = y;

        gint ymin, ymax;

        if (y > py)
        {
            ymin = py + 1;
            ymax = y;
        }
        else if (y < py)
        {
            ymin = y;
            ymax = py - 1;
        }
        else
            ymin = ymax = y;

        py = y;

        for (y = ymin; y <= ymax; y++)
        {
            cairo_rectangle (cr, i + 2, y, 1, 1);
            set_cairo_color (cr, cols[y]);
            cairo_fill (cr);
        }
    }
DRAW_FUNC_END

GtkWidget * eq_graph_new (void)
{
    GtkWidget * graph = gtk_drawing_area_new ();
    gtk_widget_set_size_request (graph, 113, 19);
    g_signal_connect (graph, DRAW_SIGNAL, (GCallback) eq_graph_draw, NULL);
    return graph;
}

void eq_graph_update (GtkWidget * graph)
{
    gtk_widget_queue_draw (graph);
}
