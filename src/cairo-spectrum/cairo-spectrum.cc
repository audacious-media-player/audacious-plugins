/*
 * Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#define AUD_PLUGIN_GLIB_ONLY
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#define MAX_BANDS   (256)
#define VIS_DELAY 2 /* delay before falloff in frames */
#define VIS_FALLOFF 2 /* falloff in pixels per frame */

class CairoSpectrum : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Spectrum Analyzer"),
        PACKAGE
    };

    constexpr CairoSpectrum () : VisPlugin (info, Visualizer::Freq) {}

    void * get_gtk_widget ();

    void clear ();
    void render_freq (const float * freq);
};

EXPORT CairoSpectrum aud_plugin_instance;

static GtkWidget * spect_widget = nullptr;
static float xscale[MAX_BANDS + 1];
static int width, height, bands;
static int bars[MAX_BANDS + 1];
static int delay[MAX_BANDS + 1];

static void calculate_xscale ()
{
    for (int i = 0; i <= bands; i ++)
        xscale[i] = powf (256, (float) i / bands) - 0.5f;
}

void CairoSpectrum::render_freq (const float * freq)
{
    if (! bands)
        return;

    for (int i = 0; i < bands; i ++)
    {
        int a = ceilf (xscale[i]);
        int b = floorf (xscale[i + 1]);
        float n = 0;

        if (b < a)
            n += freq[b] * (xscale[i + 1] - xscale[i]);
        else
        {
            if (a > 0)
                n += freq[a - 1] * (a - xscale[i]);
            for (; a < b; a ++)
                n += freq[a];
            if (b < 256)
                n += freq[b] * (xscale[i + 1] - b);
        }

        /* fudge factor to make the graph have the same overall height as a
           12-band one no matter how many bands there are */
        n *= (float) bands / 12;

        /* 40 dB range */
        int x = 40 + 20 * log10f (n);
        x = aud::clamp (x, 0, 40);

        bars[i] -= aud::max (0, VIS_FALLOFF - delay[i]);

        if (delay[i])
            delay[i]--;

        if (x > bars[i])
        {
            bars[i] = x;
            delay[i] = VIS_DELAY;
        }
    }

    if (spect_widget)
        gtk_widget_queue_draw (spect_widget);
}

void CairoSpectrum::clear ()
{
    memset (bars, 0, sizeof bars);
    memset (delay, 0, sizeof delay);

    if (spect_widget)
        gtk_widget_queue_draw (spect_widget);
}

static void rgb_to_hsv (float r, float g, float b, float * h, float * s, float * v)
{
    float max, min;

    max = r;
    if (g > max)
        max = g;
    if (b > max)
        max = b;

    min = r;
    if (g < min)
        min = g;
    if (b < min)
        min = b;

    * v = max;

    if (max == min)
    {
        * h = 0;
        * s = 0;
        return;
    }

    if (r == max)
        * h = 1 + (g - b) / (max - min);
    else if (g == max)
        * h = 3 + (b - r) / (max - min);
    else
        * h = 5 + (r - g) / (max - min);

    * s = (max - min) / max;
}

static void hsv_to_rgb (float h, float s, float v, float * r, float * g, float * b)
{
    for (; h >= 2; h -= 2)
    {
        float * p = r;
        r = g;
        g = b;
        b = p;
    }

    if (h < 1)
    {
        * r = 1;
        * g = 0;
        * b = 1 - h;
    }
    else
    {
        * r = 1;
        * g = h - 1;
        * b = 0;
    }

    * r = v * (1 - s * (1 - * r));
    * g = v * (1 - s * (1 - * g));
    * b = v * (1 - s * (1 - * b));
}

static void get_color (gint i, gfloat * r, gfloat * g, gfloat * b)
{
    static GdkRGBA c;
    static gboolean valid = FALSE;
    gfloat h, s, v, n;

    if (! valid)
    {
        /* we want a color that matches the current theme
         * selected color of a GtkEntry should be reasonable */
        GtkStyleContext * style = gtk_style_context_new ();
        GtkWidgetPath * path = gtk_widget_path_new ();
        gtk_widget_path_append_type (path, GTK_TYPE_ENTRY);
        gtk_style_context_set_path (style, path);
        gtk_widget_path_free (path);
        gtk_style_context_add_class (style, "entry");
        gtk_style_context_get_background_color (style,
         (GtkStateFlags) (GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_SELECTED), & c);
        g_object_unref (style);
        valid = TRUE;
    }

    rgb_to_hsv (c.red, c.green, c.blue, & h, & s, & v);

    if (s < 0.1) /* monochrome theme? use blue instead */
    {
        h = 5;
        s = 0.75;
    }

    n = i / (gfloat) (bands - 1);
    s = 1 - 0.9 * n;
    v = 0.75 + 0.25 * n;

    hsv_to_rgb (h, s, v, r, g, b);
}

static void draw_background (GtkWidget * area, cairo_t * cr)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation (area, & alloc);

    cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
    cairo_fill (cr);
}

static void draw_visualizer (GtkWidget *widget, cairo_t *cr)
{
    for (int i = 0; i < bands; i++)
    {
        int x = ((width / bands) * i) + 2;
        float r, g, b;

        get_color (i, & r, & g, & b);
        cairo_set_source_rgb (cr, r, g, b);
        cairo_rectangle (cr, x + 1, height - (bars[i] * height / 40),
         (width / bands) - 1, (bars[i] * height / 40));
        cairo_fill (cr);
    }
}

static gboolean configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
    width = event->width;
    height = event->height;
    gtk_widget_queue_draw(widget);

    bands = width / 10;
    bands = aud::clamp(bands, 12, MAX_BANDS);
    calculate_xscale ();

    return TRUE;
}

static gboolean draw_event (GtkWidget * widget, cairo_t * cr, GtkWidget * area)
{
    draw_background (widget, cr);
    draw_visualizer (widget, cr);

    return TRUE;
}

void * CairoSpectrum::get_gtk_widget ()
{
    GtkWidget *area = gtk_drawing_area_new();
    spect_widget = area;

    g_signal_connect(area, "draw", (GCallback) draw_event, nullptr);
    g_signal_connect(area, "configure-event", (GCallback) configure_event, nullptr);
    g_signal_connect(area, "destroy", (GCallback) gtk_widget_destroyed, & spect_widget);

    GtkWidget * frame = gtk_frame_new (nullptr);
    gtk_frame_set_shadow_type ((GtkFrame *) frame, GTK_SHADOW_IN);
    gtk_container_add ((GtkContainer *) frame, area);
    return frame;
}
