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
#include <libaudcore/mainloop.h>
#include <libaudcore/playlist.h>

#include "../ui-common/dialogs-qt.h"

#include <QMainWindow>

class InfoBar;
class PlaylistTabs;
class PluginHandle;
class PluginWidget;
class QVBoxLayout;
class StatusBar;

class MainWindow : public QMainWindow
{
public:
    MainWindow ();
    ~MainWindow ();

private:
    QString m_config_name;
    DialogWindows m_dialogs;
    QMenuBar * m_menubar;
    PlaylistTabs * m_playlist_tabs;
    QWidget * m_center_widget;
    QVBoxLayout * m_center_layout;
    InfoBar * m_infobar;
    StatusBar * m_statusbar;

    PluginHandle * m_search_tool, * m_playlist_manager;
    Index<PluginWidget *> m_dock_widgets;

    QAction * m_search_action;
    QAction * m_play_pause_action, * m_stop_action, * m_stop_after_action;
    QAction * m_record_action;
    QAction * m_repeat_action, * m_shuffle_action;

    QueuedFunc m_buffering_timer;
    Playlist m_last_playing;

    void closeEvent (QCloseEvent * e);
    void keyPressEvent (QKeyEvent * event);

    void read_settings ();
    void set_title (const QString & title);
    void update_toggles ();
    void update_visibility ();
    void update_play_pause ();

    void title_change_cb ();
    void playback_begin_cb ();
    void buffering_cb ();
    void playback_ready_cb ();
    void pause_cb ();
    void playback_stop_cb ();

    PluginWidget * find_dock_plugin (PluginHandle * plugin);
    void show_dock_plugin (PluginHandle * plugin);
    void add_dock_plugin_cb (PluginHandle * plugin);
    void remove_dock_plugin_cb (PluginHandle * plugin);
    void add_dock_plugins ();
    void remove_dock_plugins ();

    void show_search_tool ()
        { if (m_search_tool) show_dock_plugin (m_search_tool); }
    void show_playlist_manager ()
        { if (m_playlist_manager) show_dock_plugin (m_playlist_manager); }

    static bool plugin_watcher (PluginHandle *, void * me)
        { ((MainWindow *) me)->update_toggles (); return true; }

    const HookReceiver<MainWindow>
     hook1 {"title change", this, & MainWindow::title_change_cb},
     hook2 {"playback begin", this, & MainWindow::playback_begin_cb},
     hook3 {"playback ready", this, & MainWindow::title_change_cb},
     hook4 {"playback pause", this, & MainWindow::pause_cb},
     hook5 {"playback unpause", this, & MainWindow::pause_cb},
     hook6 {"playback stop", this, & MainWindow::playback_stop_cb},
     hook7 {"set stop_after_current_song", this, & MainWindow::update_toggles},
     hook8 {"enable record", this, & MainWindow::update_toggles},
     hook9 {"set record", this, & MainWindow::update_toggles},
     hook10 {"set repeat", this, & MainWindow::update_toggles},
     hook11 {"set shuffle", this, & MainWindow::update_toggles},
     hook12 {"qtui toggle menubar", this, & MainWindow::update_visibility},
     hook13 {"qtui toggle infoarea", this, & MainWindow::update_visibility},
     hook14 {"qtui toggle statusbar", this, & MainWindow::update_visibility},
     hook15 {"qtui show search tool", this, & MainWindow::show_search_tool},
     hook16 {"qtui show playlist manager", this, & MainWindow::show_playlist_manager};

    const HookReceiver<MainWindow, PluginHandle *>
     plugin_hook1 {"dock plugin enabled", this, & MainWindow::add_dock_plugin_cb},
     plugin_hook2 {"dock plugin disabled", this, & MainWindow::remove_dock_plugin_cb};
};

#endif
