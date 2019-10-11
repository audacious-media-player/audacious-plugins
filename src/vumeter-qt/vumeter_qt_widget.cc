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

#include "vumeter_qt_widget.h"

#include <math.h>
#include <libaudcore/runtime.h>

const QColor VUMeterQtWidget::backgroundColor = QColor(16, 16, 16, 255);
const QColor VUMeterQtWidget::text_color = QColor(255, 255, 255);
const QColor VUMeterQtWidget::db_line_color = QColor(120, 120, 120);
const float VUMeterQtWidget::legend_line_width = 1.0f;
const int VUMeterQtWidget::redraw_interval = 25; // ms

float VUMeterQtWidget::get_db_on_range(float db)
{
    return aud::clamp<float>(db, -db_range, 0);
}

float VUMeterQtWidget::get_db_factor(float db)
{
    float factor = 0.0f;

    if (db < -db_range)
    {
        factor = 0.0f;
    }
    else if (db < -60.0f)
    {
        factor = (db + db_range) * 2.5f/(db_range-60);
    }
    else if (db < -50.0f)
    {
        factor = (db + 60.0f) * 0.5f + 2.5f;
    }
    else if (db < -40.0f)
    {
        factor = (db + 50.0f) * 0.75f + 7.5f;
    }
    else if (db < -30.0f)
    {
        factor = (db + 40.0f) * 1.5f + 15.0f;
    }
    else if (db < -20.0f)
    {
        factor = (db + 30.0f) * 2.0f + 30.0f;
    }
    else if (db < 0.0f)
    {
        factor = (db + 20.0f) * 2.5f + 50.0f;
    }
    else
    {
        factor = 100.0f;
    }

    return factor / 100.0f;
}

float VUMeterQtWidget::get_height_from_db(float db)
{
    return get_db_factor(db) * vumeter_height;
}

float VUMeterQtWidget::get_y_from_db(float db)
{
    return vumeter_top_padding + vumeter_height - get_height_from_db(db);
}

void VUMeterQtWidget::render_multi_pcm (const float * pcm, int channels)
{
    nchannels = aud::clamp(channels, 0, max_channels);

    float peaks[channels];
    for (int channel = 0; channel < channels; channel++)
    {
        peaks[channel] = fabsf(pcm[channel]);
    }

    for (int i = 0; i < 512 * channels;)
    {
        for (int channel = 0; channel < channels; channel++)
        {
            peaks[channel] = fmaxf(peaks[channel], fabsf(pcm[i++]));
        }
    }

    for (int i = 0; i < nchannels; i++)
    {
        float n = peaks[i];

        float db = 20 * log10f(n);
        db = get_db_on_range(db);

        if (db > channels_db_level[i])
        {
            channels_db_level[i] = db;
        }

        if (db > channels_peaks[i])
        {
            channels_peaks[i] = db;
            last_peak_times[i].start();
        }
    }
}

void VUMeterQtWidget::redraw_timer_expired()
{
    qint64 elapsed_render_time = redraw_elapsed_timer.restart();
    float falloff = aud_get_double ("vumeter", "falloff") / 1000.0;
    qint64 peak_hold_time = aud_get_double ("vumeter", "peak_hold_time") * 1000;

    for (int i = 0; i < nchannels; i++)
    {
        float decay_amount = elapsed_render_time * falloff;
        channels_db_level[i] = get_db_on_range(channels_db_level[i] - decay_amount);

        qint64 elapsed_peak_time = last_peak_times[i].elapsed();
        if (channels_db_level[i] > channels_peaks[i] || elapsed_peak_time > peak_hold_time)
        {
            channels_peaks[i] = channels_db_level[i];
            last_peak_times[i].start();
        }
    }

    update();
}

void VUMeterQtWidget::reset()
{
    for (int i = 0; i < max_channels; i++)
    {
        last_peak_times[i].start();
        channels_db_level[i] = -db_range;
        channels_peaks[i] = -db_range;
    }
}

