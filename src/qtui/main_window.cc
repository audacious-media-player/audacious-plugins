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
#include <libaudcore/runtime.h>
#include <libaudcore/interface.h>

#include <libaudqt/libaudqt.h>

#include "filter_input.h"
#include "main_window.h"
#include "playlist.h"
#include "time_slider.h"

MainWindow::MainWindow (QMainWindow * parent) : QMainWindow (parent)
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

    filterInput = new FilterInput (this);
    toolBar->addWidget (filterInput);

    slider = new TimeSlider (this);
    toolBar->insertWidget (actionRepeat, slider);
    toolBar->insertWidget (actionRepeat, slider->label ());

    createStatusBar ();

    updateToggles ();

    connect (actionAbout, &QAction::triggered, aud_ui_show_about_window);
    connect (actionPreferences, &QAction::triggered, aud_ui_show_prefs_window);

    connect (actionQuit, &QAction::triggered, aud_quit);

    connect (actionRepeat, &QAction::toggled, [] (bool checked)
    {
        aud_set_bool (nullptr, "repeat", checked);
    });

    connect (actionShuffle, &QAction::triggered, [] (bool checked)
    {
        aud_set_bool (nullptr, "shuffle", checked);
    });

    connect (actionNoPlaylistAdvance, &QAction::triggered, [] (bool checked)
    {
        aud_set_bool (nullptr, "no_playlist_advance", checked);
    });

    connect (actionStopAfterThisSong, &QAction::triggered, [] (bool checked)
    {
        aud_set_bool (nullptr, "stop_after_current_song", checked);
    });

    connect (actionOpenFiles, &QAction::triggered, audqt::fileopener_show);

    connect (actionAddFiles,  &QAction::triggered, [] ()
    {
        audqt::fileopener_show (true);
    });

    connect (actionPlayPause, &QAction::triggered, aud_drct_play_pause);
    connect (actionStop,      &QAction::triggered, aud_drct_stop);
    connect (actionPrevious,  &QAction::triggered, aud_drct_pl_prev);
    connect (actionNext,      &QAction::triggered, aud_drct_pl_next);

    connect (actionEqualizer, &QAction::triggered, audqt::equalizer_show);

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

    hook_associate ("ui show progress",   (HookFunction) show_progress_cb, this);
    hook_associate ("ui show progress 2", (HookFunction) show_progress_2_cb, this);
    hook_associate ("ui hide progress",   (HookFunction) hide_progress_cb, this);
    hook_associate ("ui show error",      (HookFunction) show_error_cb, this);

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

    hook_dissociate ("ui show progress",   (HookFunction) show_progress_cb);
    hook_dissociate ("ui show progress 2", (HookFunction) show_progress_2_cb);
    hook_dissociate ("ui hide progress",   (HookFunction) hide_progress_cb);
    hook_dissociate ("ui show error",      (HookFunction) show_error_cb);

    hook_dissociate ("set repeat",                  (HookFunction) update_toggles_cb);
    hook_dissociate ("set shuffle",                 (HookFunction) update_toggles_cb);
    hook_dissociate ("set no_playlist_advance",     (HookFunction) update_toggles_cb);
    hook_dissociate ("set stop_after_current_song", (HookFunction) update_toggles_cb);

    hook_dissociate ("playlist activate", (HookFunction) update_playlist_length_cb);
    hook_dissociate ("playlist update",   (HookFunction) update_playlist_length_cb);

    hook_dissociate ("playback ready", (HookFunction) update_codec_info_cb);
    hook_dissociate ("info change",    (HookFunction) update_codec_info_cb);
}

void MainWindow::createProgressDialog ()
{
    if (! progressDialog)
    {
        progressDialog = new QMessageBox (this);
        progressDialog->setIcon (QMessageBox::Information);
        progressDialog->setText ("Working ...");
        progressDialog->setStandardButtons (QMessageBox::NoButton);
        progressDialog->setWindowModality (Qt::WindowModal);
    }
}

