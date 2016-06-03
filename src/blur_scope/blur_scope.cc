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

static void /* GtkWidget */ * bscope_get_color_chooser ();

static const PreferencesWidget bscope_widgets[] = {
    WidgetLabel (N_("<b>Color</b>")),
    WidgetCustomGTK (bscope_get_color_chooser)
};

static const PluginPreferences bscope_prefs = {{bscope_widgets}};

static const char * const bscope_defaults[] = {
 "color", aud::numeric_string<0xFF3F7F>::str,
 nullptr};

static int bscope_color;

class BlurScope : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Blur Scope"),
        PACKAGE,
        nullptr,
        & bscope_prefs,
        PluginGLibOnly
    };

    constexpr BlurScope () : VisPlugin (info, Visualizer::MonoPCM) {}

    bool init ();
    void cleanup ();

    void * get_gtk_widget ();

    void clear ();
    void render_mono_pcm (const float * pcm);

private:
    void resize (int w, int h);
    void draw ();

    void blur ();
    void draw_vert_line (int x, int y1, int y2);

    static gboolean configure_event (GtkWidget * widget, GdkEventConfigure * event, void * user);
    static gboolean expose_event (GtkWidget * widget, GdkEventExpose * event, void * user);

    GtkWidget * area = nullptr;
    int width = 0, height = 0, stride = 0, image_size = 0;
    uint32_t * image = nullptr, * corner = nullptr;
};

EXPORT BlurScope aud_plugin_instance;

bool BlurScope::init ()
{
    aud_config_set_defaults ("BlurScope", bscope_defaults);
    bscope_color = aud_get_int ("BlurScope", "color");

    return true;
}

void BlurScope::cleanup ()
{
    aud_set_int ("BlurScope", "color", bscope_color);

    g_free (image);
    image = nullptr;
}

void BlurScope::resize (int w, int h)
{
    width = w;
    height = h;
    stride = width + 2;
    image_size = (stride << 2) * (height + 2);
    image = (uint32_t *) g_realloc (image, image_size);
    memset (image, 0, image_size);
    corner = image + stride + 1;
}

void BlurScope::draw ()
{
    if (! area || ! gtk_widget_get_window (area))
        return;

    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (area));
    cairo_surface_t * surf = cairo_image_surface_create_for_data
     ((unsigned char *) image, CAIRO_FORMAT_RGB24, width, height, stride << 2);
    cairo_set_source_surface (cr, surf, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (surf);
    cairo_destroy (cr);
}

gboolean BlurScope::configure_event (GtkWidget * widget, GdkEventConfigure * event, void * user)
{
    ((BlurScope *) user)->resize (event->width, event->height);
    return true;
}

gboolean BlurScope::expose_event (GtkWidget * widget, GdkEventExpose * event, void * user)
{
    ((BlurScope *) user)->draw ();
    return true;
}

void * BlurScope::get_gtk_widget ()
{
    area = gtk_drawing_area_new ();

    g_signal_connect (area, "expose-event", (GCallback) expose_event, this);
    g_signal_connect (area, "configure-event", (GCallback) configure_event, this);
    g_signal_connect (area, "destroy", (GCallback) gtk_widget_destroyed, & area);

    GtkWidget * frame = gtk_frame_new (nullptr);
    gtk_frame_set_shadow_type ((GtkFrame *) frame, GTK_SHADOW_IN);
    gtk_container_add ((GtkContainer *) frame, area);
    return frame;
}

void BlurScope::clear ()
{
    memset (image, 0, image_size);
    draw ();
}

void BlurScope::blur ()
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

void BlurScope::draw_vert_line (int x, int y1, int y2)
{
    int y, h;

    if (y1 < y2) {y = y1 + 1; h = y2 - y1;}
    else if (y2 < y1) {y = y2; h = y1 - y2;}
    else {y = y1; h = 1;}

    uint32_t * p = corner + y * stride + x;

    for (; h --; p += stride)
        * p = bscope_color;
}

void BlurScope::render_mono_pcm (const float * pcm)
{
    blur ();

    int prev_y = (0.5 + pcm[0]) * height;
    prev_y = aud::clamp (prev_y, 0, height - 1);

    for (int i = 0; i < width; i ++)
    {
        int y = (0.5 + pcm[i * 512 / width]) * height;
        y = aud::clamp (y, 0, height - 1);
        draw_vert_line (i, prev_y, y);
        prev_y = y;
    }

    draw ();
}

static void color_set_cb (GtkWidget * chooser)
{
    GdkColor gdk_color;
    gtk_color_button_get_color ((GtkColorButton *) chooser, & gdk_color);
    bscope_color = ((gdk_color.red & 0xff00) << 8) | (gdk_color.green & 0xff00) | (gdk_color.blue >> 8);
}

static void /* GtkWidget */ * bscope_get_color_chooser ()
{
    GdkColor gdk_color = {0, (uint16_t) ((bscope_color & 0xff0000) >> 8),
     (uint16_t) (bscope_color & 0xff00), (uint16_t) ((bscope_color & 0xff) << 8)};
    GtkWidget * chooser = gtk_color_button_new_with_color (& gdk_color);
    gtk_color_button_set_use_alpha ((GtkColorButton *) chooser, false);

    g_signal_connect (chooser, "color-set", (GCallback) color_set_cb, nullptr);

    return chooser;
}
