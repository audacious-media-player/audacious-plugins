/*
 *  Copyright (C) 2010 William Pitcock <nenolod@atheme.org>.
 *  Copyright (C) 2010 John Lindgren <john.lindgren@tds.net>.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <math.h>

#include "gtkui_cfg.h"
#include "ui_gtk.h"
#include "ui_playlist_widget.h"
#include "ui_playlist_model.h"
#include "ui_manager.h"
#include "ui_infoarea.h"

#define DEFAULT_ARTWORK DATA_DIR "/images/audio.png"
#define STREAM_ARTWORK DATA_DIR "/images/streambrowser-64x64.png"
#define ICON_SIZE 64
#define SPECT_BANDS 12
#define VIS_OFFSET (10 + 12 * SPECT_BANDS + 7)

#if ! GTK_CHECK_VERSION (2, 18, 0)
#define gtk_widget_get_allocation(w, ap) (* (ap) = (w)->allocation)
#endif

typedef struct {
    GtkWidget *parent;

    gchar * title, * artist, * album;
    gchar * last_title, * last_artist, * last_album;
    gfloat alpha, last_alpha;

    gboolean stopped;
    gint fade_timeout;
    guint8 visdata[SPECT_BANDS];

    GdkPixbuf * pb, * last_pb;
} UIInfoArea;

static const gfloat alpha_step = 0.10;

static void ui_infoarea_draw_visualizer (UIInfoArea * area);

/****************************************************************************/

static void ui_infoarea_visualization_timeout (const VisNode * vis, UIInfoArea *
 area)
{
    const gfloat xscale[SPECT_BANDS + 1] = {0.00, 0.59, 1.52, 3.00, 5.36, 9.10,
     15.0, 24.5, 39.4, 63.2, 101, 161, 256}; /* logarithmic scale - 1 */
    gint16 mono_freq[2][256];

    aud_calc_mono_freq(mono_freq, vis->data, vis->nch);

    for (gint i = 0; i < SPECT_BANDS; i ++)
    {
        gint a = ceil (xscale[i]);
        gint b = floor (xscale[i + 1]);
        gint n = 0;

        if (b < a)
            n += mono_freq[0][b] * (xscale[i + 1] - xscale[i]);
        else
        {
            if (a > 0)
                n += mono_freq[0][a - 1] * (a - xscale[i]);
            for (; a < b; a ++)
                n += mono_freq[0][a];
            if (b < 256)
                n += mono_freq[0][b] * (xscale[i + 1] - b);
        }

        /* 40 dB range */
        /* 0.00305 == 1 / 32767 * 10^(40/20) */
        n = 32 * log10 (n * 0.00305);
        n = CLAMP (n, 0, 64);
        area->visdata[i] = MAX (area->visdata[i] - 3, n);
    }

#if GTK_CHECK_VERSION (2, 20, 0)
    if (gtk_widget_is_drawable (area->parent))
#else
    if (GTK_WIDGET_DRAWABLE (area->parent))
#endif
        ui_infoarea_draw_visualizer (area);
}

static void vis_clear_cb (void * hook_data, UIInfoArea * area)
{
    memset (area->visdata, 0, sizeof area->visdata);
}

/****************************************************************************/

static void ui_infoarea_draw_text (UIInfoArea * area, gint x, gint y, gint
 width, gfloat r, gfloat g, gfloat b, gfloat a, const gchar * font,
 const gchar * text)
{
    cairo_t *cr;
    PangoLayout *pl;
    PangoFontDescription *desc;
    gchar *str;

    str = g_markup_escape_text(text, -1);

    cr = gdk_cairo_create(area->parent->window);
    cairo_move_to(cr, x, y);
    cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
    cairo_set_source_rgba(cr, r, g, b, a);

    desc = pango_font_description_from_string(font);
    pl = gtk_widget_create_pango_layout(area->parent, NULL);
    pango_layout_set_markup(pl, str, -1);
    pango_layout_set_font_description(pl, desc);
    pango_font_description_free(desc);
    pango_layout_set_width (pl, width * PANGO_SCALE);
    pango_layout_set_ellipsize (pl, PANGO_ELLIPSIZE_END);

    AUDDBG("Drawing %s to %d, %d at %p layout %p\n", text, x, y, cr, pl);

    pango_cairo_show_layout(cr, pl);

    cairo_destroy(cr);
    g_object_unref(pl);
    g_free(str);
}

/****************************************************************************/

