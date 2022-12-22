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

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudgui/gtk-compat.h>
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
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginGLibOnly
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

void CairoSpectrum::render_freq (const float * freq)
{
    if (! bands)
        return;

    for (int i = 0; i < bands; i ++)
    {
        /* 40 dB range */
        int x = 40 + compute_freq_band (freq, xscale, i, bands);
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

static void draw_background (GtkWidget * area, cairo_t * cr)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation (area, & alloc);

    cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
    cairo_fill (cr);
}

static void draw_visualizer (GtkWidget *widget, cairo_t *cr)
{
    auto & c = (gtk_widget_get_style (widget))->base[GTK_STATE_SELECTED];

    for (int i = 0; i < bands; i++)
    {
        int x = ((width / bands) * i) + 2;
        float r, g, b;

        audgui_vis_bar_color (c, i, bands, r, g, b);
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
    Visualizer::compute_log_xscale (xscale, bands);

    return true;
}

#ifdef USE_GTK3
static gboolean draw_event (GtkWidget * widget, cairo_t * cr, GtkWidget * area)
{
    draw_background (widget, cr);
    draw_visualizer (widget, cr);

    return true;
}
#else
static gboolean draw_event (GtkWidget * widget)
{
    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));

    draw_background (widget, cr);
    draw_visualizer (widget, cr);

    cairo_destroy (cr);
    return true;
}
#endif

void * CairoSpectrum::get_gtk_widget ()
{
    GtkWidget *area = gtk_drawing_area_new();
    spect_widget = area;

    g_signal_connect(area, AUDGUI_DRAW_SIGNAL, (GCallback) draw_event, nullptr);
    g_signal_connect(area, "configure-event", (GCallback) configure_event, nullptr);
    g_signal_connect(area, "destroy", (GCallback) gtk_widget_destroyed, & spect_widget);

    GtkWidget * frame = gtk_frame_new (nullptr);
    gtk_frame_set_shadow_type ((GtkFrame *) frame, GTK_SHADOW_IN);
    gtk_container_add ((GtkContainer *) frame, area);
    return frame;
}
