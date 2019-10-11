/*
 * Copyright (c) 2017-2019 Marc Sanchez Fauste.
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

#ifndef __VUMETER_QT_WIDGET_H
#define __VUMETER_QT_WIDGET_H

#include <QWidget>
#include <QPainter>
#include <QLinearGradient>
#include <QColor>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

class VUMeterQtWidget : public QWidget
{
private:
    static constexpr int max_channels = 20;
    static constexpr int db_range = 96;

    static const QColor backgroundColor;
    static const QColor text_color;
    static const QColor db_line_color;
    static const float legend_line_width;
    static const int redraw_interval;

    int nchannels = 2;
    float channels_db_level[max_channels];
    float channels_peaks[max_channels];
    QElapsedTimer last_peak_times[max_channels]; // Time elapsed since peak was set
    QLinearGradient vumeter_pattern;
    QLinearGradient background_vumeter_pattern;
    float legend_width;
    float vumeter_height;
    float vumeter_width;
    float vumeter_top_padding;
    float vumeter_bottom_padding;
    bool must_draw_vu_legend;
    QTimer *redraw_timer;
    QElapsedTimer redraw_elapsed_timer;

    void draw_background (QPainter &p);
    void draw_visualizer (QPainter &p);
    void draw_vu_legend(QPainter &p);
    float get_height_from_db(float db);
    float get_y_from_db(float db);
    QLinearGradient get_vumeter_pattern(int alpha = 255);
    float get_bar_width(int channels);
    void draw_vu_legend_db(QPainter &p, float db, const char *text);
    void draw_vu_legend_line(QPainter &p, float db, float line_width_factor = 1.0f);
    void draw_visualizer_peaks(QPainter &p);
    void update_sizes();

    static QString format_db(const float val);
    static float get_db_on_range(float db);
    static float get_db_factor(float db);

public slots:
    void redraw_timer_expired();

public:
    VUMeterQtWidget (QWidget * parent = nullptr);
    ~VUMeterQtWidget ();

    void reset ();
    void render_multi_pcm (const float * pcm, int channels);
    void toggle_display_legend();

protected:
    void resizeEvent (QResizeEvent *);
    void paintEvent (QPaintEvent *);
};

#endif
