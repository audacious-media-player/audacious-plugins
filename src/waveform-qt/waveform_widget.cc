/*
 * Copyright (c) 2026 0000xFFFF.
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

#include "waveform_widget.h"

#include <cstdlib>

#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>

#include "preload.h"
#include "track_peaks.h"
#include "track_state.h"

static WaveformWidget * spect_widget = nullptr;
static QWidget * g_container = nullptr;

WaveformWidget::WaveformWidget(QWidget * parent) : QWidget(parent)
{
    spect_widget = this;

    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, this, [this]() { update(); });
    m_timer->start(100);

    setMinimumHeight(80);

    if (aud_drct_get_ready())
    {
        String fn = aud_drct_get_filename();
        if (fn)
            load_track_async((const char *)fn);
    }
}

WaveformWidget::~WaveformWidget()
{
    spect_widget = nullptr;
    g_container = nullptr;
}

WaveformWidget * waveform_get_active_widget() { return spect_widget; }

void WaveformWidget::apply_peaks(const char * filename, TrackPeaks * peaks)
{
    track_state_set(filename, peaks);
    update();
}

void WaveformWidget::paint_background(QPainter & p)
{
    p.fillRect(0, 0, width(), height(), QColor(18, 18, 20));
}

void WaveformWidget::paint_waveform(QPainter & p)
{
    TrackPeaks local_peaks;
    track_state_copy_into(&local_peaks);

    int w = width(), h = height();
    int center_y = h / 2;

    p.setPen(QColor(90, 90, 95));
    p.drawLine(0, center_y, w, center_y);

    if (local_peaks.num_buckets == 0)
        return;

    double half = h / 2.0;
    double scale = (half * 0.95) / (double)local_peaks.global_peak;
    int n = local_peaks.num_buckets;

    p.setPen(Qt::NoPen);

    /* keep quiet bands faintly visible rather than pure black */
    const int floor_c = 40;

    for (int x = 0; x < w; x++)
    {
        int idx = x * n / w;
        if (idx >= n)
            idx = n - 1;
        double top = center_y - local_peaks.maxs[idx] * scale;
        double bot = center_y - local_peaks.mins[idx] * scale;
        if (bot < top)
        {
            double tmp = top;
            top = bot;
            bot = tmp;
        }

        /* low -> red, mid -> green, high -> blue, the same convention */
        int r = floor_c + (255 - floor_c) * local_peaks.low[idx] / 255;
        int g = floor_c + (255 - floor_c) * local_peaks.mid[idx] / 255;
        int b = floor_c + (255 - floor_c) * local_peaks.high[idx] / 255;

        p.setBrush(QColor(r, g, b));
        p.drawRect(QRectF(x, top, 1.0, bot - top + 1.0));
    }
}

void WaveformWidget::paint_playhead(QPainter & p)
{
    if (!aud_drct_get_playing())
        return;

    int length = aud_drct_get_length();
    if (length <= 0)
        return;

    int time = aud_drct_get_time();
    double frac = (double)time / length;
    if (frac < 0)
        frac = 0;
    if (frac > 1)
        frac = 1;

    double x = frac * width();

    QPen pen(QColor(255, 40, 40));
    pen.setWidth(2);
    p.setPen(pen);
    p.drawLine(QPointF(x, 0), QPointF(x, height()));
}

void WaveformWidget::resizeEvent(QResizeEvent *) { update(); }

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paint_background(p);
    paint_waveform(p);
    paint_playhead(p);
}

void WaveformWidget::seek_to_x(int x)
{
    int length = aud_drct_get_length();
    if (length <= 0 || width() <= 0)
        return;

    double frac = (double)x / width();
    if (frac < 0)
        frac = 0;
    if (frac > 1)
        frac = 1;

    aud_drct_seek((int)(frac * length));
    update();
}

void WaveformWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
        seek_to_x(event->pos().x());
}

void WaveformWidget::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() & Qt::LeftButton)
        seek_to_x(event->pos().x());
}

// async loader thread helper
static void * worker_thread_func(void * arg)
{
    char * filename = (char *)arg;
    TrackPeaks * peaks = new TrackPeaks();

    bool ok = preload_one_file(filename, peaks);
    if (!ok)
    {
        delete peaks;
        free(filename);
        return nullptr;
    }

    /* dynamic copy passed along safely to the Qt event loop lambda */
    char * fn_copy = strdup(filename);
    QMetaObject::invokeMethod(
        qApp,
        [fn_copy, peaks]() {
            if (spect_widget)
                spect_widget->apply_peaks(fn_copy, peaks);
            else
                track_state_set(fn_copy, peaks);
            free(fn_copy);
        },
        Qt::QueuedConnection);

    free(filename);
    return nullptr;
}

void load_track_async(const char * filename)
{
    if (track_state_is_current(filename))
        return;

    char * fn_arg = strdup(filename);
    pthread_t thread;
    if (pthread_create(&thread, nullptr, worker_thread_func, fn_arg) == 0)
    {
        pthread_detach(thread);
    }
    else
    {
        free(fn_arg);
    }
}

QWidget * waveform_get_container_widget()
{
    if (g_container)
        return g_container;

    auto * container = new QWidget();
    auto * layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto * waveform = new WaveformWidget();
    layout->addWidget(waveform);

    g_container = container;
    return container;
}
