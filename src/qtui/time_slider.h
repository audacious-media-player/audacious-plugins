/*
 * time_slider.h
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

#ifndef TIME_SLIDER_H
#define TIME_SLIDER_H

#include <QLabel>
#include <QSlider>

#include <libaudcore/hook.h>

class QMouseEvent;

class MyLabel : public QLabel
{
public:
    MyLabel (QWidget * parent);
    ~MyLabel ();

protected:
     void mouseDoubleClickEvent (QMouseEvent * event);
};

class TimeSlider : public QSlider
{
public:
    TimeSlider (QWidget * parent);
    ~TimeSlider ();

    MyLabel * label ()
        { return m_label; }

private:
    void set_label (int time, int length);

    void start_stop ();
    void update ();
    void moved (int value);
    void pressed ();
    void released ();

    void mousePressEvent (QMouseEvent * event);

    MyLabel * m_label;

    const Timer<TimeSlider>
     m_timer {TimerRate::Hz4, this, & TimeSlider::update};

    const HookReceiver<TimeSlider>
     hook1 {"playback ready", this, & TimeSlider::start_stop},
     hook2 {"playback pause", this, & TimeSlider::start_stop},
     hook3 {"playback unpause", this, & TimeSlider::start_stop},
     hook4 {"playback seek", this, & TimeSlider::update},
     hook5 {"playback stop", this, & TimeSlider::start_stop},
     hook6 {"qtui toggle remaining time", this, & TimeSlider::start_stop};
};

#endif
