/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include <string.h>

#include "draw-compat.h"
#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_skinned_textbox.h"

#define TIMEOUT 30
#define DELAY 50

typedef struct {
    gint width;
    gchar * text;
    PangoFontDescription * font;
    cairo_surface_t * buf;
    gint buf_width;
    gboolean may_scroll, two_way;
    gboolean scrolling, backward;
    gint scroll_source;
    gint offset, delay;
} TextboxData;

static GList * textboxes;

DRAW_FUNC_BEGIN (textbox_draw)
    TextboxData * data = g_object_get_data ((GObject *) wid, "textboxdata");
    g_return_val_if_fail (data && data->buf, FALSE);

    if (data->scrolling)
    {
        cairo_set_source_surface (cr, data->buf, -data->offset, 0);
        cairo_paint (cr);

        if (-data->offset + data->buf_width < data->width)
        {
            cairo_set_source_surface (cr, data->buf, -data->offset +
             data->buf_width, 0);
            cairo_paint (cr);
        }
    }
    else
    {
        cairo_set_source_surface (cr, data->buf, 0, 0);
        cairo_paint (cr);
    }
DRAW_FUNC_END

static gboolean textbox_scroll (GtkWidget * textbox)
{
    TextboxData * data = g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_val_if_fail (data, FALSE);

    if (data->delay < DELAY)
    {
        data->delay ++;
        return TRUE;
    }

    if (data->two_way && data->backward)
        data->offset --;
    else
        data->offset ++;

    if (data->two_way && (data->backward ? (data->offset <= 0) :
     (data->offset + data->width >= data->buf_width)))
    {
        data->backward = ! data->backward;
        data->delay = 0;
    }

    if (! data->two_way && data->offset >= data->buf_width)
        data->offset = 0;

    gtk_widget_queue_draw (textbox);
    return TRUE;
}

static void textbox_render_vector (GtkWidget * textbox, TextboxData * data,
 const gchar * text)
{
    g_return_if_fail (data->font && ! data->buf && text);

    PangoLayout * layout = gtk_widget_create_pango_layout (textbox, text);
    pango_layout_set_font_description (layout, data->font);

    PangoRectangle ink, logical;
    pango_layout_get_pixel_extents (layout, & ink, & logical);

    /* use logical width so as not to trim off the trailing space of the " --- " */
    /* use ink height since vertical space is quite limited */
    logical.width = MAX (logical.width, 1);
    ink.height = MAX (ink.height, 1);

    gtk_widget_set_size_request (textbox, data->width, ink.height);

    data->buf_width = MAX (logical.width, data->width);
    data->buf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
     data->buf_width, ink.height);

    cairo_t * cr = cairo_create (data->buf);

    set_cairo_color (cr, active_skin->colors[SKIN_TEXTBG]);
    cairo_paint (cr);

    cairo_move_to (cr, -logical.x, -ink.y);
    set_cairo_color (cr, active_skin->colors[SKIN_TEXTFG]);
    pango_cairo_show_layout (cr, layout);

    cairo_destroy (cr);
    g_object_unref (layout);
}

static void lookup_char (const gchar c, gint * x, gint * y)
{
    gint tx, ty;

    switch (c)
    {
    case '"': tx = 26; ty = 0; break;
    case '@': tx = 27; ty = 0; break;
    case ' ': tx = 29; ty = 0; break;
    case ':':
    case ';':
    case '|': tx = 12; ty = 1; break;
    case '(':
    case '{': tx = 13; ty = 1; break;
    case ')':
    case '}': tx = 14; ty = 1; break;
    case '-':
    case '~': tx = 15; ty = 1; break;
    case '`':
    case '\'': tx = 16; ty = 1; break;
    case '!': tx = 17; ty = 1; break;
    case '_': tx = 18; ty = 1; break;
    case '+': tx = 19; ty = 1; break;
    case '\\': tx = 20; ty = 1; break;
    case '/': tx = 21; ty = 1; break;
    case '[': tx = 22; ty = 1; break;
    case ']': tx = 23; ty = 1; break;
    case '^': tx = 24; ty = 1; break;
    case '&': tx = 25; ty = 1; break;
    case '%': tx = 26; ty = 1; break;
    case '.':
    case ',': tx = 27; ty = 1; break;
    case '=': tx = 28; ty = 1; break;
    case '$': tx = 29; ty = 1; break;
    case '#': tx = 30; ty = 1; break;
    case '?': tx = 3; ty = 2; break;
    case '*': tx = 4; ty = 2; break;
    default: tx = 3; ty = 2; break; /* '?' */
    }

    * x = tx * active_skin->properties.textbox_bitmap_font_width;
    * y = ty * active_skin->properties.textbox_bitmap_font_height;
}

