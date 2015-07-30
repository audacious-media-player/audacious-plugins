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

#include <libaudcore/equalizer.h>
#include <libaudcore/runtime.h>

#include "skins_cfg.h"
#include "skin.h"
#include "eq-graph.h"

#define N 10
static_assert (N == AUD_EQ_NBANDS, "only a 10-band EQ is supported");

static void init_spline (const double * x, const double * y, double * y2)
{
    int k;
    double p, qn, sig, un;
    double u[N];

    y2[0] = u[0] = 0.0;

    for (int i = 1; i < N - 1; i ++)
    {
        sig = ((double) x[i] - x[i - 1]) / ((double) x[i + 1] - x[i - 1]);
        p = sig * y2[i - 1] + 2.0;
        y2[i] = (sig - 1.0) / p;
        u[i] =
            (((double) y[i + 1] - y[i]) / (x[i + 1] - x[i])) -
            (((double) y[i] - y[i - 1]) / (x[i] - x[i - 1]));
        u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }
    qn = un = 0.0;

    y2[N - 1] = (un - qn * u[N - 2]) / (qn * y2[N - 2] + 1.0);
    for (k = N - 2; k >= 0; k --)
        y2[k] = y2[k] * y2[k + 1] + u[k];
}

static double eval_spline (const double * xa, const double * ya, const double * y2a, double x)
{
    int klo, khi, k;
    double h, b, a;

    klo = 0;
    khi = N - 1;
    while (khi - klo > 1)
    {
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

void EqGraph::draw (cairo_t * cr)
{
    static const double x[N] = {0, 11, 23, 35, 47, 59, 71, 83, 97, 109};

    if (cairo_image_surface_get_height (skin.pixmaps[SKIN_EQMAIN].get ()) < 313)
        return;

    skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 294, 0, 0, 113, 19);
    skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 314, 0, 9 + (aud_get_double (nullptr,
     "equalizer_preamp") * 9 + AUD_EQ_MAX_GAIN / 2) / AUD_EQ_MAX_GAIN, 113, 1);

    double bands[N];
    aud_eq_get_bands (bands);

    double yf[N];
    init_spline (x, bands, yf);

    int py = 0;
    for (int i = 0; i < 109; i ++)
    {
        int y = 9.5 - eval_spline (x, bands, yf, i) * 9 / AUD_EQ_MAX_GAIN;
        y = aud::clamp (y, 0, 18);

        if (!i)
            py = y;

        int ymin, ymax;

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

        for (y = ymin; y <= ymax; y ++)
        {
            cairo_rectangle (cr, i + 2, y, 1, 1);
            set_cairo_color (cr, skin.eq_spline_colors[y]);
            cairo_fill (cr);
        }
    }
}

EqGraph::EqGraph ()
{
    set_scale (config.scale);
    add_drawable (113, 19);
}
