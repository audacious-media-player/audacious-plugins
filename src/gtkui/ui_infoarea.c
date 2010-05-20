/*  
 *  Copyright (C) 2010 William Pitcock <nenolod@atheme.org>.
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

#include "gtkui_cfg.h"
#include "ui_gtk.h"
#include "ui_playlist_widget.h"
#include "ui_playlist_model.h"
#include "ui_manager.h"
#include "ui_infoarea.h"

#define DEFAULT_ARTWORK DATA_DIR "/images/audio.png"

static const gfloat alpha_step = 0.10;

const Tuple *
ui_infoarea_get_current_tuple(void)
{
    gint playlist;
    gint entry;

    playlist = aud_playlist_get_playing();
    entry = aud_playlist_get_position(playlist);

    return aud_playlist_entry_get_tuple(playlist, entry);
}

/****************************************************************************/

static void
ui_infoarea_draw_text(UIInfoArea *area, gint x, gint y, gfloat alpha, const gchar *font, const gchar *text)
{
    cairo_t *cr;
    PangoLayout *pl;
    PangoFontDescription *desc;
    gchar *str;

    str = g_markup_escape_text(text, -1);

    cr = gdk_cairo_create(area->parent->window);
    cairo_move_to(cr, x, y);
    cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);

    desc = pango_font_description_from_string(font);
    pl = gtk_widget_create_pango_layout(area->parent, NULL);
    pango_layout_set_markup(pl, str, -1);
    pango_layout_set_font_description(pl, desc);
    pango_font_description_free(desc);

    AUDDBG("Drawing %s to %d, %d at %p layout %p\n", text, x, y, cr, pl);

    pango_cairo_show_layout(cr, pl);

    cairo_destroy(cr);
    g_object_unref(pl);
    g_free(str);
}

/****************************************************************************/

void
ui_infoarea_draw_album_art(UIInfoArea *area)
{
    GError *err = NULL;
    GdkPixbuf *pb;
    cairo_t *cr;

    if (area->tu == NULL)
        return;

    pb = gdk_pixbuf_new_from_file(DEFAULT_ARTWORK, &err);
    if (pb == NULL)
        return;

    cr = gdk_cairo_create(area->parent->window);
    gdk_cairo_set_source_pixbuf(cr, pb, 12.0, 12.0);
    cairo_paint_with_alpha(cr, area->alpha.artwork);

    cairo_destroy(cr);
    g_object_unref(pb);
}

void
ui_infoarea_draw_title(UIInfoArea *area)
{
    if (area->tu == NULL)
        return;

    ui_infoarea_draw_text(area, 86,  8, area->alpha.title, "Sans 20", tuple_get_string(area->tu, FIELD_TITLE, NULL));
    ui_infoarea_draw_text(area, 86, 42, area->alpha.artist, "Sans 9", tuple_get_string(area->tu, FIELD_ARTIST, NULL));
    ui_infoarea_draw_text(area, 86, 58, area->alpha.album, "Sans 9", tuple_get_string(area->tu, FIELD_ALBUM, NULL));
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

    return TRUE;
}

static gboolean
ui_infoarea_do_fade_in(UIInfoArea *area)
{
    gboolean ret = FALSE;

    if (area->alpha.title < 1.0)
    {
        area->alpha.title += alpha_step;
        ret = TRUE;
    }

    if (area->alpha.artist < 1.0)
    {
        area->alpha.artist += alpha_step;
        ret = TRUE;
    }

    if (area->alpha.album < 0.7)
    {
        area->alpha.album += alpha_step;
        ret = TRUE;
    }

    if (area->alpha.artwork < 0.7)
    {
        area->alpha.artwork += alpha_step;
        ret = TRUE;
    }

    gtk_widget_queue_draw(GTK_WIDGET(area->parent));

    if (ret == FALSE)
        area->fadein_timeout = 0;

    if (ret == FALSE)
        AUDDBG("fadein complete\n");

    return ret;
}

void
ui_infoarea_set_title(gpointer unused, UIInfoArea *area)
{
    const Tuple *tuple;

    g_return_if_fail(area != NULL);

    tuple = ui_infoarea_get_current_tuple();

    if (area->tu != NULL)
        mowgli_object_unref(area->tu);

    area->tu = tuple_copy(tuple);
    area->alpha.artist = area->alpha.album = area->alpha.artwork = area->alpha.title = 0.0;

    if (!area->fadein_timeout)
        area->fadein_timeout = g_timeout_add(10, (GSourceFunc) ui_infoarea_do_fade_in, area);

    gtk_widget_queue_draw(GTK_WIDGET(area->parent));
}

UIInfoArea *
ui_infoarea_new(void)
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

    return area;
}
