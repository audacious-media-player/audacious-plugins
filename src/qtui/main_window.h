/*
 * main_window.h
 * Copyright 2014 Michał Lipski
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

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QLabel>
#include <QSlider>
#include <QTimer>

#include <libaudcore/drct.h>

#include "ui_main_window.h"

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    MainWindow (QMainWindow * parent = 0);
    ~MainWindow ();

public slots:
    void timeCounterSlot ();
    void sliderValueChanged (int value);
    void sliderPressed ();
    void sliderReleased ();

private:
    QLabel * timeCounterLabel;
    QTimer * timeCounter;
    QSlider * slider;
    void setTimeCounterLabel (int time, int length);
    void enableSlider ();
    void disableSlider ();
    void enableTimeCounter ();
    void disableTimeCounter ();
    void populatePlaylists ();

    static void title_change_cb (void * unused, MainWindow * window)
    {
        auto title = aud_drct_get_title ();
        if (title)
            window->setWindowTitle (QString ("Audacious - ") + QString (title));
    }

    static void playback_begin_cb (void * unused, MainWindow * window)
    {
        window->setWindowTitle ("Audacious - Buffering...");

        pause_cb (NULL, window);
    }

    static void playback_ready_cb (void * unused, MainWindow * window)
    {
        title_change_cb (NULL, window);
        pause_cb (NULL, window);

        window->enableSlider ();
        window->enableTimeCounter ();
    }

    static void pause_cb (void * unused, MainWindow * window)
    {
        if (aud_drct_get_paused ())
            window->actionPlayPause->setIcon (QIcon::fromTheme ("media-playback-start"));
        else
            window->actionPlayPause->setIcon (QIcon::fromTheme ("media-playback-pause"));
    }

    static void playback_stop_cb (void * unused, MainWindow * window)
    {
        window->setWindowTitle ("Audacious");
        window->disableTimeCounter ();
        window->disableSlider ();

        window->actionPlayPause->setIcon (QIcon::fromTheme ("media-playback-start"));
    }
};

#endif
