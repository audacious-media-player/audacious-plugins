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

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/objects.h>

#include "drawing.h"
#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_skinned_textbox.h"

#define DELAY 50

struct TextboxData
{
    String text;
    PangoFontDescPtr font;
    CairoSurfacePtr buf;

    int width, buf_width;
    bool may_scroll, two_way;
    bool scrolling, backward;
    int offset, delay;
};

static GList * textboxes;

DRAW_FUNC_BEGIN (textbox_draw, TextboxData)
    if (data->scrolling)
    {
        cairo_set_source_surface (cr, data->buf.get (), -data->offset * config.scale, 0);
        cairo_paint (cr);

        if (-data->offset + data->buf_width < data->width)
        {
            cairo_set_source_surface (cr, data->buf.get (),
             (-data->offset + data->buf_width) * config.scale, 0);
            cairo_paint (cr);
        }
    }
    else
    {
        cairo_set_source_surface (cr, data->buf.get (), 0, 0);
        cairo_paint (cr);
    }
DRAW_FUNC_END

static void textbox_scroll (void * textbox)
{
    TextboxData * data = (TextboxData *) g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (data->delay < DELAY)
    {
        data->delay ++;
        return;
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

    if (gtk_widget_is_drawable ((GtkWidget *) textbox))
        textbox_draw ((GtkWidget *) textbox, nullptr, data);
}

static void textbox_render_vector (GtkWidget * textbox, TextboxData * data,
 const char * text)
{
    PangoLayout * layout = gtk_widget_create_pango_layout (textbox, text);
    pango_layout_set_font_description (layout, data->font.get ());

    PangoRectangle ink, logical;
    pango_layout_get_pixel_extents (layout, & ink, & logical);

    /* use logical width so as not to trim off the trailing space of the " --- " */
    /* use ink height since vertical space is quite limited */
    logical.width = aud::max (logical.width, 1);
    ink.height = aud::max (ink.height, 1);

    gtk_widget_set_size_request (textbox, data->width * config.scale, ink.height);

    data->buf_width = aud::max ((logical.width + config.scale - 1) / config.scale, data->width);
    data->buf.capture (cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
     data->buf_width * config.scale, ink.height));

    cairo_t * cr = cairo_create (data->buf.get ());

    set_cairo_color (cr, skin.colors[SKIN_TEXTBG]);
    cairo_paint (cr);

    cairo_move_to (cr, -logical.x, -ink.y);
    set_cairo_color (cr, skin.colors[SKIN_TEXTFG]);
    pango_cairo_show_layout (cr, layout);

    cairo_destroy (cr);
    g_object_unref (layout);
}

static void lookup_char (const char c, int * x, int * y)
{
    int tx, ty;

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

    * x = tx * skin.hints.textbox_bitmap_font_width;
    * y = ty * skin.hints.textbox_bitmap_font_height;
}

static void textbox_render_bitmap (GtkWidget * textbox, TextboxData * data,
 const char * text)
{
    int cw = skin.hints.textbox_bitmap_font_width;
    int ch = skin.hints.textbox_bitmap_font_height;

    gtk_widget_set_size_request (textbox, data->width * config.scale, ch * config.scale);

    long len;
    gunichar * utf32 = g_utf8_to_ucs4 (text, -1, nullptr, & len, nullptr);
    g_return_if_fail (utf32);

    data->buf_width = aud::max (cw * (int) len, data->width);
    data->buf.capture (cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
     data->buf_width * config.scale, ch * config.scale));

    cairo_t * cr = cairo_create (data->buf.get ());

    gunichar * s = utf32;
    for (int x = 0; x < data->buf_width; x += cw)
    {
        gunichar c = * s ? * s ++ : ' ';
        int cx = 0, cy = 0;

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
    data->scrolling = FALSE;
    data->backward = FALSE;
    data->offset = 0;
    data->delay = 0;

    const char * text = data->text ? data->text : "";

    if (data->font)
        textbox_render_vector (textbox, data, text);
    else
        textbox_render_bitmap (textbox, data, text);

    if (data->may_scroll && data->buf_width > data->width)
    {
        data->scrolling = TRUE;

        if (! data->two_way)
        {
            StringBuf temp = str_printf ("%s --- ", text);

            if (data->font)
                textbox_render_vector (textbox, data, temp);
            else
                textbox_render_bitmap (textbox, data, temp);
        }
    }

    gtk_widget_queue_draw (textbox);

    if (data->scrolling)
        timer_add (TimerRate::Hz30, textbox_scroll, textbox);
    else
        timer_remove (TimerRate::Hz30, textbox_scroll, textbox);
}

void textbox_set_width (GtkWidget * textbox, int width)
{
    TextboxData * data = (TextboxData *) g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (data->width == width)
        return;

    data->width = width;
    textbox_render (textbox, data);
}

const char * textbox_get_text (GtkWidget * textbox)
{
    TextboxData * data = (TextboxData *) g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_val_if_fail (data, nullptr);

    return data->text;
}

void textbox_set_text (GtkWidget * textbox, const char * text)
{
    TextboxData * data = (TextboxData *) g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (! strcmp_safe (data->text, text))
        return;

    data->text = String (text);
    textbox_render (textbox, data);
}

void textbox_set_font (GtkWidget * textbox, const char * font)
{
    TextboxData * data = (TextboxData *) g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (font)
        data->font.capture (pango_font_description_from_string (font));
    else
        data->font.clear ();

    textbox_render (textbox, data);
}

void textbox_set_scroll (GtkWidget * textbox, bool scroll)
{
    TextboxData * data = (TextboxData *) g_object_get_data ((GObject *) textbox, "textboxdata");
    g_return_if_fail (data);

    if (data->may_scroll == scroll && data->two_way == config.twoway_scroll)
        return;

    data->may_scroll = scroll;
    data->two_way = config.twoway_scroll;
    textbox_render (textbox, data);
}

static void textbox_destroy (GtkWidget * textbox, TextboxData * data)
{
    timer_remove (TimerRate::Hz30, textbox_scroll, textbox);
    textboxes = g_list_remove (textboxes, textbox);
    delete data;
}

GtkWidget * textbox_new (int width, const char * font, bool scroll)
{
    GtkWidget * textbox = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) textbox, false);
    gtk_widget_set_size_request (textbox, width, 0);
    gtk_widget_add_events (textbox, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK);

    TextboxData * data = new TextboxData ();
    data->width = width;
    data->may_scroll = scroll;
    data->two_way = config.twoway_scroll;
    g_object_set_data ((GObject *) textbox, "textboxdata", data);

    if (font)
        data->font.capture (pango_font_description_from_string (font));

    DRAW_CONNECT_PROXY (textbox, textbox_draw, data);
    g_signal_connect (textbox, "destroy", (GCallback) textbox_destroy, data);

    textboxes = g_list_prepend (textboxes, textbox);

    textbox_render (textbox, data);
    return textbox;
}

void textbox_update_all (void)
{
    for (GList * node = textboxes; node; node = node->next)
    {
        GtkWidget * textbox = (GtkWidget *) node->data;
        g_return_if_fail (textbox);
        TextboxData * data = (TextboxData *) g_object_get_data ((GObject *) textbox, "textboxdata");
        g_return_if_fail (data);

        textbox_render (textbox, data);
    }
}
