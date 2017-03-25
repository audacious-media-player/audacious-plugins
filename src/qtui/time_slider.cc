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

#include <QMouseEvent>
#include <QStyle>

MyLabel::MyLabel (QWidget * parent) : QLabel (parent) {}
MyLabel::~MyLabel () {}

void MyLabel::mouseDoubleClickEvent (QMouseEvent * event)
{
    aud_toggle_bool ("qtui", "show_remaining_time");
    hook_call ("qtui toggle remaining time", nullptr);

    event->accept ();

    QLabel::mouseDoubleClickEvent (event);
}

TimeSlider::TimeSlider (QWidget * parent) :
    QSlider (Qt::Horizontal, parent),
    m_label (new MyLabel (parent))
{
    setFocusPolicy (Qt::NoFocus);
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_label->setContentsMargins (4, 0, 4, 0);
    m_label->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    connect (this, & QSlider::sliderMoved, this, & TimeSlider::moved);
    connect (this, & QSlider::sliderReleased, this, & TimeSlider::released);

    start_stop ();
}

TimeSlider::~TimeSlider () {}

void TimeSlider::set_label (int time, int length)
{
    QString text;

    if (length >= 0)
        if (aud_get_bool ("qtui", "show_remaining_time"))
            text = str_concat ({str_format_time (time - length), " / ", str_format_time (length)});
        else
            text = str_concat ({str_format_time (time), " / ", str_format_time (length)});
    else
        text = str_format_time (time);

    m_label->setText (text);
}

void TimeSlider::start_stop ()
{
    bool ready = aud_drct_get_ready ();
    bool paused = aud_drct_get_paused ();

    setEnabled (ready);
    m_label->setEnabled (ready);

    update ();

    if (ready && ! paused)
        m_timer.start ();
    else
        m_timer.stop ();
}

void TimeSlider::update ()
{
    if (aud_drct_get_ready ())
    {
        if (! isSliderDown ())
        {
            int time = aud_drct_get_time ();
            int length = aud_drct_get_length ();

            setRange (0, length);
            setValue (time);

            set_label (time, length);
        }
    }
    else
    {
        setRange (0, 0);
        set_label (0, 0);
    }
}

void TimeSlider::moved (int value)
{
    set_label (value, aud_drct_get_length ());
}

void TimeSlider::released ()
{
    aud_drct_seek (value ());
}

void TimeSlider::mousePressEvent (QMouseEvent * event)
{
    if (event->button () == Qt::LeftButton)
    {
        setValue (QStyle::sliderValueFromPosition (minimum (), maximum (), event->x (), width ()));
        event->accept ();
    }

    QSlider::mousePressEvent (event);
}
