/*
 * main_window.cc
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

#include <QtGui>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>

#include "main_window.h"
#include "main_window.moc"
#include "utils.h"

MainWindow::MainWindow (QMainWindow * parent) : QMainWindow (parent)
{
    setupUi (this);

    this->setUnifiedTitleAndToolBarOnMac (true);

    slider = new QSlider (Qt::Horizontal);

    timeCounterLabel = new QLabel ("0:00 / 0:00");
    timeCounterLabel->setContentsMargins (5, 0, 0, 2);

    timeCounter = new QTimer;

    toolBar->insertWidget (actionAudioVolume, slider);
    toolBar->insertWidget (actionAudioVolume, timeCounterLabel);

    connect (actionOpen,      &QAction::triggered, Utils::openFilesDialog);
    connect (actionAdd,       &QAction::triggered, Utils::addFilesDialog);
    connect (actionPlayPause, &QAction::triggered, aud_drct_play_pause);
    connect (actionStop,      &QAction::triggered, aud_drct_stop);
    connect (actionPrevious,  &QAction::triggered, aud_drct_pl_prev);
    connect (actionNext,      &QAction::triggered, aud_drct_pl_next);

    hook_associate ("title change",     (HookFunction) title_change_cb, this);
    hook_associate ("playback begin",   (HookFunction) playback_begin_cb, this);
    hook_associate ("playback ready",   (HookFunction) playback_ready_cb, this);
    hook_associate ("playback pause",   (HookFunction) pause_cb, this);
    hook_associate ("playback unpause", (HookFunction) pause_cb, this);
    hook_associate ("playback stop",    (HookFunction) playback_stop_cb, this);
}

MainWindow::~MainWindow ()
{
    hook_dissociate ("title change",     (HookFunction) title_change_cb);
    hook_dissociate ("playback begin",   (HookFunction) playback_begin_cb);
    hook_dissociate ("playback ready",   (HookFunction) playback_ready_cb);
    hook_dissociate ("playback pause",   (HookFunction) pause_cb);
    hook_dissociate ("playback unpause", (HookFunction) pause_cb);
    hook_dissociate ("playback stop",    (HookFunction) playback_stop_cb);

    delete slider;
    delete timeCounterLabel;
    delete timeCounter;
}

void MainWindow::timeCounterSlot ()
{
    int time   = aud_drct_get_time ();
    int length = aud_drct_get_length ();
    QString text = QString (str_format_time (time)) + QString (" / ") + QString (str_format_time (length));
    timeCounterLabel->setText (text);

    slider->setRange (0, length);
    slider->setValue (time);
}
