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
#include "dialog_windows.h"
#include "playlist_tabs.h"
#include "tool_bar.h"

#include <QTimer>

class FilterInput;

class MainWindow : public QMainWindow, private Ui::MainWindow
{
public:
    MainWindow ();
    ~MainWindow ();

private:
    DialogWindows m_dialogs;
    FilterInput * filterInput;
    PlaylistTabs * playlistTabs;
    ToolBar * toolBar;

    QAction * toolButtonPlayPause;
    QAction * toolButtonRepeat;
    QAction * toolButtonShuffle;

    QTimer buffering_timer;

    void closeEvent (QCloseEvent * e);
    void keyPressEvent (QKeyEvent * e);

    void updateToggles ();
    void setupActions ();

    void action_play_pause_set_play ();
    void action_play_pause_set_pause ();

    void show_buffering ();

    static void title_change_cb (void *, MainWindow * window);
    static void playback_begin_cb (void *, MainWindow * window);
    static void playback_ready_cb (void *, MainWindow * window);
    static void pause_cb (void *, MainWindow * window);
    static void playback_stop_cb (void *, MainWindow * window);
    static void update_toggles_cb (void *, MainWindow * window);
};

#endif
