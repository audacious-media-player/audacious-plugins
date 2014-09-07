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
#include <libaudcore/hook.h>

TimeSlider::TimeSlider (QWidget * parent) :
    QSlider (Qt::Horizontal, parent),
    m_label (new QLabel (parent))
{
    setFocusPolicy (Qt::NoFocus);
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_label->setContentsMargins (4, 0, 4, 0);
    m_label->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    connect (& m_timer, & QTimer::timeout, this, & TimeSlider::update);
    connect (this, & QSlider::valueChanged, this, & TimeSlider::moved);
    connect (this, & QSlider::sliderPressed, this, & TimeSlider::pressed);
    connect (this, & QSlider::sliderReleased, this, & TimeSlider::released);

    hook_associate ("playback ready", start_stop_hook, this);
    hook_associate ("playback pause", start_stop_hook, this);
    hook_associate ("playback unpause", start_stop_hook, this);
    hook_associate ("playback stop", start_stop_hook, this);

    start_stop ();
}

TimeSlider::~TimeSlider ()
{
    hook_dissociate_full ("playback ready", start_stop_hook, this);
    hook_dissociate_full ("playback pause", start_stop_hook, this);
    hook_dissociate_full ("playback unpause", start_stop_hook, this);
    hook_dissociate_full ("playback stop", start_stop_hook, this);
}

void TimeSlider::set_label (int time, int length)
{
    QString text;

    if (length > 0)
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

    if (ready)
    {
        if (! isSliderDown ())
            update ();
    }
    else
    {
        setRange (0, 0);
        m_label->setText ("0:00 / 0:00");
    }

    if (ready && ! paused && ! isSliderDown ())
        m_timer.start (250);
    else
        m_timer.stop ();
}

void TimeSlider::update ()
{
    int time = aud_drct_get_time ();
    int length = aud_drct_get_length ();

    setRange (0, length);
    setValue (time);

    set_label (time, length);
}

void TimeSlider::moved (int value)
{
    set_label (value, aud_drct_get_length ());
}

void TimeSlider::pressed ()
{
    m_timer.stop ();
}

void TimeSlider::released ()
{
    aud_drct_seek (value ());
    set_label (value (), aud_drct_get_length ());

    if (! aud_drct_get_paused ())
        m_timer.start (250);
}