static void textbox_render_bitmap (GtkWidget * textbox, TextboxData * data,
 const gchar * text)
{
    g_return_if_fail (! data->font && ! data->buf && text);

    gint cw = active_skin->properties.textbox_bitmap_font_width;
    gint ch = active_skin->properties.textbox_bitmap_font_height;

    gtk_widget_set_size_request (textbox, data->width, ch);

    glong len;
    gunichar * utf32 = g_utf8_to_ucs4 (text, -1, NULL, & len, NULL);
    g_return_if_fail (utf32);

    data->buf_width = MAX (cw * len, data->width);
    data->buf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
     data->buf_width, ch);

    cairo_t * cr = cairo_create (data->buf);

    gunichar * s = utf32;
    for (gint x = 0; x < data->buf_width; x += cw)
    {
        gunichar c = * s ? * s ++ : ' ';
        gint cx = 0, cy = 0;

        if (c >= 'A' && c <= 'Z')
            cx = cw * (c - 'A');
        else if (c >= 'a' && c <= 'z')
            cx = cw * (c - 'a');
        else if (c >= '0' && c <= '9')
        {
            cx = cw * (c - '0');
            cy = ch;
        }
        else
            lookup_char (c, & cx, & cy);

        skin_draw_pixbuf (cr, SKIN_TEXT, cx, cy, x, 0, cw, ch);
    }

    cairo_destroy (cr);
    g_free (utf32);
}

static void textbox_render (GtkWidget * textbox, TextboxData * data)
{
    g_return_if_fail (data->text);

    if (data->buf)
    {
        cairo_surface_destroy (data->buf);
        data->buf = NULL;
    }

    data->scrolling = FALSE;
    data->backward = FALSE;
    data->offset = 0;
    data->delay = 0;

    if (data->font)
        textbox_render_vector (textbox, data, data->text);
    else
        textbox_render_bitmap (textbox, data, data->text);

    if (data->may_scroll && data->buf_width > data->width)
    {
        data->scrolling = TRUE;

        if (! data->two_way)
        {
            if (data->buf)
            {
                cairo_surface_destroy (data->buf);
                data->buf = NULL;
            }

            gchar * temp = g_strdup_printf ("%s --- ", data->text);

            if (data->font)
                textbox_render_vector (textbox, data, temp);
            else
                textbox_render_bitmap (textbox, data, temp);

            g_free (temp);
        }
    }

    gtk_widget_queue_draw (textbox);

    if (data->scrolling)
    {
        if (! data->scroll_source)
            data->scroll_source = g_timeout_add (TIMEOUT, (GSourceFunc)
             textbox_scroll, textbox);
    }
    else
    {
        if (data->scroll_source)
        {
            g_source_remove (data->scroll_source);
            data->scroll_source = 0;
        }
    }
}

void textbox_set_width (GtkWidget * textbox, gint width)
{
    TextboxData * data = g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (data->width == width)
        return;

    data->width = width;
    textbox_render (textbox, data);
}

const gchar * textbox_get_text (GtkWidget * textbox)
{
    TextboxData * data = g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_val_if_fail (data, NULL);

    return data->text;
}

void textbox_set_text (GtkWidget * textbox, const gchar * text)
{
    TextboxData * data = g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (! text)
        text = "";

    if (data->text && ! strcmp (data->text, text))
        return;

    g_free (data->text);
    data->text = g_strdup (text);
    textbox_render (textbox, data);
}

void textbox_set_font (GtkWidget * textbox, const gchar * font)
{
    TextboxData * data = g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (data->font)
    {
        pango_font_description_free (data->font);
        data->font = NULL;
    }

    if (font)
        data->font = pango_font_description_from_string (font);

    textbox_render (textbox, data);
}

void textbox_set_scroll (GtkWidget * textbox, gboolean scroll)
{
    TextboxData * data = g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (data->may_scroll == scroll && data->two_way == config.twoway_scroll)
        return;

    data->may_scroll = scroll;
    data->two_way = config.twoway_scroll;
    textbox_render (textbox, data);
}

static void textbox_destroy (GtkWidget * textbox)
{
    TextboxData * data = g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (data->font)
        pango_font_description_free (data->font);
    if (data->buf)
        cairo_surface_destroy (data->buf);
    if (data->scroll_source)
        g_source_remove (data->scroll_source);

    g_free (data->text);
    g_free (data);

    textboxes = g_list_remove (textboxes, textbox);
}

GtkWidget * textbox_new (gint width, const gchar * text, const gchar * font,
 gboolean scroll)
{
    GtkWidget * textbox = gtk_drawing_area_new ();
    gtk_widget_set_size_request (textbox, width, 0);
    gtk_widget_add_events (textbox, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK);

    DRAW_CONNECT (textbox, textbox_draw);
    g_signal_connect (textbox, "destroy", (GCallback) textbox_destroy, NULL);

    TextboxData * data = g_malloc0 (sizeof (TextboxData));
    data->width = width;
    data->text = g_strdup (text);
    data->may_scroll = scroll;
    data->two_way = config.twoway_scroll;
    g_object_set_data ((GObject *) textbox, "textboxdata", data);

    if (font)
        data->font = pango_font_description_from_string (font);

    textboxes = g_list_prepend (textboxes, textbox);

    textbox_render (textbox, data);
    return textbox;
}

void textbox_update_all (void)
{
    for (GList * node = textboxes; node; node = node->next)
    {
        GtkWidget * textbox = node->data;
        g_return_if_fail (textbox);
        TextboxData * data = g_object_get_data ((GObject *) textbox,
         "textboxdata");
        g_return_if_fail (data);

        textbox_render (textbox, data);
    }
}