static void rgb_to_hsv (gfloat r, gfloat g, gfloat b, gfloat * h, gfloat * s,
 gfloat * v)
{
    gfloat max, min;

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

static void hsv_to_rgb (gfloat h, gfloat s, gfloat v, gfloat * r, gfloat * g,
 gfloat * b)
{
    for (; h >= 2; h -= 2)
    {
        gfloat * p = r;
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

static void get_color (GtkWidget * widget, gint i, gfloat * r, gfloat * g,
 gfloat * b)
{
    GdkColor * c = widget->style->base + GTK_STATE_SELECTED;
    gfloat h, s, v, n;

    rgb_to_hsv (c->red / 65535.0, c->green / 65535.0, c->blue / 65535.0, & h,
     & s, & v);

    if (s < 0.1) /* monochrome theme? use blue instead */
    {
        h = 5;
        s = 0.75;
    }

    n = sqrt (i / 11.0);
    s = sqrt (s) * (1 - 0.75 * n);
    v = 0.75 + 0.25 * n;

    hsv_to_rgb (h, s, v, r, g, b);
}

static void ui_infoarea_draw_visualizer (UIInfoArea * area)
{
    GtkAllocation alloc;
    cairo_t *cr;

    gtk_widget_get_allocation(GTK_WIDGET(area->parent), &alloc);
    cr = gdk_cairo_create(area->parent->window);

    for (auto gint i = 0; i < SPECT_BANDS; i++)
    {
        gint x = alloc.width - VIS_OFFSET + 10 + 12 * i;
        gfloat r, g, b;

        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_rectangle (cr, x, 10, 9, 64 - area->visdata[i]);
        cairo_fill (cr);

        get_color (area->parent, i, & r, & g, & b);
        cairo_set_source_rgb (cr, r, g, b);
        cairo_rectangle (cr, x, 74 - area->visdata[i], 9, area->visdata[i]);
        cairo_fill (cr);
    }

    cairo_destroy(cr);
}

static GdkPixbuf * get_current_album_art (void)
{
    gint playlist = aud_playlist_get_playing ();
    const gchar * filename = aud_playlist_entry_get_filename (playlist,
     aud_playlist_get_position (playlist));
    InputPlugin * decoder = aud_file_find_decoder (filename, FALSE);
    void * data;
    gint size;
    GdkPixbuf * pixbuf;

    if (filename == NULL || decoder == NULL || ! aud_file_read_image (filename,
     decoder, & data, & size))
        return NULL;

    pixbuf = audgui_pixbuf_from_data (data, size);
    g_free (data);
    return pixbuf;
}

void ui_infoarea_draw_album_art (UIInfoArea * area)
{
    cairo_t * cr;

    if (aud_drct_get_playing () && area->pb == NULL)
    {
        area->pb = get_current_album_art ();

        if (area->pb == NULL)
            area->pb = gdk_pixbuf_new_from_file (DEFAULT_ARTWORK, NULL);

        if (area->pb != NULL)
            audgui_pixbuf_scale_within (& area->pb, ICON_SIZE);
    }

    cr = gdk_cairo_create (area->parent->window);

    if (area->pb != NULL)
    {
        gdk_cairo_set_source_pixbuf (cr, area->pb, 10.0, 10.0);
        cairo_paint_with_alpha (cr, area->alpha);
    }

    if (area->last_pb != NULL)
    {
        gdk_cairo_set_source_pixbuf (cr, area->last_pb, 10.0, 10.0);
        cairo_paint_with_alpha (cr, area->last_alpha);
    }

    cairo_destroy (cr);
}

void ui_infoarea_draw_title (UIInfoArea * area)
{
    GtkAllocation alloc;
    gint width;

    gtk_widget_get_allocation (area->parent, & alloc);
    width = alloc.width - (86 + VIS_OFFSET + 6);

    if (area->title != NULL)
        ui_infoarea_draw_text (area, 86, 8, width, 1, 1, 1, area->alpha,
         "Sans 18", area->title);
    if (area->last_title != NULL)
        ui_infoarea_draw_text (area, 86, 8, width, 1, 1, 1, area->last_alpha,
         "Sans 18", area->last_title);
    if (area->artist != NULL)
        ui_infoarea_draw_text (area, 86, 42, width, 1, 1, 1, area->alpha,
         "Sans 9", area->artist);
    if (area->last_artist != NULL)
        ui_infoarea_draw_text (area, 86, 42, width, 1, 1, 1, area->last_alpha,
         "Sans 9", area->last_artist);
    if (area->album != NULL)
        ui_infoarea_draw_text (area, 86, 58, width, 0.7, 0.7, 0.7, area->alpha,
         "Sans 9", area->album);
    if (area->last_album != NULL)
        ui_infoarea_draw_text (area, 86, 58, width, 0.7, 0.7, 0.7,
         area->last_alpha, "Sans 9", area->last_album);
}

void
ui_infoarea_draw_background(UIInfoArea *area)
{
    GtkWidget *evbox;
    GtkAllocation alloc;
    cairo_t *cr;

    g_return_if_fail(area != NULL);

    evbox = area->parent;
    cr = gdk_cairo_create(evbox->window);

    gtk_widget_get_allocation(GTK_WIDGET(evbox), &alloc);

    cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
    cairo_paint(cr);

    cairo_destroy(cr);
}

gboolean
ui_infoarea_expose_event(UIInfoArea *area, GdkEventExpose *event, gpointer unused)
{
    ui_infoarea_draw_background(area);
    ui_infoarea_draw_album_art(area);
    ui_infoarea_draw_title(area);
    ui_infoarea_draw_visualizer(area);

    return TRUE;
}

static gboolean ui_infoarea_do_fade (UIInfoArea * area)
{
    gboolean ret = FALSE;

    if (aud_drct_get_playing () && area->alpha < 1)
    {
        area->alpha += alpha_step;
        ret = TRUE;
    }

    if (area->last_alpha > 0)
    {
        area->last_alpha -= alpha_step;
        ret = TRUE;
    }

    gtk_widget_queue_draw ((GtkWidget *) area->parent);

    if (! ret)
        area->fade_timeout = 0;

    return ret;
}

void ui_infoarea_set_title (void * data, UIInfoArea * area)
{
    if (! aud_drct_get_playing ())
        return;

    gint playlist = aud_playlist_get_playing ();
    gint entry = aud_playlist_get_position (playlist);
    const Tuple * tuple = aud_playlist_entry_get_tuple (playlist, entry, FALSE);
    const gchar * s;

    g_free (area->title);
    area->title = NULL;
    g_free (area->artist);
    area->artist = NULL;
    g_free (area->album);
    area->album = NULL;

    if (tuple && (s = tuple_get_string (tuple, FIELD_TITLE, NULL)))
        area->title = g_strdup (s);
    else
        area->title = g_strdup (aud_playlist_entry_get_title (playlist, entry,
         FALSE));

    if (tuple && (s = tuple_get_string (tuple, FIELD_ARTIST, NULL)))
        area->artist = g_strdup (s);

    if (tuple && (s = tuple_get_string (tuple, FIELD_ALBUM, NULL)))
        area->album = g_strdup (s);

    gtk_widget_queue_draw ((GtkWidget *) area->parent);
}

static void infoarea_next (UIInfoArea * area)
{
    if (area->last_pb)
        g_object_unref (area->last_pb);
    area->last_pb = area->pb;
    area->pb = NULL;

    g_free (area->last_title);
    area->last_title = area->title;
    area->title = NULL;

    g_free (area->last_artist);
    area->last_artist = area->artist;
    area->artist = NULL;

    g_free (area->last_album);
    area->last_album = area->album;
    area->album = NULL;

    area->last_alpha = area->alpha;
    area->alpha = 0;

    gtk_widget_queue_draw ((GtkWidget *) area->parent);
}

static void ui_infoarea_playback_start (void * data, UIInfoArea * area)
{
    if (! area->stopped) /* moved to the next song without stopping? */
        infoarea_next (area);
    area->stopped = FALSE;

    ui_infoarea_set_title (NULL, area);

    if (! area->fade_timeout)
        area->fade_timeout = g_timeout_add (30, (GSourceFunc)
         ui_infoarea_do_fade, area);
}

static void ui_infoarea_playback_stop (void * data, UIInfoArea * area)
{
    infoarea_next (area);
    area->stopped = TRUE;

    if (! area->fade_timeout)
        area->fade_timeout = g_timeout_add (30, (GSourceFunc)
         ui_infoarea_do_fade, area);
}

static void destroy_cb (GtkObject * parent, UIInfoArea * area)
{
    hook_dissociate ("title change", (HookFunction) ui_infoarea_set_title);
    hook_dissociate ("playback begin", (HookFunction)
     ui_infoarea_playback_start);
    hook_dissociate ("playback stop", (HookFunction)
     ui_infoarea_playback_stop);
    hook_dissociate ("visualization clear", (HookFunction) vis_clear_cb);
    aud_vis_runner_remove_hook ((VisHookFunc) ui_infoarea_visualization_timeout);

    g_free (area->title);
    g_free (area->artist);
    g_free (area->album);
    g_free (area->last_title);
    g_free (area->last_artist);
    g_free (area->last_album);

    if (area->pb)
        g_object_unref (area->pb);
    if (area->last_pb)
        g_object_unref (area->last_pb);

    g_slice_free (UIInfoArea, area);
}

GtkWidget * ui_infoarea_new (void)
{
    UIInfoArea *area;
    GtkWidget *evbox;

    area = g_slice_new0(UIInfoArea);

    evbox = gtk_event_box_new();
    area->parent = evbox;

    gtk_widget_set_size_request(GTK_WIDGET(evbox), -1, 84);

    g_signal_connect_swapped(area->parent, "expose-event",
                             G_CALLBACK(ui_infoarea_expose_event), area);

    hook_associate("title change", (HookFunction) ui_infoarea_set_title, area);
    hook_associate("playback begin", (HookFunction) ui_infoarea_playback_start, area);
    hook_associate("playback stop", (HookFunction) ui_infoarea_playback_stop, area);
    hook_associate("visualization clear", (HookFunction) vis_clear_cb, area);
    aud_vis_runner_add_hook ((VisHookFunc) ui_infoarea_visualization_timeout,
     area);

    g_signal_connect (area->parent, "destroy", (GCallback) destroy_cb, area);

    if (aud_drct_get_playing ())
        ui_infoarea_playback_start (NULL, area);

    return area->parent;
}
