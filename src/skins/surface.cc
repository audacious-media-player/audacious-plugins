/*
 * surface.c
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <gdk/gdk.h>

#include "surface.h"

#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui-gtk.h>

cairo_surface_t * surface_new (int w, int h)
{
    return cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
}

cairo_surface_t * surface_new_from_file (const char * name)
{
    GError * error = nullptr;
    AudguiPixbuf p (gdk_pixbuf_new_from_file (name, & error));

    if (error)
    {
        AUDERR ("Error loading %s: %s.\n", name, error->message);
        g_error_free (error);
    }

    if (! p)
        return nullptr;

    cairo_surface_t * surface = surface_new (p.width (), p.height ());
    cairo_t * cr = cairo_create (surface);

    gdk_cairo_set_source_pixbuf (cr, p.get (), 0, 0);
    cairo_paint (cr);

    cairo_destroy (cr);
    return surface;
}

uint32_t surface_get_pixel (cairo_surface_t * s, int x, int y)
{
    if (x < 0 || x >= cairo_image_surface_get_width (s) ||
     y < 0 || y >= cairo_image_surface_get_height (s))
        return 0;

    return * ((uint32_t *) (cairo_image_surface_get_data (s) +
     cairo_image_surface_get_stride (s) * y) + x) & 0xffffff;
}

void surface_copy_rect (cairo_surface_t * a, int ax, int ay, int w, int h,
 cairo_surface_t * b, int bx, int by)
{
    cairo_t * cr = cairo_create (b);
    cairo_set_source_surface (cr, a, bx - ax, by - ay);
    cairo_rectangle (cr, bx, by, w, h);
    cairo_fill (cr);
    cairo_destroy (cr);
}
