/*
 * info_bar.cc
 * Copyright 2014 William Pitcock
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <cmath>

#include "info_bar.h"

#include <libaudcore/drct.h>
#include <libaudcore/interface.h>
#include <libaudqt/libaudqt.h>

#include <QPainter>

static constexpr int Spacing = 8;
static constexpr int IconSize = 64;
static constexpr int Height = IconSize + 2 * Spacing;

static constexpr int VisBands = 12;
static constexpr int VisWidth = 8 * VisBands + Spacing - 2;
static constexpr int VisCenter = IconSize * 5 / 8 + Spacing;
static constexpr int VisDelay = 2;
static constexpr int VisFalloff = 2;

class InfoVis : public QWidget, Visualizer
{
public:
    InfoVis (QWidget * parent = nullptr);
    ~InfoVis ();

    void render_freq (const float * freq);
    void clear ();

    void paintEvent (QPaintEvent *);

    const QGradient & gradient () const
        { return m_gradient; }

private:
    QLinearGradient m_gradient;
    QColor m_colors[VisBands], m_shadow[VisBands];

    char m_bars[VisBands] {};
    char m_delay[VisBands] {};
};

static void get_color (int i, QColor & color, QColor & shadow)
{
    color = QWidget ().palette ().color (QPalette::Highlight);

    qreal h, s, v;
    color.getHsvF (& h, & s, & v);

    if (s < 0.1) /* monochrome theme? use blue instead */
        h = 0.67;

    s = 1 - 0.9 * i / (VisBands - 1);
    v = 0.75 + 0.25 * i / (VisBands - 1);

    color.setHsvF (h, s, v);
    shadow = QColor (color.redF () * 77, color.greenF () * 77, color.blueF () * 77);
}

InfoVis::InfoVis (QWidget * parent) :
    QWidget (parent),
    Visualizer (Freq),
    m_gradient (0, 0, 0, Height)
{
    m_gradient.setStops ({
        {0, QColor (64, 64, 64)},
        {0.499, QColor (38, 38, 38)},
        {0.5, QColor (26, 26, 26)},
        {1, QColor (0, 0, 0)}
    });

    for (int i = 0; i < VisBands; i ++)
        get_color (i, m_colors[i], m_shadow[i]);

    setAttribute (Qt::WA_OpaquePaintEvent);
    resize (VisWidth, Height);

    aud_visualizer_add (this);
}

InfoVis::~InfoVis ()
{
    aud_visualizer_remove (this);
}

void InfoVis::render_freq (const float * freq)
{
    /* xscale[i] = pow (256, i / VIS_BANDS) - 0.5; */
    const float xscale[VisBands + 1] = {0.5, 1.09, 2.02, 3.5, 5.85, 9.58,
     15.5, 24.9, 39.82, 63.5, 101.09, 160.77, 255.5};

    for (int i = 0; i < VisBands; i ++)
    {
        int a = ceilf (xscale[i]);
        int b = floorf (xscale[i + 1]);
        float n = 0;

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
        int x = 40 + 20 * log10f (n);
        x = aud::clamp (x, 0, 40);

        m_bars[i] -= aud::max (0, VisFalloff - m_delay[i]);

        if (m_delay[i])
            m_delay[i] --;

        if (x > m_bars[i])
        {
            m_bars[i] = x;
            m_delay[i] = VisDelay;
        }
    }

    repaint ();
}

void InfoVis::clear ()
{
    memset (m_bars, 0, sizeof m_bars);
    memset (m_delay, 0, sizeof m_delay);

    update ();
}

void InfoVis::paintEvent (QPaintEvent *)
{
    QPainter p (this);
    p.fillRect (0, 0, VisWidth, Height, m_gradient);

    for (int i = 0; i < VisBands; i ++)
    {
        int x = 8 * i;
        int v = m_bars[i];
        int m = aud::min (VisCenter + v, Height);

        p.fillRect (x, VisCenter - v, 6, v, m_colors[i]);
        p.fillRect (x, VisCenter, 6, m - VisCenter, m_shadow[i]);
    }
}

InfoBar::InfoBar (QWidget * parent) :
    QWidget (parent),
    m_vis (new InfoVis (this))
{
    setFixedHeight (Height);

    m_title.setTextFormat (Qt::PlainText);
    m_artist.setTextFormat (Qt::PlainText);
    m_album.setTextFormat (Qt::PlainText);

    update_cb ();
}

void InfoBar::resizeEvent (QResizeEvent *)
{
    m_vis->move (width () - VisWidth, 0);
}

void InfoBar::paintEvent (QPaintEvent *)
{
    QPainter p (this);

    p.fillRect (0, 0, width () - VisWidth, Height, m_vis->gradient ());

    if (! m_art.isNull ())
    {
        int r = m_art.devicePixelRatio ();
        int left = Spacing + (IconSize - m_art.width () / r) / 2;
        int top = Spacing + (IconSize - m_art.height () / r) / 2;
        p.drawPixmap (left, top, m_art);
    }

    QFont font = p.font ();
    font.setPointSize (18);
    p.setFont (font);

    p.setPen (QColor (255, 255, 255));
    p.drawStaticText (IconSize + 2 * Spacing, Spacing, m_title);

    font.setPointSize (9);
    p.setFont (font);

    p.drawStaticText (IconSize + 2 * Spacing, Spacing + IconSize / 2, m_artist);

    p.setPen (QColor (179, 179, 179));
    p.drawStaticText (IconSize + 2 * Spacing, Spacing + IconSize * 3 / 4, m_album);
}

void InfoBar::update_metadata_cb ()
{
    Tuple tuple = aud_drct_get_tuple ();
    m_title.setText ((const char *) tuple.get_str (Tuple::Title));
    m_artist.setText ((const char *) tuple.get_str (Tuple::Artist));
    m_album.setText ((const char *) tuple.get_str (Tuple::Album));

    update ();
}

void InfoBar::update_cb ()
{
    m_art = audqt::art_request_current (IconSize, IconSize);
    update_metadata_cb ();
}
