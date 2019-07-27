/*
 *  Blur Scope plugin for Audacious
 *  Copyright (C) 2010-2012 John Lindgren
 *  Copyright (C) 2019 William Pitcock
 *
 *  Based on BMP - Cross-platform multimedia player:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <math.h>
#include <string.h>
#include <glib-2.0/glib.h>

#include <QWidget>
#include <QImage>
#include <QPainter>

#include "libaudqt/colorbutton.h"

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

static void /* QWidget */ * bscope_get_color_chooser ();

static const PreferencesWidget bscope_widgets[] = {
    WidgetLabel (N_("<b>Color</b>")),
    WidgetCustomQt (bscope_get_color_chooser)
};

static const PluginPreferences bscope_prefs = {{bscope_widgets}};

static const char * const bscope_defaults[] = {
 "color", aud::numeric_string<0xFF3F7F>::str,
 nullptr};

static int bscope_color;

class BlurScopeWidget : public QWidget {
public:
    BlurScopeWidget (QWidget * parent = nullptr);
    ~BlurScopeWidget ();

    void resize (int w, int h);

    void clear ();
    void blur ();
    void draw_vert_line (int x, int y1, int y2);

protected:
    void resizeEvent (QResizeEvent *);
    void paintEvent (QPaintEvent *);

private:
    int m_width = 0, m_height = 0, m_image_size = 0;
    uint32_t * m_image = nullptr, * m_corner = nullptr;
};

static BlurScopeWidget *s_widget = nullptr;

BlurScopeWidget::BlurScopeWidget (QWidget * parent) :
    QWidget (parent)
{
    resize (width (), height ());
}

BlurScopeWidget::~BlurScopeWidget ()
{
    g_free(m_image);
    m_image = nullptr;
    s_widget = nullptr;
}

void BlurScopeWidget::paintEvent (QPaintEvent *)
{
    QImage img((unsigned char *) m_image, m_width, m_height, QImage::Format_RGB32);
    QPainter p (this);

    p.drawImage (0, 0, img);
}

void BlurScopeWidget::resizeEvent (QResizeEvent *)
{
    resize (width (), height ());
}

void BlurScopeWidget::resize (int w, int h)
{
    m_width = w;
    m_height = h;
    m_image_size = (m_width << 2) * (m_height + 2);
    m_image = (uint32_t *) g_realloc (m_image, m_image_size);
    memset (m_image, 0, m_image_size);
    m_corner = m_image + m_width + 1;
}

void BlurScopeWidget::clear ()
{
    memset (m_image, 0, m_image_size);
    update ();
}

void BlurScopeWidget::blur ()
{
    for (int y = 0; y < m_height; y ++)
    {
        uint32_t * p = m_corner + m_width * y;
        uint32_t * end = p + m_width;
        uint32_t * plast = p - m_width;
        uint32_t * pnext = p + m_width;

        /* We do a quick and dirty average of four color values, first masking
         * off the lowest two bits.  Over a large area, this masking has the net
         * effect of subtracting 1.5 from each value, which by a happy chance
         * is just right for a gradual fade effect. */
        for (; p < end; p ++)
            * p = ((* plast ++ & 0xFCFCFC) + (p[-1] & 0xFCFCFC) + (p[1] &
             0xFCFCFC) + (* pnext ++ & 0xFCFCFC)) >> 2;
    }
}

void BlurScopeWidget::draw_vert_line (int x, int y1, int y2)
{
    int y, h;

    if (y1 < y2) {y = y1 + 1; h = y2 - y1;}
    else if (y2 < y1) {y = y2; h = y1 - y2;}
    else {y = y1; h = 1;}

    uint32_t * p = m_corner + y * m_width + x;

    for (; h --; p += m_width)
        * p = bscope_color;
}

class BlurScopeQt : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Blur Scope"),
        PACKAGE,
        nullptr,
        & bscope_prefs,
        PluginQtOnly
    };

    constexpr BlurScopeQt () : VisPlugin (info, Visualizer::MonoPCM) {}

    bool init ();
    void cleanup ();

    void * get_qt_widget ();

    void clear ();
    void render_mono_pcm (const float * pcm);
};

EXPORT BlurScopeQt aud_plugin_instance;

bool BlurScopeQt::init ()
{
    aud_config_set_defaults ("BlurScope", bscope_defaults);
    bscope_color = aud_get_int ("BlurScope", "color");

    return true;
}

void BlurScopeQt::cleanup ()
{
    aud_set_int ("BlurScope", "color", bscope_color);
}

void BlurScopeQt::clear ()
{
    if (s_widget)
        s_widget->clear ();
}

void BlurScopeQt::render_mono_pcm (const float * pcm)
{
    g_assert(s_widget);

    s_widget->blur ();

    int width = s_widget->width ();
    int height = s_widget->height ();

    int prev_y = (0.5 + pcm[0]) * height;
    prev_y = aud::clamp (prev_y, 0, height - 1);

    for (int i = 0; i < width; i ++)
    {
        int y = (0.5 + pcm[i * 512 / width]) * height;
        y = aud::clamp (y, 0, height - 1);
        s_widget->draw_vert_line (i, prev_y, y);
        prev_y = y;
    }

    s_widget->update ();
}

void * BlurScopeQt::get_qt_widget ()
{
    if (s_widget)
        return s_widget;

    s_widget = new BlurScopeWidget ();
    return s_widget;
}

class ColorChooserWidget : public audqt::ColorButton
{
protected:
    void onColorChanged () override;
};

void ColorChooserWidget::onColorChanged ()
{
    QColor col (color ());
    int r, g, b;

    col.getRgb(&r, &g, &b);

    bscope_color = r << 16 | g << 8 | b;
}

static void * bscope_get_color_chooser ()
{
    int r, g, b;

    r = (bscope_color & 0xff0000) >> 16;
    g = (bscope_color & 0xff00) >> 8;
    b = (bscope_color & 0xff);

    QColor color = QColor::fromRgb(r, g, b);

    auto chooser = new ColorChooserWidget ();
    chooser->setColor (color);

    return chooser;
}
