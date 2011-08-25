/*
 *  Blur Scope plugin for Audacious
 *  Copyright (C) 2010-2011 John Lindgren
 *
 *  Based on BMP - Cross-platform multimedia player:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

#include <gtk/gtk.h>
#include <string.h>

#include <audacious/gtk-compat.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "blur_scope.h"

#define D_WIDTH 64
#define D_HEIGHT 32

static gboolean bscope_init (void);
static void bscope_cleanup(void);
static void bscope_playback_stop(void);
static void bscope_render_pcm(gint16 data[2][512]);
static void /* GtkWidget */ * bscope_get_widget (void);

AUD_VIS_PLUGIN
(
    .name = "Blur Scope",                       /* description */
    .num_pcm_chs_wanted = 1, /* Number of PCM channels wanted */
    .num_freq_chs_wanted = 0, /* Number of freq channels wanted */
    .init = bscope_init,                /* init */
    .cleanup = bscope_cleanup,             /* cleanup */
    .configure = bscope_configure,           /* configure */
    .playback_stop = bscope_playback_stop,       /* playback_stop */
    .render_pcm = bscope_render_pcm,          /* render_pcm */
    .get_widget = bscope_get_widget,
)

static GtkWidget * area = NULL;
static gint width, height, stride, image_size;
static guint32 * image = NULL, * corner = NULL;

static const gchar * const bscope_defaults[] = {
 "color", "16727935", /* 0xFF3F7F */
 NULL};

static gboolean bscope_init (void)
{
    aud_config_set_defaults ("BlurScope", bscope_defaults);
    color = aud_get_int ("BlurScope", "color");

    return TRUE;
}

static void bscope_cleanup (void)
{
    aud_set_int ("BlurScope", "color", color);

    g_free (image);
    image = NULL;
}

static void bscope_resize (gint w, gint h)
{
    width = w;
    height = h;
    stride = width + 2;
    image_size = (stride << 2) * (height + 2);
    image = g_realloc (image, image_size);
    memset (image, 0, image_size);
    corner = image + stride + 1;
}

static void bscope_draw_to_cairo (cairo_t * cr)
{
    cairo_surface_t * surf = cairo_image_surface_create_for_data ((guchar *)
     image, CAIRO_FORMAT_RGB24, width, height, stride << 2);
    cairo_set_source_surface (cr, surf, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (surf);
}

static void bscope_draw (void)
{
    if (! area || ! gtk_widget_get_window (area))
        return;

    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (area));
    bscope_draw_to_cairo (cr);
    cairo_destroy (cr);
}

static gboolean configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
    bscope_resize (event->width, event->height);
    return TRUE;
}

#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean draw_cb (GtkWidget * widget, cairo_t * cr)
{
    bscope_draw_to_cairo (cr);
    return TRUE;
}
#else
static gboolean expose_event (GtkWidget * widget)
{
    bscope_draw ();
    return TRUE;
}
#endif

static void /* GtkWidget */ * bscope_get_widget (void)
{
    area = gtk_drawing_area_new ();
    gtk_widget_set_size_request (area, D_WIDTH, D_HEIGHT);
    bscope_resize (D_WIDTH, D_HEIGHT);

#if GTK_CHECK_VERSION (3, 0, 0)
    g_signal_connect (area, "draw", (GCallback) draw_cb, NULL);
#else
    g_signal_connect (area, "expose-event", (GCallback) expose_event, NULL);
#endif
    g_signal_connect (area, "configure-event", (GCallback) configure_event, NULL);
    g_signal_connect (area, "destroy", (GCallback) gtk_widget_destroyed, & area);

    return area;
}

static void bscope_playback_stop (void)
{
    g_return_if_fail (image != NULL);
    memset (image, 0, image_size);
    bscope_draw ();
}

static void bscope_blur (void)
{
    for (gint y = 0; y < height; y ++)
    {
        guint32 * p = corner + stride * y;
        guint32 * end = p + width;
        guint32 * plast = p - stride;
        guint32 * pnext = p + stride;

        /* We do a quick and dirty average of four color values, first masking
         * off the lowest two bits.  Over a large area, this masking has the net
         * effect of subtracting 1.5 from each value, which by a happy chance
         * is just right for a gradual fade effect. */
        for (; p < end; p ++)
            * p = ((* plast ++ & 0xFCFCFC) + (p[-1] & 0xFCFCFC) + (p[1] &
             0xFCFCFC) + (* pnext ++ & 0xFCFCFC)) >> 2;
    }
}

static inline void draw_vert_line (gint x, guint y1, gint y2)
{
    gint y, h;

    if (y1 < y2) {y = y1 + 1; h = y2 - y1;}
    else if (y2 < y1) {y = y2; h = y1 - y2;}
    else {y = y1; h = 1;}

    guint32 * p = corner + y * stride + x;

    for (; h --; p += stride)
        * p = color;
}

static void bscope_render_pcm (gint16 data[2][512])
{
    bscope_blur ();

    gint prev_y = ((32768 + (gint) data[0][0]) * height) >> 16;
    prev_y = CLAMP (prev_y, 0, height - 1);

    for (gint i = 0; i < width; i ++)
    {
        gint y = ((32768 + (gint) data[0][i * 512 / width]) * height) >> 16;
        y = CLAMP (y, 0, height - 1);
        draw_vert_line (i, prev_y, y);
        prev_y = y;
    }

    bscope_draw ();
}
