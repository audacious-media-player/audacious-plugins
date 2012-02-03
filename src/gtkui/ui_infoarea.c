/*
 *  Copyright (C) 2010 William Pitcock <nenolod@atheme.org>.
 *  Copyright (C) 2010-2011 John Lindgren <john.lindgren@tds.net>.
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

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "ui_infoarea.h"

#define SPACING 8
#define ICON_SIZE 64
#define HEIGHT (ICON_SIZE + 2 * SPACING)

#define VIS_BANDS 12
#define VIS_WIDTH (8 * VIS_BANDS - 2)
#define VIS_CENTER (ICON_SIZE * 5 / 8 + SPACING)
#define VIS_DELAY 2 /* delay before falloff in frames */
#define VIS_FALLOFF 2 /* falloff in pixels per frame */

typedef struct {
    GtkWidget * box, * main, * vis;

    gchar * title, * artist, * album; /* pooled */
    gchar * last_title, * last_artist, * last_album; /* pooled */
    gfloat alpha, last_alpha;

    gboolean stopped;
    gint fade_timeout;
    gchar bars[VIS_BANDS];
    gchar delay[VIS_BANDS];

    GdkPixbuf * pb, * last_pb;
} UIInfoArea;

/****************************************************************************/

static UIInfoArea * area = NULL;

static void vis_render_cb (const gfloat * freq)
{
    g_return_if_fail (area);

    const gfloat xscale[VIS_BANDS + 1] = {0.00, 0.59, 1.52, 3.00, 5.36, 9.10,
     15.0, 24.5, 39.4, 63.2, 101, 161, 256}; /* logarithmic scale - 1 */

    for (gint i = 0; i < VIS_BANDS; i ++)
    {
        gint a = ceil (xscale[i]);
        gint b = floor (xscale[i + 1]);
        gfloat n = 0;

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

        /* 40 dB range */
        int x = 20 * log10 (n * 100);
        x = CLAMP (x, 0, 40);

        area->bars[i] -= MAX (0, VIS_FALLOFF - area->delay[i]);

        if (area->delay[i])
            area->delay[i] --;

        if (x > area->bars[i])
        {
            area->bars[i] = x;
            area->delay[i] = VIS_DELAY;
        }
    }

    gtk_widget_queue_draw (area->vis);
}

static void vis_clear_cb (void)
{
    g_return_if_fail (area);

    memset (area->bars, 0, sizeof area->bars);
    memset (area->delay, 0, sizeof area->delay);

    gtk_widget_queue_draw (area->vis);
}

/****************************************************************************/

static void clear (GtkWidget * widget, cairo_t * cr)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation (widget, & alloc);
    cairo_rectangle (cr, 0, 0, alloc.width, alloc.height);
    cairo_fill (cr);
}

static void draw_text (GtkWidget * widget, cairo_t * cr, gint x, gint y, gint
 width, gfloat r, gfloat g, gfloat b, gfloat a, const gchar * font,
 const gchar * text)
{
    cairo_move_to(cr, x, y);
    cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
    cairo_set_source_rgba(cr, r, g, b, a);

    PangoFontDescription * desc = pango_font_description_from_string (font);
    PangoLayout * pl = gtk_widget_create_pango_layout (widget, NULL);
    pango_layout_set_text (pl, text, -1);
    pango_layout_set_font_description(pl, desc);
    pango_font_description_free(desc);
    pango_layout_set_width (pl, width * PANGO_SCALE);
    pango_layout_set_ellipsize (pl, PANGO_ELLIPSIZE_END);

    pango_cairo_show_layout(cr, pl);

    g_object_unref(pl);
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

static void get_color (gint i, gfloat * r, gfloat * g, gfloat * b)
{
    static GdkRGBA c;
    static bool_t valid = FALSE;
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
        gtk_style_context_get_background_color (style, GTK_STATE_FLAG_SELECTED, & c);
        g_object_unref (style);
        valid = TRUE;
    }

    rgb_to_hsv (c.red, c.green, c.blue, & h, & s, & v);

    if (s < 0.1) /* monochrome theme? use blue instead */
    {
        h = 5;
        s = 0.75;
    }

    n = i / 11.0;
    s = 1 - 0.9 * n;
    v = 0.75 + 0.25 * n;

    hsv_to_rgb (h, s, v, r, g, b);
}

