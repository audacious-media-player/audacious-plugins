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
#include <QTimer>
#include <QStyle>
#include <QMouseEvent>

#include <libaudcore/hook.h>

class TimeSlider : public QSlider
{
public:
    TimeSlider (QWidget * parent);
    ~TimeSlider ();

    QLabel * label ()
        { return m_label; }

private:
    void set_label (int time, int length);

    void start_stop ();
    void update ();
    void moved (int value);
    void pressed ();
    void released ();

    void mousePressEvent (QMouseEvent * event);

    QTimer m_timer;
    QLabel * m_label;

    HookReceiver<TimeSlider> hooks[4];
};

#endif // TIME_SLIDER_H
