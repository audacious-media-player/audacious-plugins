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

#include "main_window.h"

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

#include "filter_input.h"
#include "playlist.h"
#include "time_slider.h"

MainWindow::MainWindow () :
    m_dialogs (this),
    filterInput (new FilterInput (this))
{
#if defined(HAVE_MSWINDOWS) || defined(HAVE_DARWIN)
    QIcon::setThemeName ("QtUi");

    QStringList paths = QIcon::themeSearchPaths ();
    paths.prepend (aud_get_path (AudPath::DataDir));
    QIcon::setThemeSearchPaths (paths);
#else
    QApplication::setWindowIcon (QIcon::fromTheme ("audacious"));
#endif

    setupUi (this);

    setUnifiedTitleAndToolBarOnMac (true);

    toolBar->addWidget (new audqt::VolumeButton (this));
    toolBar->addWidget (filterInput);

    auto slider = new TimeSlider (this);
    toolBar->insertWidget (actionRepeat, slider);
    toolBar->insertWidget (actionRepeat, slider->label ());

    updateToggles ();

    connect (filterInput, &QLineEdit::textChanged, playlistTabs, &PlaylistTabs::filterTrigger);

    setupActions ();

    hook_associate ("title change",     (HookFunction) title_change_cb, this);
    hook_associate ("playback begin",   (HookFunction) playback_begin_cb, this);
    hook_associate ("playback ready",   (HookFunction) playback_ready_cb, this);
    hook_associate ("playback pause",   (HookFunction) pause_cb, this);
    hook_associate ("playback unpause", (HookFunction) pause_cb, this);
    hook_associate ("playback stop",    (HookFunction) playback_stop_cb, this);

    hook_associate ("set repeat",                  (HookFunction) update_toggles_cb, this);
    hook_associate ("set shuffle",                 (HookFunction) update_toggles_cb, this);
    hook_associate ("set no_playlist_advance",     (HookFunction) update_toggles_cb, this);
    hook_associate ("set stop_after_current_song", (HookFunction) update_toggles_cb, this);

    if (aud_drct_get_playing ())
    {
        playback_begin_cb (nullptr, this);
        if (aud_drct_get_ready ())
            playback_ready_cb (nullptr, this);
    }
    else
        playback_stop_cb (nullptr, this);

    title_change_cb (nullptr, this);
}

MainWindow::~MainWindow ()
{
    hook_dissociate ("title change",     (HookFunction) title_change_cb);
    hook_dissociate ("playback begin",   (HookFunction) playback_begin_cb);
    hook_dissociate ("playback ready",   (HookFunction) playback_ready_cb);
    hook_dissociate ("playback pause",   (HookFunction) pause_cb);
    hook_dissociate ("playback unpause", (HookFunction) pause_cb);
    hook_dissociate ("playback stop",    (HookFunction) playback_stop_cb);

    hook_dissociate ("set repeat",                  (HookFunction) update_toggles_cb);
    hook_dissociate ("set shuffle",                 (HookFunction) update_toggles_cb);
    hook_dissociate ("set no_playlist_advance",     (HookFunction) update_toggles_cb);
    hook_dissociate ("set stop_after_current_song", (HookFunction) update_toggles_cb);
}

void MainWindow::closeEvent (QCloseEvent * e)
{
    aud_quit ();
    e->ignore ();
}

void MainWindow::keyPressEvent (QKeyEvent * e)
{
    switch (e->modifiers ())
    {
    case Qt::ControlModifier:
        switch (e->key ())
        {
        case Qt::Key_F:
            filterInput->setFocus ();
            break;
        }
        break;
    }

    QMainWindow::keyPressEvent (e);
}

void MainWindow::updateToggles ()
{
    actionRepeat->setChecked (aud_get_bool (nullptr, "repeat"));
    actionShuffle->setChecked (aud_get_bool (nullptr, "shuffle"));
    actionNoPlaylistAdvance->setChecked (aud_get_bool (nullptr, "no_playlist_advance"));
    actionStopAfterThisSong->setChecked (aud_get_bool (nullptr, "stop_after_current_song"));
}

void MainWindow::action_play_pause_set_play ()
{
    actionPlayPause->setIcon (QIcon::fromTheme ("media-playback-start"));
    actionPlayPause->setText ("Play");
}

void MainWindow::action_play_pause_set_pause ()
{
    actionPlayPause->setIcon (QIcon::fromTheme ("media-playback-pause"));
    actionPlayPause->setText ("Pause");
}

void MainWindow::title_change_cb (void *, MainWindow * window)
{
    auto title = aud_drct_get_title ();
    if (title)
        window->setWindowTitle (QString ("Audacious - ") + QString (title));
}

void MainWindow::playback_begin_cb (void *, MainWindow * window)
{
    window->setWindowTitle ("Audacious - Buffering...");
    pause_cb (nullptr, window);
}

void MainWindow::playback_ready_cb (void *, MainWindow * window)
{
    title_change_cb (nullptr, window);
    pause_cb (nullptr, window);
}

void MainWindow::pause_cb (void *, MainWindow * window)
{
    if (aud_drct_get_paused ())
        window->action_play_pause_set_play ();
    else
        window->action_play_pause_set_pause ();

    window->playlistTabs->activePlaylistWidget ()->positionUpdate (); /* updates indicator icon */
}

void MainWindow::playback_stop_cb (void *, MainWindow * window)
{
    window->setWindowTitle ("Audacious");
    window->action_play_pause_set_play ();
    window->playlistTabs->activePlaylistWidget ()->positionUpdate (); /* updates indicator icon */
}

void MainWindow::update_toggles_cb (void *, MainWindow * window)
{
    window->updateToggles ();
}
