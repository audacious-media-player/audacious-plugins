/*
 *  Blur Scope plugin for Audacious
 *  Copyright (C) 2010-2012 John Lindgren
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

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#define D_WIDTH 64
#define D_HEIGHT 32

static bool bscope_init (void);
static void bscope_cleanup(void);
static void bscope_clear (void);
static void bscope_render (const float * data);
static void /* GtkWidget */ * bscope_get_widget (void);
static void /* GtkWidget */ * bscope_get_color_chooser (void);

static const PreferencesWidget bscope_widgets[] = {
    WidgetLabel (N_("<b>Color</b>")),
    WidgetCustom (bscope_get_color_chooser)
};

static const PluginPreferences bscope_prefs = {{bscope_widgets}};

#define AUD_PLUGIN_NAME        N_("Blur Scope")
#define AUD_PLUGIN_PREFS       & bscope_prefs
#define AUD_PLUGIN_INIT        bscope_init
#define AUD_PLUGIN_CLEANUP     bscope_cleanup
#define AUD_VIS_CLEAR          bscope_clear
#define AUD_VIS_RENDER_MONO    bscope_render
#define AUD_VIS_GET_WIDGET     bscope_get_widget

#define AUD_DECLARE_VIS
#include <libaudcore/plugin-declare.h>

static GtkWidget * area = nullptr;
static int width, height, stride, image_size;
static uint32_t * image = nullptr, * corner = nullptr;

static const char * const bscope_defaults[] = {
 "color", "16727935", /* 0xFF3F7F */
 nullptr};

static int color;

static bool bscope_init (void)
{
    aud_config_set_defaults ("BlurScope", bscope_defaults);
    color = aud_get_int ("BlurScope", "color");

    return TRUE;
}

static void bscope_cleanup (void)
{
    aud_set_int ("BlurScope", "color", color);

    g_free (image);
    image = nullptr;
}

static void bscope_resize (int w, int h)
{
    width = w;
    height = h;
    stride = width + 2;
    image_size = (stride << 2) * (height + 2);
    image = (uint32_t *) g_realloc (image, image_size);
    memset (image, 0, image_size);
    corner = image + stride + 1;
}

static void bscope_draw_to_cairo (cairo_t * cr)
{
    cairo_surface_t * surf = cairo_image_surface_create_for_data ((unsigned char *)
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

static gboolean draw_cb (GtkWidget * widget, cairo_t * cr)
{
    bscope_draw_to_cairo (cr);
    return TRUE;
}

static void /* GtkWidget */ * bscope_get_widget (void)
{
    area = gtk_drawing_area_new ();
    gtk_widget_set_size_request (area, D_WIDTH, D_HEIGHT);
    bscope_resize (D_WIDTH, D_HEIGHT);

    g_signal_connect (area, "draw", (GCallback) draw_cb, nullptr);
    g_signal_connect (area, "configure-event", (GCallback) configure_event, nullptr);
    g_signal_connect (area, "destroy", (GCallback) gtk_widget_destroyed, & area);

    GtkWidget * frame = gtk_frame_new (nullptr);
    gtk_frame_set_shadow_type ((GtkFrame *) frame, GTK_SHADOW_IN);
    gtk_container_add ((GtkContainer *) frame, area);
    return frame;
}

static void bscope_clear (void)
{
    g_return_if_fail (image != nullptr);
    memset (image, 0, image_size);
    bscope_draw ();
}

static void bscope_blur (void)
{
    for (int y = 0; y < height; y ++)
    {
        uint32_t * p = corner + stride * y;
        uint32_t * end = p + width;
        uint32_t * plast = p - stride;
        uint32_t * pnext = p + stride;

        /* We do a quick and dirty average of four color values, first masking
         * off the lowest two bits.  Over a large area, this masking has the net
         * effect of subtracting 1.5 from each value, which by a happy chance
         * is just right for a gradual fade effect. */
        for (; p < end; p ++)
            * p = ((* plast ++ & 0xFCFCFC) + (p[-1] & 0xFCFCFC) + (p[1] &
             0xFCFCFC) + (* pnext ++ & 0xFCFCFC)) >> 2;
    }
}

static inline void draw_vert_line (int x, int y1, int y2)
{
    int y, h;

    if (y1 < y2) {y = y1 + 1; h = y2 - y1;}
    else if (y2 < y1) {y = y2; h = y1 - y2;}
    else {y = y1; h = 1;}

    uint32_t * p = corner + y * stride + x;

    for (; h --; p += stride)
        * p = color;
}

static void bscope_render (const float * data)
{
    bscope_blur ();

    int prev_y = (0.5 + data[0]) * height;
    prev_y = aud::clamp (prev_y, 0, height - 1);

    for (int i = 0; i < width; i ++)
    {
        int y = (0.5 + data[i * 512 / width]) * height;
        y = aud::clamp (y, 0, height - 1);
        draw_vert_line (i, prev_y, y);
        prev_y = y;
    }

    bscope_draw ();
}

static void color_set_cb (GtkWidget * chooser)
{
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba ((GtkColorChooser *) chooser, & rgba);

    int red = round (rgba.red * 255);
    int green = round (rgba.green * 255);
    int blue = round (rgba.blue * 255);
    color = (red << 16) | (green << 8) | blue;
}

static void /* GtkWidget */ * bscope_get_color_chooser (void)
{
    GdkRGBA rgba = {
        ((color & 0xff0000) >> 16) / 255.0,
        ((color & 0xff00) >> 8) / 255.0,
        (color & 0xff) / 255.0
    };

    GtkWidget * chooser = gtk_color_button_new_with_rgba (& rgba);
    gtk_color_chooser_set_use_alpha ((GtkColorChooser *) chooser, false);

    g_signal_connect (chooser, "color-set", (GCallback) color_set_cb, nullptr);

    return chooser;
}
