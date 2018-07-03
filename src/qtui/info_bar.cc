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
#include "settings.h"

#include <libaudcore/drct.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include <QEvent>
#include <QPainter>

static constexpr int FadeSteps = 10;

static constexpr int VisBands = 12;
static constexpr int VisDelay = 2; /* delay before falloff in frames */
static constexpr int VisFalloff = 2; /* falloff in decibels per frame */

struct BarColors
{
    QColor main, shadow;
};

struct PixelSizes
{
    int Spacing, IconSize, Height, BandWidth, BandSpacing, VisWidth, VisScale, VisCenter;

    PixelSizes (int dpi) :
        Spacing (aud::rescale (dpi, 12, 1)),
        IconSize (2 * aud::rescale (dpi, 3, 1)), // should be divisible by 2
        Height (IconSize + 2 * Spacing),
        BandWidth (aud::rescale (dpi, 16, 1)),
        BandSpacing (aud::rescale (dpi, 48, 1)),
        VisWidth (VisBands * (BandWidth + BandSpacing) - BandSpacing + 2 * Spacing),
        VisScale (aud::rescale (IconSize, 8, 5)),
        VisCenter (VisScale + Spacing) {}
};

class InfoVis : public QWidget, Visualizer
{
public:
    InfoVis (QWidget * parent = nullptr);
    ~InfoVis ();

    void enable (bool enabled);

    const QGradient & gradient () const
        { return m_gradient; }
    const PixelSizes & pixelSizes () const
        { return ps; }

protected:
    void render_freq (const float * freq);
    void clear ();

    void changeEvent (QEvent * event);
    void paintEvent (QPaintEvent *);

private:
    void update_colors ();

    const PixelSizes ps;
    QLinearGradient m_gradient;
    BarColors m_bar_colors[VisBands];

    float m_bars[VisBands] {};
    char m_delay[VisBands] {};
};

static BarColors get_bar_colors (const QColor & highlight, int i)
{
    qreal h, s, v;
    highlight.getHsvF (& h, & s, & v);

    if (s < 0.1) /* monochrome theme? use blue instead */
        h = 0.67;

    s = 1 - 0.9 * i / (VisBands - 1);
    v = 0.75 + 0.25 * i / (VisBands - 1);

    return {
        QColor::fromHsvF (h, s, v),
        QColor::fromHsvF (h, s, v * 0.3)
    };
}

static QGradientStops get_gradient_stops (const QColor & base)
{
    QColor mid = QColor (64, 64, 64);
    QColor dark = QColor (38, 38, 38);
    QColor darker = QColor (26, 26, 26);

    /* In a dark theme, try to match the tone of the base color */
    int v = base.value ();
    if (v >= 10 && v < 80)
    {
        int r = base.red (), g = base.green (), b = base.blue ();
        mid = QColor (r * 64 / v, g * 64 / v, b * 64 / v);
        dark = QColor (r * 38 / v, g * 38 / v, b * 38 / v);
        darker = QColor (r * 26 / v, g * 26 / v, b * 26 / v);
    }

    return {
        {0, mid},
        {0.499, dark},
        {0.5, darker},
        {1, Qt::black}
    };
}

void InfoVis::update_colors ()
{
    auto & base = palette ().color (QPalette::Window);
    auto & highlight = palette ().color (QPalette::Highlight);

    m_gradient.setStops (get_gradient_stops (base));

    for (int i = 0; i < VisBands; i ++)
        m_bar_colors[i] = get_bar_colors (highlight, i);
}

InfoVis::InfoVis (QWidget * parent) :
    QWidget (parent),
    Visualizer (Freq),
    ps (audqt::sizes.OneInch),
    m_gradient (0, 0, 0, ps.Height)
{
    update_colors ();
    setAttribute (Qt::WA_OpaquePaintEvent);
    resize (ps.VisWidth + 2 * ps.Spacing, ps.Height);
}