static gboolean draw_vis_cb (GtkWidget * vis, cairo_t * cr)
{
    g_return_val_if_fail (area, FALSE);

    clear (vis, cr);

    for (gint i = 0; i < VIS_BANDS; i++)
    {
        gint x = SPACING + 8 * i;
        gint t = VIS_CENTER - area->bars[i];
        gint m = MIN (VIS_CENTER + area->bars[i], HEIGHT);

        gfloat r, g, b;
        get_color (i, & r, & g, & b);

        cairo_set_source_rgb (cr, r, g, b);
        cairo_rectangle (cr, x, t, 6, VIS_CENTER - t);
        cairo_fill (cr);

        cairo_set_source_rgb (cr, r * 0.3, g * 0.3, b * 0.3);
        cairo_rectangle (cr, x, VIS_CENTER, 6, m - VIS_CENTER);
        cairo_fill (cr);
    }

    return TRUE;
}

static void draw_album_art (cairo_t * cr)
{
    g_return_if_fail (area);

    if (area->pb != NULL)
    {
        gdk_cairo_set_source_pixbuf (cr, area->pb, SPACING, SPACING);
        cairo_paint_with_alpha (cr, area->alpha);
    }

    if (area->last_pb != NULL)
    {
        gdk_cairo_set_source_pixbuf (cr, area->last_pb, SPACING, SPACING);
        cairo_paint_with_alpha (cr, area->last_alpha);
    }
}

static void draw_title (cairo_t * cr)
{
    g_return_if_fail (area);

    GtkAllocation alloc;
    gtk_widget_get_allocation (area->main, & alloc);

    gint x = ICON_SIZE + SPACING * 2;
    gint width = alloc.width - x;

    if (area->title != NULL)
        draw_text (area->main, cr, x, SPACING, width, 1, 1, 1, area->alpha,
         "Sans 18", area->title);
    if (area->last_title != NULL)
        draw_text (area->main, cr, x, SPACING, width, 1, 1, 1, area->last_alpha,
         "Sans 18", area->last_title);
    if (area->artist != NULL)
        draw_text (area->main, cr, x, SPACING + ICON_SIZE / 2, width, 1, 1, 1,
         area->alpha, "Sans 9", area->artist);
    if (area->last_artist != NULL)
        draw_text (area->main, cr, x, SPACING + ICON_SIZE / 2, width, 1, 1, 1,
         area->last_alpha, "Sans 9", area->last_artist);
    if (area->album != NULL)
        draw_text (area->main, cr, x, SPACING + ICON_SIZE * 3 / 4, width, 0.7,
         0.7, 0.7, area->alpha, "Sans 9", area->album);
    if (area->last_album != NULL)
        draw_text (area->main, cr, x, SPACING + ICON_SIZE * 3 / 4, width, 0.7,
         0.7, 0.7, area->last_alpha, "Sans 9", area->last_album);
}

static gboolean draw_cb (GtkWidget * widget, cairo_t * cr)
{
    g_return_val_if_fail (area, FALSE);

    clear (widget, cr);

    draw_album_art (cr);
    draw_title (cr);

    return TRUE;
}

static gboolean ui_infoarea_do_fade (void)
{
    g_return_val_if_fail (area, FALSE);
    gboolean ret = FALSE;

    if (aud_drct_get_playing () && area->alpha < 1)
    {
        area->alpha += 0.1;
        ret = TRUE;
    }

    if (area->last_alpha > 0)
    {
        area->last_alpha -= 0.1;
        ret = TRUE;
    }

    gtk_widget_queue_draw (area->main);

    if (! ret)
        area->fade_timeout = 0;

    return ret;
}

static gint strcmp_null (const gchar * a, const gchar * b)
{
    if (! a)
        return (! b) ? 0 : -1;
    if (! b)
        return 1;
    return strcmp (a, b);
}

void ui_infoarea_set_title (void)
{
    g_return_if_fail (area);

    if (! aud_drct_get_playing ())
        return;

    gint playlist = aud_playlist_get_playing ();
    gint entry = aud_playlist_get_position (playlist);

    gchar * title, * artist, * album;
    aud_playlist_entry_describe (playlist, entry, & title, & artist, & album, TRUE);

    if (! strcmp_null (title, area->title) && ! strcmp_null (artist,
     area->artist) && ! strcmp_null (album, area->album))
    {
        str_unref (title);
        str_unref (artist);
        str_unref (album);
        return;
    }

    str_unref (area->title);
    str_unref (area->artist);
    str_unref (area->album);
    area->title = title;
    area->artist = artist;
    area->album = album;

    gtk_widget_queue_draw (area->main);
}

