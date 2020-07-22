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
#include <libaudqt/libaudqt.h>

#define MAX_BANDS   (256)
#define VIS_DELAY 2 /* delay before falloff in frames */
#define VIS_FALLOFF 2 /* falloff in pixels per frame */

static float xscale[MAX_BANDS + 1];
static int bands;
static int bars[MAX_BANDS + 1];
static int delay[MAX_BANDS + 1];

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
        auto color = audqt::vis_bar_color (palette ().color (QPalette::Highlight), i, bands);

        p.fillRect (x + 1, height () - (bars[i] * height () / 40),
         (width () / bands) - 1, (bars[i] * height () / 40), color);
    }
}

void SpectrumWidget::resizeEvent (QResizeEvent * event)
{
    bands = width () / 10;
    bands = aud::clamp(bands, 12, MAX_BANDS);
    Visualizer::compute_log_xscale (xscale, bands);
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
        /* 40 dB range */
        int x = 40 + compute_freq_band (freq, xscale, i, bands);
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
