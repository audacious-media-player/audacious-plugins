/*
 * Copyright (c) 2017-2019 Marc Sanchez Fauste.
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
#include <gtk/gtk.h>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <stdio.h>
#include <string.h>

#define MAX_CHANNELS 20
#define DB_RANGE 96

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
    "Copyright 2017-2019 Marc Sánchez Fauste");

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
static float legend_width;
static float vumeter_height;
static float vumeter_width;
static float vumeter_top_padding = 0;
static float vumeter_bottom_padding = 0;
static int nchannels = 2;
static float channels_db_level[MAX_CHANNELS];
static float channels_peaks[MAX_CHANNELS];
static gint64 last_peak_times[MAX_CHANNELS]; // Time elapsed since peak was set
static gint64 last_render_time = 0;

static float fclamp(float x, float low, float high)
{
    return fminf(fmaxf(x, low), high);
}

static float get_db_on_range(float db)
{
    return fclamp(db, -DB_RANGE, 0);
}

static float get_db_factor(float db)
{
    float factor = 0.0f;

    if (db < -DB_RANGE) {
        factor = 0.0f;
    } else if (db < -60.0f) {
        factor = (db + DB_RANGE) * 2.5f/(DB_RANGE-60);
    } else if (db < -50.0f) {
        factor = (db + 60.0f) * 0.5f + 2.5f;
    } else if (db < -40.0f) {
        factor = (db + 50.0f) * 0.75f + 7.5f;
    } else if (db < -30.0f) {
        factor = (db + 40.0f) * 1.5f + 15.0f;
    } else if (db < -20.0f) {
        factor = (db + 30.0f) * 2.0f + 30.0f;
    } else if (db < 0.0f) {
        factor = (db + 20.0f) * 2.5f + 50.0f;
    } else {
        factor = 100.0f;
    }

    return factor / 100.0f;
}

static float get_height_from_db(float db)
{
    return get_db_factor(db) * vumeter_height;
}

static float get_y_from_db(float db)
{
    return vumeter_top_padding + vumeter_height - get_height_from_db(db);
}

void VUMeter::render_multi_pcm (const float * pcm, int channels)
{
    gint64 current_time = g_get_monotonic_time();
    gint64 elapsed_render_time = current_time - last_render_time;
    last_render_time = current_time;
    nchannels = fclamp(channels, 0, MAX_CHANNELS);
    float falloff = aud_get_double ("vumeter", "falloff") / 1000000.0;
    gint64 peak_hold_time = aud_get_double ("vumeter", "peak_hold_time") * 1000000;

    float peaks[channels];
    for (int channel = 0; channel < channels; channel++)
    {
        peaks[channel] = fabsf(pcm[channel]);
    }

    for (int i = 0; i < 512 * channels;)
    {
        for (int channel = 0; channel < channels; channel++)
        {
            peaks[channel] = fmaxf(peaks[channel], fabsf(pcm[i++]));
        }
    }

    for (int i = 0; i < nchannels; i++)
    {
        float n = peaks[i];

        float db = 20 * log10f(n);
        db = get_db_on_range(db);

        channels_db_level[i] = get_db_on_range(channels_db_level[i] - elapsed_render_time * falloff);

        if (db > channels_db_level[i])
        {
            channels_db_level[i] = db;
        }
        gint64 elapsed_peak_time = current_time - last_peak_times[i];
        if (channels_db_level[i] > channels_peaks[i] || elapsed_peak_time > peak_hold_time) {
            channels_peaks[i] = channels_db_level[i];
            last_peak_times[i] = g_get_monotonic_time();
        }
    }

    if (spect_widget)
    {
        gtk_widget_queue_draw (spect_widget);
    }
}

static void initialize_variables()
{
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channels_db_level[i] = -DB_RANGE;
        channels_peaks[i] = -DB_RANGE;
    }
    memset (last_peak_times, 0, sizeof last_peak_times);
}

bool VUMeter::init ()
{
    initialize_variables();

    aud_config_set_defaults ("vumeter", prefs_defaults);
    return true;
}

void VUMeter::clear ()
{
    initialize_variables();

    if (spect_widget)
    {
        gtk_widget_queue_draw (spect_widget);
    }
}

static void draw_vu_legend_db(cairo_t * cr, float db, const char *text)
{
    float y = get_y_from_db(db);
    float padding = fclamp(legend_width * 0.25f, 0, 8) * 1.5f;
    cairo_text_extents_t extents;
    cairo_text_extents (cr, text, &extents);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_move_to(cr, legend_width - extents.width - padding, y + (extents.height/2));
    cairo_show_text (cr, text);
    cairo_move_to(cr, width - legend_width + padding, y + (extents.height/2));
    cairo_show_text (cr, text);
}

static void draw_vu_legend_line(cairo_t * cr, float db, float line_width_factor = 1)
{
    float y = get_y_from_db(db);
    float line_width = fclamp(legend_width * 0.25f, 0, 8);
    cairo_move_to(cr, legend_width - line_width * line_width_factor, y);
    cairo_line_to(cr, legend_width, y);
    cairo_stroke(cr);
    cairo_move_to(cr, width - legend_width, y);
    cairo_line_to(cr, width - legend_width + line_width * line_width_factor, y);
    cairo_stroke(cr);
}

static void draw_vu_legend(cairo_t * cr)
{
    float font_size_width = legend_width / 3.5f;
    float font_size_height = 2.5f * vumeter_height / DB_RANGE;
    cairo_set_font_size (cr, fmin(font_size_width, font_size_height));
    draw_vu_legend_db(cr, 0, "0");
    draw_vu_legend_db(cr, -3, "-3");
    draw_vu_legend_db(cr, -6, "-6");
    draw_vu_legend_db(cr, -9, "-9");
    draw_vu_legend_db(cr, -12, "-12");
    draw_vu_legend_db(cr, -15, "-15");
    draw_vu_legend_db(cr, -18, "-18");
    draw_vu_legend_db(cr, -20, "-20");
    draw_vu_legend_db(cr, -25, "-25");
    draw_vu_legend_db(cr, -30, "-30");
    draw_vu_legend_db(cr, -35, "-35");
    draw_vu_legend_db(cr, -40, "-40");
    draw_vu_legend_db(cr, -50, "-50");
    draw_vu_legend_db(cr, -60, "-60");
    draw_vu_legend_db(cr, -DB_RANGE, "-inf");

    cairo_set_line_width(cr, 0.1 * vumeter_height / DB_RANGE);
    cairo_set_source_rgb (cr, 120/255.0, 120/255.0, 120/255.0);
    for (int i = 0; i > -DB_RANGE; i--)
    {
        if (i > -30)
        {
            draw_vu_legend_line(cr, i);
            draw_vu_legend_line(cr, i - 0.5, 0.5);
        }
        else if (i > -40)
        {
            draw_vu_legend_line(cr, i);
        }
        else if (i > -60)
        {
            draw_vu_legend_line(cr, i);
            i -= 1;
        }
        else
        {
            draw_vu_legend_line(cr, i);
            i -= (DB_RANGE - 60) / 2;
        }
    }
    draw_vu_legend_line(cr, -DB_RANGE);
}

static void draw_background (GtkWidget * area, cairo_t * cr)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation (area, & alloc);

    cairo_set_source_rgba(cr, 40/255.0, 40/255.0, 40/255.0, 1.0);
    cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
    cairo_fill (cr);

    draw_vu_legend(cr);
}

static cairo_pattern_t* get_meter_pattern(float alpha = 1.0)
{
    cairo_pattern_t* meter_pattern = cairo_pattern_create_linear(0.0, 0.0, 0.0, vumeter_height);
    cairo_pattern_add_color_stop_rgba(meter_pattern, 1 - get_db_factor(0), 190/255.0, 40/255.0, 10/255.0, alpha);
    cairo_pattern_add_color_stop_rgba(meter_pattern, 1 - get_db_factor(-2), 190/255.0, 40/255.0, 10/255.0, alpha);
    cairo_pattern_add_color_stop_rgba(meter_pattern, 1 - get_db_factor(-9), 210/255.0, 210/255.0, 15/255.0, alpha);
    cairo_pattern_add_color_stop_rgba(meter_pattern, 1 - get_db_factor(-50), 0/255.0, 190/255.0, 20/255.0, alpha);
    return meter_pattern;
}

static void draw_visualizer (cairo_t *cr)
{
    cairo_pattern_t* meter_pattern = get_meter_pattern();
    cairo_pattern_t* meter_pattern_background = get_meter_pattern(0.1);

    for (int i = 0; i < nchannels; i++)
    {
        float x = legend_width + vumeter_width * i;
        float vumeter_padding = fclamp(vumeter_width * 0.02, 0, 2);
        if (i > 0)
        {
            x += vumeter_padding;
        }
        else
        {
            vumeter_padding = 0.0f;
        }

        cairo_set_source(cr, meter_pattern_background);
        cairo_rectangle (cr, x, vumeter_top_padding,
            vumeter_width - vumeter_padding, vumeter_height);
        cairo_fill (cr);

        cairo_set_source(cr, meter_pattern);
        cairo_rectangle (cr, x, get_y_from_db(channels_db_level[i]),
            vumeter_width - vumeter_padding, get_height_from_db(channels_db_level[i]));
        cairo_fill (cr);
        if (channels_peaks[i] > -DB_RANGE)
        {
            cairo_rectangle (cr, x, get_y_from_db(channels_peaks[i]),
                vumeter_width - vumeter_padding, (0.1f * vumeter_height / DB_RANGE));
            cairo_fill (cr);
        }
    }

    cairo_pattern_destroy(meter_pattern);
    cairo_pattern_destroy(meter_pattern_background);
}

static void format_db(char *buf, const float val) {
    if (val > -10)
    {
        sprintf(buf, "%+.1f", val);
    }
    else if (val > -DB_RANGE)
    {
        sprintf(buf, "%.0f ", val);
    }
    else
    {
        sprintf(buf, "-inf");
    }
}

static void draw_visualizer_peak_legend(cairo_t *cr)
{
    float font_size_width = vumeter_width / 3.0f;
    float font_size_height = vumeter_top_padding * 0.80f;
    cairo_set_font_size (cr, fmin(font_size_width, font_size_height));
    cairo_set_source_rgb (cr, 1, 1, 1);

    for (int i = 0; i < nchannels; i++)
    {
        char text[10];
        format_db(text, channels_peaks[i]);
        cairo_text_extents_t extents;
        cairo_text_extents (cr, text, &extents);
        cairo_move_to(cr, legend_width + vumeter_width*(i+0.5f) - (extents.width/2.0f),
            vumeter_top_padding/2.0f + (extents.height/2.0f));
        cairo_show_text (cr, text);
    }
}

static gboolean configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
    width = event->width;
    height = event->height;
    vumeter_top_padding = 0.04f * height;
    vumeter_bottom_padding = 0.015f * height;
    vumeter_height = event->height - vumeter_top_padding - vumeter_bottom_padding;
    legend_width = width * 0.3f;
    gtk_widget_queue_draw(widget);

    return true;
}

static gboolean draw_event (GtkWidget * widget)
{
    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));

    vumeter_width = width * 0.4f / nchannels;
    draw_background (widget, cr);
    draw_visualizer (cr);
    draw_visualizer_peak_legend(cr);

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