void VUMeterQtWidget::draw_background(QPainter & p)
{
    p.fillRect(0, 0, width(), height(), backgroundColor);
}

void VUMeterQtWidget::draw_vu_legend(QPainter & p)
{
    float font_size_width = legend_width / 4.0f;
    float font_size_height = vumeter_height * 0.015f;

    QFont font = p.font();
    font.setPointSizeF(fminf(font_size_width, font_size_height));
    p.setFont(font);

    QPen pen = p.pen();
    pen.setWidth(legend_line_width);
    pen.setColor(text_color);
    p.setPen(pen);

    draw_vu_legend_db(p, 0, "0");
    draw_vu_legend_db(p, -3, "-3");
    draw_vu_legend_db(p, -6, "-6");
    draw_vu_legend_db(p, -9, "-9");
    draw_vu_legend_db(p, -12, "-12");
    draw_vu_legend_db(p, -15, "-15");
    draw_vu_legend_db(p, -18, "-18");
    draw_vu_legend_db(p, -20, "-20");
    draw_vu_legend_db(p, -25, "-25");
    draw_vu_legend_db(p, -30, "-30");
    draw_vu_legend_db(p, -35, "-35");
    draw_vu_legend_db(p, -40, "-40");
    draw_vu_legend_db(p, -50, "-50");
    draw_vu_legend_db(p, -60, "-60");
    draw_vu_legend_db(p, -db_range, "-inf");

    pen.setColor(db_line_color);
    p.setPen(pen);
    for (int i = 0; i >= -60; i--)
    {
        if (i > -30)
        {
            draw_vu_legend_line(p, i);
            draw_vu_legend_line(p, i - 0.5, 0.5);
        }
        else if (i > -40)
        {
            draw_vu_legend_line(p, i);
        }
        else if (i >= -60)
        {
            draw_vu_legend_line(p, i);
            i -= 1;
        }
    }
    draw_vu_legend_line(p, -db_range);
}

void VUMeterQtWidget::draw_vu_legend_line(QPainter &p, float db, float line_width_factor)
{
    float y = get_y_from_db(db);
    if (db > -db_range)
    {
        y += (legend_line_width / 2.0f);
    }
    else
    {
        y -= (legend_line_width / 2.0f);
    }
    float line_width = aud::clamp<float>(legend_width * 0.25f, 1, 8);
    p.drawLine(
        QPointF(legend_width - line_width * line_width_factor - (legend_line_width / 2.0f), y),
        QPointF(legend_width - (legend_line_width / 2.0f), y)
    );
    p.drawLine(
        QPointF(width() - legend_width + (legend_line_width / 2.0f), y),
        QPointF(width() - legend_width + (legend_line_width / 2.0f) + line_width * line_width_factor, y)
    );
}

void VUMeterQtWidget::draw_vu_legend_db(QPainter &p, float db, const char *text)
{
    QFontMetricsF fm(p.font());
    QSizeF text_size = fm.size(0, text);
    float y = get_y_from_db(db);
    float padding = aud::clamp<float>(legend_width * 0.25f, 1, 8) * 1.5f;
    p.drawText(QPointF(legend_width - text_size.width() - padding, y + (text_size.height()/4.0f)), text);
    p.drawText(QPointF(width() - legend_width + padding, y + (text_size.height()/4.0f)), text);
}

void VUMeterQtWidget::draw_visualizer_peaks(QPainter &p)
{
    float bar_width = get_bar_width(nchannels);
    float font_size_width = bar_width / 3.0f;
    float font_size_height = vumeter_top_padding * 0.50f;

    QFont font = p.font();
    font.setPointSizeF(fminf(font_size_width, font_size_height));
    p.setFont(font);

    QPen pen = p.pen();
    pen.setColor(text_color);
    p.setPen(pen);

    QFontMetricsF fm(p.font());
    for (int i = 0; i < nchannels; i++)
    {
        QString text = format_db(channels_peaks[i]);
        QSizeF text_size = fm.size(0, text);
        p.drawText(
            QPointF(
                legend_width + bar_width*(i+0.5f) - text_size.width()/2.0f,
                vumeter_top_padding/2.0f + (text_size.height()/4.0f)
            ),
            text
        );
    }
}

