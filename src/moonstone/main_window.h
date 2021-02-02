/*
 * main_window.h
 * Copyright 2014 Michał Lipski
 * Copyright 2020 Ariadne Conill
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
#include <QVBoxLayout>
#include <QSplitter>

namespace Moonstone {

class InfoBar;
class ToolBar;
class PlaylistTabs;
class PlaylistsView;

class MainWindow : public QMainWindow {
public:
    MainWindow();

private:
    QWidget * m_center_widget;
    QVBoxLayout * m_center_layout;
    InfoBar * m_infobar;
    ToolBar * m_toolbar;

    QSplitter * m_splitter;
    PlaylistTabs * m_playlist_tabs;
    PlaylistsView * m_playlists_view;

    QAction *m_search_action;
    QAction *m_play_pause_action, *m_stop_action, *m_stop_after_action;
    QAction *m_record_action;
    QAction *m_repeat_action, *m_shuffle_action;

    QueuedFunc m_buffering_timer;
    Playlist m_last_playing;

    void closeEvent(QCloseEvent * e) override;

    void set_title(const QString & title);

    void update_toggles();
    void update_play_pause();

    void title_change_cb();
    void playback_begin_cb();
    void playback_ready_cb();
    void pause_cb();
    void playback_stop_cb();

    const HookReceiver<MainWindow> //
        hook1{"title change", this, &MainWindow::title_change_cb},
        hook2{"playback begin", this, &MainWindow::playback_begin_cb},
        hook3{"playback ready", this, &MainWindow::title_change_cb},
        hook4{"playback pause", this, &MainWindow::pause_cb},
        hook5{"playback unpause", this, &MainWindow::pause_cb},
        hook6{"playback stop", this, &MainWindow::playback_stop_cb},
        hook7{"set stop_after_current_song", this, &MainWindow::update_toggles},
        hook8{"enable record", this, &MainWindow::update_toggles},
        hook9{"set record", this, &MainWindow::update_toggles},
        hook10{"set repeat", this, &MainWindow::update_toggles},
        hook11{"set shuffle", this, &MainWindow::update_toggles};
};

}

#endif