static void set_album_art (void)
{
    g_return_if_fail (area);

    if (area->pb)
        g_object_unref (area->pb);

    area->pb = audgui_pixbuf_for_current ();
    if (area->pb)
        audgui_pixbuf_scale_within (& area->pb, ICON_SIZE);
}

static void infoarea_next (void)
{
    g_return_if_fail (area);

    if (area->last_pb)
        g_object_unref (area->last_pb);
    area->last_pb = area->pb;
    area->pb = NULL;

    str_unref (area->last_title);
    area->last_title = area->title;
    area->title = NULL;

    str_unref (area->last_artist);
    area->last_artist = area->artist;
    area->artist = NULL;

    str_unref (area->last_album);
    area->last_album = area->album;
    area->album = NULL;

    area->last_alpha = area->alpha;
    area->alpha = 0;

    gtk_widget_queue_draw (area->main);
}

static void ui_infoarea_playback_start (void)
{
    g_return_if_fail (area);

    if (! area->stopped) /* moved to the next song without stopping? */
        infoarea_next ();
    area->stopped = FALSE;

    ui_infoarea_set_title ();
    set_album_art ();

    if (! area->fade_timeout)
        area->fade_timeout = g_timeout_add (30, (GSourceFunc)
         ui_infoarea_do_fade, area);
}

static void ui_infoarea_playback_stop (void)
{
    g_return_if_fail (area);

    infoarea_next ();
    area->stopped = TRUE;

    if (! area->fade_timeout)
        area->fade_timeout = g_timeout_add (30, (GSourceFunc)
         ui_infoarea_do_fade, area);
}

static void destroy_cb (GtkWidget * widget)
{
    g_return_if_fail (area);

    hook_dissociate ("playlist update", (HookFunction) ui_infoarea_set_title);
    hook_dissociate ("playback begin", (HookFunction)
     ui_infoarea_playback_start);
    hook_dissociate ("playback stop", (HookFunction)
     ui_infoarea_playback_stop);

    aud_vis_func_remove ((VisFunc) vis_clear_cb);
    aud_vis_func_remove ((VisFunc) vis_render_cb);

    if (area->fade_timeout)
    {
        g_source_remove (area->fade_timeout);
        area->fade_timeout = 0;
    }

    str_unref (area->title);
    str_unref (area->artist);
    str_unref (area->album);
    str_unref (area->last_title);
    str_unref (area->last_artist);
    str_unref (area->last_album);

    if (area->pb)
        g_object_unref (area->pb);
    if (area->last_pb)
        g_object_unref (area->last_pb);

    g_slice_free (UIInfoArea, area);
    area = NULL;
}

GtkWidget * ui_infoarea_new (void)
{
    g_return_val_if_fail (! area, NULL);
    area = g_slice_new0 (UIInfoArea);

    area->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    area->main = gtk_drawing_area_new ();
    gtk_widget_set_size_request (area->main, ICON_SIZE + 2 * SPACING, HEIGHT);
    gtk_box_pack_start ((GtkBox *) area->box, area->main, TRUE, TRUE, 0);

    area->vis = gtk_drawing_area_new ();
    gtk_widget_set_size_request (area->vis, VIS_WIDTH + 2 * SPACING, HEIGHT);
    gtk_box_pack_start ((GtkBox *) area->box, area->vis, FALSE, FALSE, 0);

    g_signal_connect (area->main, "draw", (GCallback) draw_cb, NULL);
    g_signal_connect (area->vis, "draw", (GCallback) draw_vis_cb, NULL);

    hook_associate ("playlist update", (HookFunction) ui_infoarea_set_title, NULL);
    hook_associate ("playback begin", (HookFunction) ui_infoarea_playback_start, NULL);
    hook_associate ("playback stop", (HookFunction) ui_infoarea_playback_stop, NULL);

    aud_vis_func_add (AUD_VIS_TYPE_CLEAR, (VisFunc) vis_clear_cb);
    aud_vis_func_add (AUD_VIS_TYPE_FREQ, (VisFunc) vis_render_cb);

    g_signal_connect (area->box, "destroy", (GCallback) destroy_cb, NULL);

    if (aud_drct_get_playing ())
    {
        ui_infoarea_playback_start ();

        /* skip fade-in */
        area->alpha = 1;

        if (area->fade_timeout)
        {
            g_source_remove (area->fade_timeout);
            area->fade_timeout = 0;
        }
    }

    return area->box;
}