void VUMeterQtWidget::draw_visualizer(QPainter & p)
{
    for (int i = 0; i < nchannels; i++)
    {
        float bar_width = get_bar_width(nchannels);
        float x = legend_width + (bar_width * i);
        if (i > 0)
        {
             x += 1;
             bar_width -= 1;
        }

        p.fillRect (
            QRectF(x, vumeter_top_padding, bar_width, vumeter_height),
            background_vumeter_pattern
        );

        p.fillRect (
            QRectF(x, get_y_from_db(channels_db_level[i]),
                bar_width, (get_height_from_db(channels_db_level[i]))),
            vumeter_pattern
        );

        if (channels_peaks[i] > -db_range)
        {
            p.fillRect (
                QRectF(x, get_y_from_db(channels_peaks[i]), bar_width, 1),
                vumeter_pattern
            );
        }
    }
}

QString VUMeterQtWidget::format_db(const float val)
{
    if (val > -10)
    {
        return QString::number(val, 'f', 1);
    }
    else if (val > -db_range)
    {
        return QString::number(val, 'f', 0);
    }
    else
    {
        return QString("-inf");
    }
}

float VUMeterQtWidget::get_bar_width(int channels)
{
    return vumeter_width / channels;
}

void VUMeterQtWidget::update_sizes()
{
    if (height() > 200 && width() > 60 && aud_get_bool("vumeter", "display_legend"))
    {
        must_draw_vu_legend = true;
        vumeter_top_padding = height() * 0.03f;
        vumeter_bottom_padding = height() * 0.015f;
        vumeter_height = height() - vumeter_top_padding - vumeter_bottom_padding;
        legend_width = width() * 0.3f;
        vumeter_width = width() - (legend_width * 2);
    }
    else
    {
        must_draw_vu_legend = false;
        vumeter_top_padding = 0;
        vumeter_bottom_padding = 0;
        vumeter_height = height();
        legend_width = 0;
        vumeter_width = width();
    }
    vumeter_pattern = get_vumeter_pattern();
    background_vumeter_pattern = get_vumeter_pattern(30);
}

VUMeterQtWidget::VUMeterQtWidget (QWidget * parent)
    : QWidget (parent),
    redraw_timer(new QTimer(this))
{
    reset();
    connect(redraw_timer, &QTimer::timeout, this, &VUMeterQtWidget::redraw_timer_expired);
    redraw_timer->start(redraw_interval);
    redraw_elapsed_timer.start();
    update_sizes();
}

QLinearGradient VUMeterQtWidget::get_vumeter_pattern(int alpha)
{
    QLinearGradient vumeter_pattern = QLinearGradient(
        0, vumeter_top_padding + vumeter_height, 0, vumeter_top_padding
    );
    vumeter_pattern.setColorAt(get_db_factor(0), QColor(190, 40, 10, alpha));
    vumeter_pattern.setColorAt(get_db_factor(-2), QColor(190, 40, 10, alpha));
    vumeter_pattern.setColorAt(get_db_factor(-9), QColor(210, 210, 15, alpha));
    vumeter_pattern.setColorAt(get_db_factor(-50), QColor(0, 190, 20, alpha));
    return vumeter_pattern;
}

void VUMeterQtWidget::resizeEvent (QResizeEvent *)
{
    update_sizes();
}

void VUMeterQtWidget::paintEvent (QPaintEvent *)
{
    QPainter p(this);

    draw_background(p);
    if (must_draw_vu_legend)
    {
        draw_vu_legend(p);
        draw_visualizer_peaks(p);
    }
    draw_visualizer(p);
}

void VUMeterQtWidget::toggle_display_legend()
{
    update_sizes();
    update();
}
