/*
 * time_slider.cc
 * Copyright 2014 John Lindgren
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

#include "time_slider.h"

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include <QMouseEvent>
#include <QProxyStyle>
#include <QStyle>

TimeSliderLabel::TimeSliderLabel(QWidget * parent) : QLabel(parent) {}
TimeSliderLabel::~TimeSliderLabel() {}

void TimeSliderLabel::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
    {
        aud_toggle_bool("qtui", "show_remaining_time");
        hook_call("qtui toggle remaining time", nullptr);
        event->accept();
    }

    QLabel::mouseDoubleClickEvent(event);
}

class TimeSliderStyle : public QProxyStyle
{
public:
    int styleHint(QStyle::StyleHint hint, const QStyleOption * option = nullptr,
                  const QWidget * widget = nullptr,
                  QStyleHintReturn * returnData = nullptr) const
    {
        int styleHint =
            QProxyStyle::styleHint(hint, option, widget, returnData);

        if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
            styleHint |= Qt::LeftButton;

        return styleHint;
    }
};

TimeSlider::TimeSlider(QWidget * parent)
    : QSlider(Qt::Horizontal, parent), m_label(new TimeSliderLabel(parent))
{
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    auto style = new TimeSliderStyle;
    style->setParent(this);
    setStyle(style);

    m_label->setContentsMargins(audqt::sizes.FourPt, 0, 0, 0);
    m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    connect(this, &QSlider::sliderMoved, this, &TimeSlider::moved);
    connect(this, &QSlider::sliderPressed, this, &TimeSlider::pressed);
    connect(this, &QSlider::sliderReleased, this, &TimeSlider::released);

    start_stop();
}

void TimeSlider::set_label(int time, int length)
{
    QString text;

    if (length >= 0)
    {
        auto length_str = str_format_time(length);
        auto time_pad = length_str.len();

        QString time_str;
        if (aud_get_bool("qtui", "show_remaining_time"))
        {
            time = aud::max(0, length - time);
            time_str = QString('-') + str_format_time(time);
            time_pad++;
        }
        else
            time_str = str_format_time(time);

        int a, b;
        aud_drct_get_ab_repeat(a, b);

        QString ab_str;
        if (a >= 0)
            ab_str += " A=<tt>" + QString(str_format_time(a)) + "</tt>";
        if (b >= 0)
            ab_str += " B=<tt>" + QString(str_format_time(b)) + "</tt>";

        // To avoid the label changing width as time progresses, use
        // monospaced digits and pad the time to the width of the song
        // length (which should be the widest time we'll display).
        text = "<b><tt>" + time_str.rightJustified(time_pad, QChar::Nbsp) +
               "</tt> / <tt>" + length_str + "</tt>" + ab_str + "</b>";
    }
    else
        text = "<b><tt>" + QString(str_format_time(time)) + "</tt></b>";

    m_label->setText(text);
}

void TimeSlider::start_stop()
{
    bool ready = aud_drct_get_ready();
    bool paused = aud_drct_get_paused();

    m_label->setEnabled(ready);

    update();

    if (ready && !paused)
        m_timer.start();
    else
        m_timer.stop();
}

void TimeSlider::update()
{
    if (aud_drct_get_ready())
    {
        if (!isSliderDown())
        {
            int time = aud_drct_get_time();
            int length = aud_drct_get_length();

            setEnabled(length >= 0);
            setRange(0, length);
            setValue(time);

            set_label(time, length);
        }
    }
    else
    {
        setEnabled(false);
        setRange(0, 0);
        set_label(0, 0);
    }
}

void TimeSlider::moved(int value) { set_label(value, aud_drct_get_length()); }

void TimeSlider::pressed() { set_label(value(), aud_drct_get_length()); }

void TimeSlider::released() { aud_drct_seek(value()); }
