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
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#define MAX_BANDS   (256)
#define VIS_DELAY 2 /* delay before falloff in frames */
#define VIS_FALLOFF 2 /* falloff in pixels per frame */
#define DB_RANGE 65

class VUMeter : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("VU Meter"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginGLibOnly
    };

    constexpr VUMeter () : VisPlugin (info, Visualizer::MultiPCM) {}

    void * get_gtk_widget ();

    void clear ();
    void render_multi_pcm (const float * pcm, int channels);
};

EXPORT VUMeter aud_plugin_instance;

static GtkWidget * spect_widget = nullptr;
static int width, height;
static int bands = 4;
static int nchannels = 2;
static float bars[MAX_BANDS + 1];
static int delay[MAX_BANDS + 1];

static float fclamp(float x, float low, float high)
{
    return fmin (fmax (x, low), high);
}

void VUMeter::render_multi_pcm (const float * pcm, int channels)
{
    nchannels = channels;
    bands = channels + 2;

    float peaks[channels];
    for (int channel = 0; channel < channels; channel++)
    {
        peaks[channel] = pcm[channel];
    }

    for (int i = 0; i < 512 * channels;)
    {
        for (int channel = 0; channel < channels; channel++)
        {
            peaks[channel] = fmax(peaks[channel], fabs(pcm[i++]));
        }
    }

    for (int i = 0; i < channels; i ++)
    {
        float n = peaks[i];

        float x = DB_RANGE + 20 * log10f (n);
        x = fclamp (x, 0, DB_RANGE);

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

void VUMeter::clear ()
{
    memset (bars, 0, sizeof bars);
    memset (delay, 0, sizeof delay);

    if (spect_widget)
        gtk_widget_queue_draw (spect_widget);
}

static void draw_vu_legend_db(cairo_t * cr, int db, const char *text)
{
    cairo_text_extents_t extents;
    cairo_text_extents (cr, text, &extents);
    cairo_move_to(cr, (width / bands) - extents.width - 4, height - ((DB_RANGE - db) * height / DB_RANGE) + (extents.height/2));
    cairo_show_text (cr, text);
    cairo_move_to(cr, (width / bands) * (nchannels + 1) + 4, height - ((DB_RANGE - db) * height / DB_RANGE) + (extents.height/2));
    cairo_show_text (cr, text);
}

static void draw_vu_legend(cairo_t * cr)
{
    cairo_set_source_rgb (cr, 1, 1, 1);
    float font_size_width = (width / bands) / 2;
    float font_size_height = 2 * height / DB_RANGE;
    cairo_set_font_size (cr, fmin(font_size_width, font_size_height));
    draw_vu_legend_db(cr, 3, "-3");
    draw_vu_legend_db(cr, 5, "-5");
    draw_vu_legend_db(cr, 7, "-7");
    draw_vu_legend_db(cr, 9, "-9");
    draw_vu_legend_db(cr, 12, "-12");
    draw_vu_legend_db(cr, 15, "-15");
    draw_vu_legend_db(cr, 17, "-17");
    draw_vu_legend_db(cr, 20, "-20");
    draw_vu_legend_db(cr, 25, "-25");
    draw_vu_legend_db(cr, 30, "-30");
    draw_vu_legend_db(cr, 35, "-35");
    draw_vu_legend_db(cr, 40, "-40");
    draw_vu_legend_db(cr, 45, "-45");
    draw_vu_legend_db(cr, 50, "-50");
    draw_vu_legend_db(cr, 55, "-55");
    draw_vu_legend_db(cr, 60, "-60");
}

static void draw_background (GtkWidget * area, cairo_t * cr)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation (area, & alloc);

    cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
    cairo_fill (cr);

    draw_vu_legend(cr);
}

static void draw_visualizer (cairo_t *cr)
{
    for (int i = 0; i < nchannels; i++)
    {
        int x = ((width / bands) * (i+1)) + 2;
        float r=0, g=1, b=0;

        if (bars[i] > DB_RANGE - 3) {
            float size = fclamp (bars[i] - (DB_RANGE - 3), 0, 3) + 2;
            float barsdei = bars[i];
            cairo_set_source_rgb (cr, 1, 0, 0);
            cairo_rectangle (cr, x + 1, height - (barsdei * height / DB_RANGE),
             (width / bands) - 1, (size * height / DB_RANGE));
            cairo_fill (cr);
        }
        if (bars[i] >= DB_RANGE - 9) {
            float size = fclamp (bars[i] - (DB_RANGE - 9), 0, 9) + 2;
            float barsdei = fclamp (bars[i], 0, DB_RANGE - 3);
            cairo_set_source_rgb (cr, 1, 1, 0);
            cairo_rectangle (cr, x + 1, height - (barsdei * height / DB_RANGE),
             (width / bands) - 1, (size * height / DB_RANGE));
            cairo_fill (cr);
        }
        float size = fclamp (bars[i], 0, DB_RANGE - 10);
        cairo_set_source_rgb (cr, r, g, b);
        cairo_rectangle (cr, x + 1, height - (size * height / DB_RANGE),
         (width / bands) - 1, (size * height / DB_RANGE));
        cairo_fill (cr);
    }
}

static gboolean configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
    width = event->width;
    height = event->height;
    gtk_widget_queue_draw(widget);

    return true;
}

static gboolean draw_event (GtkWidget * widget)
{
    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));

    draw_background (widget, cr);
    draw_visualizer (cr);

    cairo_destroy (cr);
    return true;
}

void * VUMeter::get_gtk_widget ()
{
    GtkWidget *area = gtk_drawing_area_new();
    spect_widget = area;

    g_signal_connect(area, "expose-event", (GCallback) draw_event, nullptr);
    g_signal_connect(area, "configure-event", (GCallback) configure_event, nullptr);
    g_signal_connect(area, "destroy", (GCallback) gtk_widget_destroyed, & spect_widget);

    GtkWidget * frame = gtk_frame_new (nullptr);
    gtk_frame_set_shadow_type ((GtkFrame *) frame, GTK_SHADOW_IN);
    gtk_container_add ((GtkContainer *) frame, area);
    return frame;
}
