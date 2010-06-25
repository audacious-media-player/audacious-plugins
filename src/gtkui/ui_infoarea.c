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

#include <audacious/plugin.h>
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

static void
ui_infoarea_visualization_timeout(gpointer hook_data, UIInfoArea *area)
{
    const gfloat xscale[SPECT_BANDS + 1] = {0.00, 0.59, 1.52, 3.00, 5.36, 9.10,
     15.0, 24.5, 39.4, 63.2, 101, 161, 256}; /* logarithmic scale - 1 */
    VisNode *vis = (VisNode*) hook_data;
    gint16 mono_freq[2][256];

    aud_calc_mono_freq(mono_freq, vis->data, vis->nch);

    for (gint i = 0; i < SPECT_BANDS; i ++)
    {
        gint a = ceil (xscale[i]);
        gint b = floor (xscale[i + 1]);
        gint n = 0;

        if (a > 0)
            n += mono_freq[0][a - 1] * (a - xscale[i]);
        for (; a < b; a ++)
            n += mono_freq[0][a];
        if (b < 256)
            n += mono_freq[0][b] * (xscale[i + 1] - b);

        n = 32 * log10 (n / 327.67); /* 40 dB range */
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
    memset (area->visdata, 0, 20);
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

static struct {
    guint8 red;
    guint8 green;
    guint8 blue;
} colors[] = {
    { 0xff, 0xb1, 0x6f },
    { 0xff, 0xc8, 0x7f },
    { 0xff, 0xcf, 0x7e },
    { 0xf6, 0xe6, 0x99 },
    { 0xf1, 0xfc, 0xd4 },
    { 0xbd, 0xd8, 0xab },
    { 0xcd, 0xe6, 0xd0 },
    { 0xce, 0xe0, 0xea },
    { 0xd5, 0xdd, 0xea },
    { 0xee, 0xc1, 0xc8 },
    { 0xee, 0xaa, 0xb7 },
    { 0xec, 0xce, 0xb6 },
};

static void ui_infoarea_draw_visualizer (UIInfoArea * area)
{
    GtkAllocation alloc;
    cairo_t *cr;

    gtk_widget_get_allocation(GTK_WIDGET(area->parent), &alloc);
    cr = gdk_cairo_create(area->parent->window);

    for (auto gint i = 0; i < SPECT_BANDS; i++)
    {
        gint x, y, w, h;

        x = alloc.width - VIS_OFFSET + 10 + 12 * i;
        w = 9;

        y = 10;
        h = 64 - area->visdata[i];

        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_rectangle (cr, x, y, w, h);
        cairo_fill (cr);

        y = 10 + 64 - area->visdata[i];
        h = area->visdata[i];

        cairo_set_source_rgb (cr, colors[i].red / 255.0, colors[i].green /
         255.0, colors[i].blue / 255.0);
        cairo_rectangle (cr, x, y, w, h);
        cairo_fill (cr);
    }

    cairo_destroy(cr);
}

static GdkPixbuf * get_current_album_art (void)
{
    gint playlist = aud_playlist_get_playing ();
    gint position = aud_playlist_get_position (playlist);
    const gchar * filename = aud_playlist_entry_get_filename (playlist, position);
    InputPlugin * decoder = aud_playlist_entry_get_decoder (playlist, position);
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

    if (audacious_drct_get_playing () && ! area->pb)
    {
        area->pb = get_current_album_art ();

        if (! area->pb)
            area->pb = gdk_pixbuf_new_from_file (DEFAULT_ARTWORK, NULL);

        if (area->pb)
            audgui_pixbuf_scale_within (& area->pb, ICON_SIZE);
    }

    cr = gdk_cairo_create (area->parent->window);

    if (area->pb)
    {
        gdk_cairo_set_source_pixbuf (cr, area->pb, 10.0, 10.0);
        cairo_paint_with_alpha (cr, area->alpha);
    }

    if (area->last_pb)
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

    if (area->title)
        ui_infoarea_draw_text (area, 86, 8, width, 1, 1, 1, area->alpha,
         "Sans 18", area->title);
    if (area->last_title)
        ui_infoarea_draw_text (area, 86, 8, width, 1, 1, 1, area->last_alpha,
         "Sans 18", area->last_title);
    if (area->artist)
        ui_infoarea_draw_text (area, 86, 42, width, 1, 1, 1, area->alpha,
         "Sans 9", area->artist);
    if (area->last_artist)
        ui_infoarea_draw_text (area, 86, 42, width, 1, 1, 1, area->last_alpha,
         "Sans 9", area->last_artist);
    if (area->album)
        ui_infoarea_draw_text (area, 86, 58, width, 0.7, 0.7, 0.7, area->alpha,
         "Sans 9", area->album);
    if (area->last_album)
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

    if (audacious_drct_get_playing () && area->alpha < 1)
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
    gint playlist = aud_playlist_get_playing ();
    gint entry = aud_playlist_get_position (playlist);
    const Tuple * tuple = aud_playlist_entry_get_tuple (playlist, entry);
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
        area->title = g_strdup (aud_playlist_entry_get_title (playlist, entry));

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
    aud_hook_dissociate ("title change", (HookFunction) ui_infoarea_set_title);
    aud_hook_dissociate ("playback begin", (HookFunction)
     ui_infoarea_playback_start);
    aud_hook_dissociate ("playback stop", (HookFunction)
     ui_infoarea_playback_stop);
    aud_hook_dissociate ("visualization timeout", (HookFunction)
     ui_infoarea_visualization_timeout);
    aud_hook_dissociate ("visualization clear", (HookFunction) vis_clear_cb);

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

    aud_hook_associate("title change", (HookFunction) ui_infoarea_set_title, area);
    aud_hook_associate("playback begin", (HookFunction) ui_infoarea_playback_start, area);
    aud_hook_associate("playback stop", (HookFunction) ui_infoarea_playback_stop, area);
    aud_hook_associate("visualization timeout", (HookFunction) ui_infoarea_visualization_timeout, area);
    aud_hook_associate("visualization clear", (HookFunction) vis_clear_cb, area);

    g_signal_connect (area->parent, "destroy", (GCallback) destroy_cb, area);

    if (audacious_drct_get_playing ())
        ui_infoarea_playback_start (NULL, area);

    return area->parent;
}