void MainWindow::createErrorDialog (const QString &message)
{
    if (! errorDialog)
    {
        errorDialog = new QMessageBox (this);
        errorDialog->setIcon (QMessageBox::Warning);
        errorDialog->setWindowModality (Qt::WindowModal);
    }
    errorDialog->setText (message);
    errorDialog->show ();
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

void MainWindow::createStatusBar ()
{
    QStatusBar * bar = QMainWindow::statusBar();

    playlistLengthLabel = new QLabel ("0:00 / 0:00", this);
    playlistLengthLabel->setAlignment(Qt::AlignRight);

    codecInfoLabel = new QLabel ("", this);

    bar->addPermanentWidget(playlistLengthLabel);
    bar->addWidget(codecInfoLabel);

    hook_associate ("playlist activate", (HookFunction) update_playlist_length_cb, playlistLengthLabel);
    hook_associate ("playlist update", (HookFunction) update_playlist_length_cb, playlistLengthLabel);

    hook_associate ("playback ready", (HookFunction) update_codec_info_cb, codecInfoLabel);
    hook_associate ("info change", (HookFunction) update_codec_info_cb, codecInfoLabel);
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
    window->codecInfoLabel->setText ("");
}

void MainWindow::update_toggles_cb (void *, MainWindow * window)
{
    window->updateToggles ();
}

void MainWindow::show_progress_cb (void * message, MainWindow * window)
{
    window->createProgressDialog ();
    window->progressDialog->setInformativeText ((const char *) message);
    window->progressDialog->show ();
}

void MainWindow::show_progress_2_cb (void * message, MainWindow * window)
{
    window->createProgressDialog ();
    window->progressDialog->setText ((const char *) message);
    window->progressDialog->show ();
}

void MainWindow::hide_progress_cb (void *, MainWindow * window)
{
    if (window->progressDialog)
        window->progressDialog->hide ();
}

void MainWindow::show_error_cb (void * message, MainWindow * window)
{
    window->createErrorDialog (QString ((const char *) message));
}

void MainWindow::update_playlist_length_cb (void *, QLabel * label)
{
    int playlist = aud_playlist_get_active ();

    StringBuf s1 = str_format_time (aud_playlist_get_selected_length (playlist));
    StringBuf s2 = str_format_time (aud_playlist_get_total_length (playlist));

    label->setText (QString (str_concat ({s1, " / ", s2})));
}

#define APPEND(b, ...) snprintf (b + strlen (b), sizeof b - strlen (b), __VA_ARGS__)

void MainWindow::update_codec_info_cb (void *, QLabel * label)
{
    /* may be called asynchronously */
    if (! aud_drct_get_playing ())
        return;

    int playlist = aud_playlist_get_playing ();
    Tuple tuple = aud_playlist_entry_get_tuple (playlist,
     aud_playlist_get_position (playlist), false);
    String codec = tuple.get_str (FIELD_CODEC);

    int bitrate, samplerate, channels;
    aud_drct_get_info (& bitrate, & samplerate, & channels);

    char buf[256];
    buf[0] = 0;

    if (codec)
    {
        APPEND (buf, "%s", (const char *) codec);
        if (channels > 0 || samplerate > 0 || bitrate > 0)
            APPEND (buf, ", ");
    }

    if (channels > 0)
    {
        if (channels == 1)
            APPEND (buf, _("mono"));
        else if (channels == 2)
            APPEND (buf, _("stereo"));
        else
            APPEND (buf, ngettext ("%d channel", "%d channels", channels),
             channels);

        if (samplerate > 0 || bitrate > 0)
            APPEND (buf, ", ");
    }

    if (samplerate > 0)
    {
        APPEND (buf, "%d kHz", samplerate / 1000);
        if (bitrate > 0)
            APPEND (buf, ", ");
    }

    if (bitrate > 0)
        APPEND (buf, _("%d kbps"), bitrate / 1000);

    label->setText (buf);
}
