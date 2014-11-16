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

#include <libaudcore/hook.h>
#include <libaudcore/index.h>
#include <libaudcore/objects.h>

#include "dialog_windows.h"
#include "info_bar.h"

#include <QMainWindow>
#include <QTimer>
#include <QVBoxLayout>

class FilterInput;
class PlaylistTabs;
class PluginHandle;

struct DockWidget;

class MainWindow : public QMainWindow
{
public:
    MainWindow ();
    ~MainWindow ();

private:
    DialogWindows m_dialogs;
    FilterInput * filterInput;
    PlaylistTabs * playlistTabs;
    InfoBar * infoBar;
    QWidget * centralWidget;
    QVBoxLayout * centralLayout;

    QAction * toolButtonPlayPause;
    QAction * toolButtonRepeat;
    QAction * toolButtonShuffle;

    QTimer buffering_timer;

    void closeEvent (QCloseEvent * e);
    void keyPressEvent (QKeyEvent * e);

    void updateToggles ();
    void setupActions ();
    void readSettings ();

    void action_play_pause_set_play ();
    void action_play_pause_set_pause ();

    void show_buffering ();

    void add_dock_plugins ();
    void remove_dock_plugins ();

    void title_change_cb ();
    void playback_begin_cb ();
    void playback_ready_cb ();
    void pause_cb ();
    void playback_stop_cb ();
    void update_toggles_cb ();

    void add_dock_plugin_cb (PluginHandle * plugin);
    void remove_dock_plugin_cb (PluginHandle * plugin);

    // unfortunately GCC cannot handle these as an array
    const HookReceiver<MainWindow> hook1, hook2, hook3, hook4, hook5, hook6,
     hook7, hook8, hook9, hook10;
    const HookReceiver<MainWindow, PluginHandle *> plugin_hook1, plugin_hook2;

    Index<DockWidget> dock_widgets;
};

#endif
