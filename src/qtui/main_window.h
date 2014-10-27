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

#include <libaudcore/index.h>
#include <libaudcore/objects.h>

#include "dialog_windows.h"

#include <QMainWindow>
#include <QTimer>

class FilterInput;
class PlaylistTabs;
class PluginHandle;

namespace Ui { class MainWindow; }

struct DockWidget;

class MainWindow : public QMainWindow
{
public:
    MainWindow ();
    ~MainWindow ();

private:
    SmartPtr<Ui::MainWindow> ui;

    DialogWindows m_dialogs;
    FilterInput * filterInput;
    PlaylistTabs * playlistTabs;

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

    void add_dock_plugins ();
    void remove_dock_plugins ();

    static void title_change_cb (void *, MainWindow * window);
    static void playback_begin_cb (void *, MainWindow * window);
    static void playback_ready_cb (void *, MainWindow * window);
    static void pause_cb (void *, MainWindow * window);
    static void playback_stop_cb (void *, MainWindow * window);
    static void update_toggles_cb (void *, MainWindow * window);

    static void add_dock_plugin_cb (PluginHandle * plugin, MainWindow * window);
    static void remove_dock_plugin_cb (PluginHandle * plugin, MainWindow * window);

    Index <DockWidget *> dock_widgets;
};

#endif
