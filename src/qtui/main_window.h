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

#include "ui_main_window.h"

#include <QLabel>
#include <QMessageBox>

class FilterInput;
class TimeSlider;

class MainWindow : public QMainWindow, private Ui::MainWindow
{
public:
    MainWindow (QMainWindow * parent = 0);
    ~MainWindow ();

private:
    void closeEvent (QCloseEvent * e);
    void keyPressEvent (QKeyEvent * e);

    QLabel * codecInfoLabel = nullptr;
    QLabel * playlistLengthLabel = nullptr;
    TimeSlider * slider = nullptr;
    FilterInput * filterInput = nullptr;
    QMessageBox * progressDialog = nullptr;
    QMessageBox * errorDialog = nullptr;

    void createProgressDialog ();
    void createErrorDialog (const QString &message);
    void updateToggles ();
    void setupActions ();
    void createStatusBar ();

    void action_play_pause_set_play ();
    void action_play_pause_set_pause ();

    static void title_change_cb (void *, MainWindow * window);
    static void playback_begin_cb (void *, MainWindow * window);
    static void playback_ready_cb (void *, MainWindow * window);
    static void pause_cb (void *, MainWindow * window);
    static void playback_stop_cb (void *, MainWindow * window);
    static void update_toggles_cb (void *, MainWindow * window);
    static void show_progress_cb (void * message, MainWindow * window);
    static void show_progress_2_cb (void * message, MainWindow * window);
    static void hide_progress_cb (void *, MainWindow * window);
    static void show_error_cb (void * message, MainWindow * window);
    static void update_playlist_length_cb (void *, QLabel * label);
    static void update_codec_info_cb (void *, QLabel * label);
};

#endif
