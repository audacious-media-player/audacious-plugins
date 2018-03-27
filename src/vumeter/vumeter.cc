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
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#define MAX_BANDS   (256)
#define DB_RANGE 65

class VUMeter : public VisPlugin
{
public:
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;
    static const char * const prefs_defaults[];

    static constexpr PluginInfo info = {
        N_("VU Meter"),
        PACKAGE,
        about,
        & prefs,
        PluginGLibOnly
    };

    constexpr VUMeter () : VisPlugin (info, Visualizer::MultiPCM) {}

    bool init ();

    void * get_gtk_widget ();

    void clear ();
    void render_multi_pcm (const float * pcm, int channels);
};

EXPORT VUMeter aud_plugin_instance;

const char VUMeter::about[] =
 N_("VU Meter Plugin for Audacious\n"
    "Copyright 2017-2018 Marc Sánchez");

const PreferencesWidget VUMeter::widgets[] = {
    WidgetLabel (N_("<b>VU Meter Settings</b>")),
    WidgetSpin (
        N_("Peak hold time:"),
        WidgetFloat ("vumeter", "peak_hold_time"),
        {0.1, 30, 0.1, N_("seconds")}
    ),
    WidgetSpin (
        N_("Fall-off time:"),
        WidgetFloat ("vumeter", "falloff"),
        {0.1, 96, 0.1, N_("dB/second")}
    )
};

const PluginPreferences VUMeter::prefs = {{widgets}};

const char * const VUMeter::prefs_defaults[] = {
    "peak_hold_time", "1.6",
    "falloff", "13.3",
    nullptr
};

static GtkWidget * spect_widget = nullptr;
static int width, height;
static int bands = 4;
static int nchannels = 2;
static float bars[MAX_BANDS + 1];
static float peak[MAX_BANDS + 1];
static gint64 last_peak_times[MAX_BANDS + 1]; // Time elapsed since peak was set
static gint64 last_render_time = 0;

static float fclamp(float x, float low, float high)
{
    return fminf (fmaxf (x, low), high);
}

void VUMeter::render_multi_pcm (const float * pcm, int channels)
{
    gint64 current_time = g_get_monotonic_time();
    gint64 elapsed_render_time = current_time - last_render_time;
    last_render_time = current_time;
    nchannels = channels;
    bands = channels + 2;
    float falloff = aud_get_double ("vumeter", "falloff") / 1000000.0;
    gint64 peak_hold_time = aud_get_double ("vumeter", "peak_hold_time") * 1000000;

    float peaks[channels];
    for (int channel = 0; channel < channels; channel++)
    {
        peaks[channel] = pcm[channel];
    }

    for (int i = 0; i < 512 * channels;)
    {
        for (int channel = 0; channel < channels; channel++)
        {
            peaks[channel] = fmaxf(peaks[channel], fabsf(pcm[i++]));
        }
    }

    for (int i = 0; i < channels; i ++)
    {
        float n = peaks[i];

        float x = DB_RANGE + 20 * log10f (n);
        x = fclamp (x, 0, DB_RANGE);

        bars[i] = fclamp(bars[i] - elapsed_render_time * falloff, 0, DB_RANGE);

        if (x > bars[i])
        {
            bars[i] = x;
        }
        gint64 elapsed_peak_time = current_time - last_peak_times[i];
        if (x > peak[i] || elapsed_peak_time > peak_hold_time) {
            peak[i] = x;
            last_peak_times[i] = g_get_monotonic_time();
        }
    }

    if (spect_widget)
        gtk_widget_queue_draw (spect_widget);
}

bool VUMeter::init ()
{
    aud_config_set_defaults ("vumeter", prefs_defaults);
    return true;
}

void VUMeter::clear ()
{
    memset (bars, 0, sizeof bars);
    memset (peak, 0, sizeof peak);
    memset (last_peak_times, 0, sizeof last_peak_times);

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
    draw_vu_legend_db(cr, 1, "-1");
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

        cairo_set_source_rgba (cr, 1, 0, 0, 0.2);
        cairo_rectangle (cr, x + 1, 0, (width / bands) - 1,
         (3 * height / DB_RANGE) + 1);
        cairo_fill (cr);
        if (bars[i] > DB_RANGE - 3) {
            float size = fclamp (bars[i] - (DB_RANGE - 3), 0, 3) + 2;
            float barsdei = bars[i];
            cairo_set_source_rgb (cr, 1, 0, 0);
            cairo_rectangle (cr, x + 1, height - (barsdei * height / DB_RANGE),
             (width / bands) - 1, (size * height / DB_RANGE));
            cairo_fill (cr);
        }
        cairo_set_source_rgba (cr, 1, 1, 0, 0.2);
        cairo_rectangle (cr, x + 1, height - ((DB_RANGE - 3) * height / DB_RANGE),
         (width / bands) - 1, (6 * height / DB_RANGE));
        cairo_fill (cr);
        if (bars[i] >= DB_RANGE - 9) {
            float size = fclamp (bars[i] - (DB_RANGE - 9), 0, 6) + 2;
            float barsdei = fclamp (bars[i], 0, DB_RANGE - 3);
            cairo_set_source_rgb (cr, 1, 1, 0);
            cairo_rectangle (cr, x + 1, height - (barsdei * height / DB_RANGE),
             (width / bands) - 1, (size * height / DB_RANGE));
            cairo_fill (cr);
        }
        cairo_set_source_rgba (cr, r, g, b, 0.2);
        cairo_rectangle (cr, x + 1, height - ((DB_RANGE - 9) * height / DB_RANGE),
         (width / bands) - 1, ((DB_RANGE - 9) * height / DB_RANGE));
        cairo_fill (cr);
        float size = fclamp (bars[i], 0, DB_RANGE - 9);
        cairo_set_source_rgb (cr, r, g, b);
        cairo_rectangle (cr, x + 1, height - (size * height / DB_RANGE),
         (width / bands) - 1, (size * height / DB_RANGE));
        cairo_fill (cr);
        if (peak[i] > DB_RANGE - 3) {
            cairo_set_source_rgb (cr, 1, 0, 0);
        } else if (peak[i] >= DB_RANGE - 9) {
            cairo_set_source_rgb (cr, 1, 1, 0);
        }
        cairo_rectangle (cr, x + 1, height - (peak[i] * height / DB_RANGE),
         (width / bands) - 1, (0.1 * height / DB_RANGE));
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
