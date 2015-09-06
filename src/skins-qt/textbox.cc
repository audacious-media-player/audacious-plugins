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

#include "skins_cfg.h"
#include "skin.h"
#include "textbox.h"

#define DELAY 50

static Index<TextBox *> textboxes;

void TextBox::draw (QPainter & cr)
{
    if (m_scrolling)
    {
        cr.drawImage (-m_offset * config.scale, 0, * m_buf);
        if (-m_offset + m_buf_width < m_width)
            cr.drawImage ((-m_offset + m_buf_width) * config.scale, 0, * m_buf);
    }
    else
        cr.drawImage (0, 0, * m_buf);
}

bool TextBox::button_press (QMouseEvent * event)
{
    return press ? press (event) : false;
}

void TextBox::scroll_timeout ()
{
    if (m_delay < DELAY)
    {
        m_delay ++;
        return;
    }

    if (m_two_way && m_backward)
        m_offset --;
    else
        m_offset ++;

    if (m_two_way && (m_backward ? (m_offset <= 0) : (m_offset + m_width >= m_buf_width)))
    {
        m_backward = ! m_backward;
        m_delay = 0;
    }

    if (! m_two_way && m_offset >= m_buf_width)
        m_offset = 0;

    draw_now ();
}

void TextBox::render_vector (const char * text)
{
    QRect ink = m_metrics->tightBoundingRect (text);
    int logical_width = m_metrics->width (text);

    /* use logical width so as not to trim off the trailing space of the " --- " */
    /* use ink height since vertical space is quite limited */
    int width = aud::max (-ink.x () + logical_width, 1);
    int height = aud::max (ink.height (), 1);

    resize (m_width * config.scale, height);

    m_buf_width = aud::max ((width + config.scale - 1) / config.scale, m_width);
    m_buf.capture (new QImage (m_buf_width * config.scale, height, QImage::Format_RGB32));

    QPainter cr (m_buf.get ());

    cr.fillRect (cr.window (), QColor (skin.colors[SKIN_TEXTBG]));

    cr.setFont (* m_font);
    cr.setPen (QColor (skin.colors[SKIN_TEXTFG]));
    cr.drawText (-ink.x (), -ink.y (), text);
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

void TextBox::render_bitmap (const char * text)
{
    int cw = skin.hints.textbox_bitmap_font_width;
    int ch = skin.hints.textbox_bitmap_font_height;

    resize (m_width * config.scale, ch * config.scale);

    auto ucs4 = QString (text).toUcs4 ();

    m_buf_width = aud::max (cw * ucs4.length (), m_width);
    m_buf.capture (new QImage (m_buf_width * config.scale, ch * config.scale,
     QImage::Format_RGB32));

    QPainter cr (m_buf.get ());
    if (config.scale != 1)
        cr.setTransform (QTransform ().scale (config.scale, config.scale));

    for (int x = 0, i = 0; x < m_buf_width; x += cw, i ++)
    {
        unsigned c = (i < ucs4.length ()) ? ucs4[i] : ' ';
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
}

void TextBox::render ()
{
    m_scrolling = false;
    m_backward = false;
    m_offset = 0;
    m_delay = 0;

    const char * text = m_text ? m_text : "";

    if (m_font)
        render_vector (text);
    else
        render_bitmap (text);

    if (m_may_scroll && m_buf_width > m_width)
    {
        m_scrolling = true;

        if (! m_two_way)
        {
            StringBuf temp = str_printf ("%s --- ", text);

            if (m_font)
                render_vector (temp);
            else
                render_bitmap (temp);
        }
    }

    queue_draw ();

    if (m_scrolling)
        scroll_timer.start ();
    else
        scroll_timer.stop ();
}

void TextBox::set_width (int width)
{
    if (m_width != width)
    {
        m_width = width;
        render ();
    }
}

void TextBox::set_text (const char * text)
{
    if (strcmp_safe (m_text, text))
    {
        m_text = String (text);
        render ();
    }
}

void TextBox::set_font (const char * font)
{
    if (font)
    {
        m_font.capture (qfont_from_string (font));
        m_metrics.capture (new QFontMetrics (* m_font, this));
    }
    else
    {
        m_font.clear ();
        m_metrics.clear ();
    }

    render ();
}

void TextBox::set_scroll (bool scroll)
{
    if (m_may_scroll != scroll || m_two_way != config.twoway_scroll)
    {
        m_may_scroll = scroll;
        m_two_way = config.twoway_scroll;
        render ();
    }
}

TextBox::~TextBox ()
{
    int idx = textboxes.find (this);
    if (idx >= 0)
        textboxes.remove (idx, 1);
}

TextBox::TextBox (int width, const char * font, bool scroll) :
    m_width (width),
    m_may_scroll (scroll),
    m_two_way (config.twoway_scroll)
{
    /* size is computed by set_font() */
    add_input (1, 1, false, true);
    set_font (font);

    textboxes.append (this);
}

void TextBox::update_all ()
{
    for (TextBox * textbox : textboxes)
        textbox->render ();
}
