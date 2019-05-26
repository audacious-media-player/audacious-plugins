/*
 * Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include <string.h>

#include <QWidget>
#include <QPainter>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>

#define MAX_BANDS   (256)
#define VIS_DELAY 2 /* delay before falloff in frames */
#define VIS_FALLOFF 2 /* falloff in pixels per frame */

static float xscale[MAX_BANDS + 1];
static int bands;
static int bars[MAX_BANDS + 1];
static int delay[MAX_BANDS + 1];

static void calculate_xscale ()
{
    for (int i = 0; i <= bands; i ++)
        xscale[i] = powf (256, (float) i / bands) - 0.5f;
}

class SpectrumWidget : public QWidget
{
public:
    SpectrumWidget (QWidget * parent = nullptr);
    ~SpectrumWidget ();

protected:
    void resizeEvent (QResizeEvent *);
    void paintEvent (QPaintEvent *);

private:
    void paint_background (QPainter &);
    void paint_spectrum (QPainter &);
    QColor get_color (int i);
};

static SpectrumWidget * spect_widget = nullptr;

SpectrumWidget::SpectrumWidget (QWidget * parent) :
    QWidget (parent)
{
}

SpectrumWidget::~SpectrumWidget ()
{
    spect_widget = nullptr;
}

QColor SpectrumWidget::get_color (int i)
{
    auto & highlight = palette ().color (QPalette::Highlight);
    qreal h, s, v;

    highlight.getHsvF (& h, & s, & v);

    if (s < 0.1) /* monochrome theme? use blue instead */
        h = 4.6;

    s = 1 - 0.9 * i / (bands - 1);
    v = 0.75 + 0.25 * i / (bands - 1);

    return QColor::fromHsvF (h, s, v);
}

void SpectrumWidget::paint_background (QPainter & p)
{
    auto & base = palette ().color (QPalette::Window);
    p.fillRect (0, 0, width (), height (), base);
}

void SpectrumWidget::paint_spectrum (QPainter & p)
{
    for (int i = 0; i < bands; i++)
    {
        int x = ((width () / bands) * i) + 2;
        auto color = get_color (i);

        p.fillRect (x + 1, height () - (bars[i] * height () / 40),
         (width () / bands) - 1, (bars[i] * height () / 40), color);
    }
}

void SpectrumWidget::resizeEvent (QResizeEvent * event)
{
    bands = width () / 10;
    bands = aud::clamp(bands, 12, MAX_BANDS);
    calculate_xscale ();
    update ();
}

void SpectrumWidget::paintEvent (QPaintEvent * event)
{
    QPainter p (this);

    paint_background (p);
    paint_spectrum (p);
}

class QtSpectrum : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Spectrum Analyzer"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginQtOnly
    };

    constexpr QtSpectrum () : VisPlugin (info, Visualizer::Freq) {}

    void * get_qt_widget ();

    void clear ();
    void render_freq (const float * freq);
};

EXPORT QtSpectrum aud_plugin_instance;

void QtSpectrum::render_freq (const float * freq)
{
    if (! bands)
        return;

    for (int i = 0; i < bands; i ++)
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

        /* fudge factor to make the graph have the same overall height as a
           12-band one no matter how many bands there are */
        n *= (float) bands / 12;

        /* 40 dB range */
        int x = 40 + 20 * log10f (n);
        x = aud::clamp (x, 0, 40);

        bars[i] -= aud::max (0, VIS_FALLOFF - delay[i]);

        if (delay[i])
            delay[i]--;

        if (x > bars[i])
        {
            bars[i] = x;
            delay[i] = VIS_DELAY;
        }
    }

    if (spect_widget)
        spect_widget->update ();
}

void QtSpectrum::clear ()
{
    memset (bars, 0, sizeof bars);
    memset (delay, 0, sizeof delay);

    if (spect_widget)
        spect_widget->update ();
}

void * QtSpectrum::get_qt_widget ()
{
    if (spect_widget)
        return spect_widget;

    spect_widget = new SpectrumWidget ();
    return spect_widget;
}