InfoVis::~InfoVis ()
{
    enable (false);
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
        float x = 40 + 20 * log10f (n);

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

void InfoVis::changeEvent (QEvent * event)
{
    if (event->type () == QEvent::PaletteChange)
        update_colors ();

    QWidget::changeEvent (event);
}

void InfoVis::paintEvent (QPaintEvent *)
{
    QPainter p (this);
    p.fillRect (0, 0, ps.VisWidth, ps.Height, m_gradient);

    for (int i = 0; i < VisBands; i ++)
    {
        int x = ps.Spacing + i * (ps.BandWidth + ps.BandSpacing);
        int v = aud::clamp ((int) (m_bars[i] * ps.VisScale / 40), 0, ps.VisScale);
        int m = aud::min (ps.VisCenter + v, ps.Height);

        p.fillRect (x, ps.VisCenter - v, ps.BandWidth, v, m_bar_colors[i].main);
        p.fillRect (x, ps.VisCenter, ps.BandWidth, m - ps.VisCenter, m_bar_colors[i].shadow);
    }
}

void InfoVis::enable (bool enabled)
{
    if (enabled)
        aud_visualizer_add (this);
    else
    {
        aud_visualizer_remove (this);
        clear ();
    }
}

InfoBar::InfoBar (QWidget * parent) :
    QWidget (parent),
    m_vis (new InfoVis (this)),
    ps (m_vis->pixelSizes ()),
    m_stopped (true)
{
    update_vis ();
    setFixedHeight (ps.Height);

    for (SongData & d : sd)
    {
        d.title.setTextFormat (Qt::PlainText);
        d.artist.setTextFormat (Qt::PlainText);
        d.album.setTextFormat (Qt::PlainText);
        d.alpha = 0;
    }

    if (aud_drct_get_ready ())
    {
        m_stopped = false;
        update_title ();
        update_album_art ();

        /* skip fade-in */
        sd[Cur].alpha = FadeSteps;
    }
}

void InfoBar::resizeEvent (QResizeEvent *)
{
    for (SongData & d : sd)
        d.title.setText (QString ());

    m_vis->move (width () - ps.VisWidth, 0);
}

void InfoBar::paintEvent (QPaintEvent *)
{
    QPainter p (this);

    p.fillRect (0, 0, width () - ps.VisWidth, ps.Height, m_vis->gradient ());

    for (SongData & d : sd)
    {
        p.setOpacity ((qreal) d.alpha / FadeSteps);

        if (! d.art.isNull ())
        {
            auto sz = d.art.size () / d.art.devicePixelRatio ();
            int left = ps.Spacing + (ps.IconSize - sz.width ()) / 2;
            int top = ps.Spacing + (ps.IconSize - sz.height ()) / 2;
            p.drawPixmap (left, top, d.art);
        }

        QFont font = p.font ();
        font.setPointSize (18);
        p.setFont (font);

        if (d.title.text ().isNull () && ! d.orig_title.isNull ())
        {
            QFontMetrics metrics = p.fontMetrics ();
            d.title = QStaticText(metrics.elidedText (d.orig_title, Qt::ElideRight,
             width () - ps.VisWidth - ps.Height - ps.Spacing));
        }

        p.setPen (QColor (255, 255, 255));
        p.drawStaticText (ps.Height, ps.Spacing, d.title);

        font.setPointSize (9);
        p.setFont (font);

        p.drawStaticText (ps.Height, ps.Spacing + ps.IconSize / 2, d.artist);

        p.setPen (QColor (179, 179, 179));
        p.drawStaticText (ps.Height, ps.Spacing + ps.IconSize * 3 / 4, d.album);
    }
}

void InfoBar::update_title ()
{
    Tuple tuple = aud_drct_get_tuple ();

    sd[Cur].title.setText (QString ());
    sd[Cur].orig_title = tuple.get_str (Tuple::Title);
    sd[Cur].artist.setText ((const char *) tuple.get_str (Tuple::Artist));
    sd[Cur].album.setText ((const char *) tuple.get_str (Tuple::Album));

    update ();
}

void InfoBar::update_album_art ()
{
    sd[Cur].art = audqt::art_request_current (ps.IconSize, ps.IconSize);
}

void InfoBar::next_song ()
{
    sd[Prev] = std::move (sd[Cur]);
    sd[Cur].alpha = 0;
}

void InfoBar::do_fade ()
{
    bool done = true;

    if (aud_drct_get_playing () && sd[Cur].alpha < FadeSteps)
    {
        sd[Cur].alpha ++;
        done = false;
    }

    if (sd[Prev].alpha > 0)
    {
        sd[Prev].alpha --;
        done = false;
    }

    update ();

    if (done)
        fade_timer.stop ();
}

void InfoBar::playback_ready_cb ()
{
    if (! m_stopped)
        next_song ();

    m_stopped = false;
    update_title ();
    update_album_art ();

    update ();
    fade_timer.start ();
}

void InfoBar::playback_stop_cb ()
{
    next_song ();
    m_stopped = true;

    update ();
    fade_timer.start ();
}

void InfoBar::update_vis ()
{
    m_vis->enable (aud_get_bool ("qtui", "infoarea_show_vis"));
}
