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

#ifndef __WAVEFORM_WIDGET_H__
#define __WAVEFORM_WIDGET_H__

#include <QTimer>
#include <QWidget>

struct TrackPeaks;

class WaveformWidget : public QWidget
{
public:
    explicit WaveformWidget(QWidget * parent = nullptr);
    ~WaveformWidget() override;

    /* Takes ownership of `peaks` and makes it the displayed envelope. */
    void apply_peaks(const char * filename, TrackPeaks * peaks);

protected:
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

private:
    void paint_background(QPainter &);
    void paint_waveform(QPainter &);
    void paint_playhead(QPainter &);
    void seek_to_x(int x);

    QTimer * m_timer;
};

/* Asynchronously loads (or decodes+caches) the peak envelope for
 * `filename` and applies it to the active WaveformWidget, if any. */
void load_track_async(const char * filename);

/* The currently active WaveformWidget instance, or nullptr if none has
 * been created (or it has since been destroyed by the host). */
WaveformWidget * waveform_get_active_widget();

/* Returns the outer container widget that holds the waveform view,
 * creating it on first call. Subsequent calls return the same
 * instance until the host destroys it. */
QWidget * waveform_get_container_widget();

#endif
