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
#include <QMessageBox>
#include <QtCore>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>

#include "playlist_tabs.h"
#include "ui_main_window.h"
#include "filter_input.h"

#define APPEND(b, ...) snprintf (b + strlen (b), sizeof b - strlen (b), __VA_ARGS__)

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    MainWindow (QMainWindow * parent = 0);
    ~MainWindow ();

protected:
    void closeEvent (QCloseEvent * e);
    void keyPressEvent (QKeyEvent * e);

public slots:
    void timeCounterSlot ();
    void sliderValueChanged (int value);
    void sliderPressed ();
    void sliderReleased ();

private:
    QTimer timeCounter;

    QLabel * codecInfoLabel = nullptr;
    QLabel * playlistLengthLabel = nullptr;
    QLabel * timeCounterLabel = nullptr;
    QSlider * slider = nullptr;
    FilterInput * filterInput = nullptr;
    QMessageBox * progressDialog = nullptr;
    QMessageBox * errorDialog = nullptr;

    void setTimeCounterLabel (int time, int length);
    void enableSlider ();
    void disableSlider ();
    void enableTimeCounter ();
    void disableTimeCounter ();
    void createProgressDialog ();
    void createErrorDialog (const QString &message);
    void updateToggles ();
    void setupActions ();
    void createStatusBar ();

    static void title_change_cb (void * unused, MainWindow * window)
    {
        auto title = aud_drct_get_title ();
        if (title)
            window->setWindowTitle (QString ("Audacious - ") + QString (title));
    }

    static void playback_begin_cb (void * unused, MainWindow * window)
    {
        window->setWindowTitle ("Audacious - Buffering...");

        pause_cb (nullptr, window);
    }

    static void playback_ready_cb (void * unused, MainWindow * window)
    {
        title_change_cb (nullptr, window);
        pause_cb (nullptr, window);

        window->enableSlider ();
        window->enableTimeCounter ();
    }

    static void action_play_pause_set_play (MainWindow * window)
    {
        window->actionPlayPause->setIcon (QIcon::fromTheme ("media-playback-start"));
        window->actionPlayPause->setText ("Play");
    }

    static void action_play_pause_set_pause (MainWindow * window)
    {
        window->actionPlayPause->setIcon (QIcon::fromTheme ("media-playback-pause"));
        window->actionPlayPause->setText ("Pause");
    }

    static void pause_cb (void * unused, MainWindow * window)
    {
        if (aud_drct_get_paused ())
            action_play_pause_set_play (window);
        else
            action_play_pause_set_pause (window);
        window->playlistTabs->activePlaylistWidget ()->positionUpdate (); /* updates indicator icon */
    }

    static void playback_stop_cb (void * unused, MainWindow * window)
    {
        window->setWindowTitle ("Audacious");
        window->disableTimeCounter ();
        window->disableSlider ();

        action_play_pause_set_play (window);
        window->playlistTabs->activePlaylistWidget ()->positionUpdate (); /* updates indicator icon */

        window->codecInfoLabel->setText ("");
    }

    static void update_toggles_cb (void * unused, MainWindow * window)
    {
        window->updateToggles ();
    }

    static void show_progress_cb (void * message, MainWindow * window)
    {
        window->createProgressDialog ();
        window->progressDialog->setInformativeText ((const char *) message);
        window->progressDialog->show ();
    }

    static void show_progress_2_cb (void * message, MainWindow * window)
    {
        window->createProgressDialog ();
        window->progressDialog->setText ((const char *) message);
        window->progressDialog->show ();
    }

    static void hide_progress_cb (void * unused, MainWindow * window)
    {
        if (window->progressDialog)
            window->progressDialog->hide ();
    }

    static void show_error_cb (void * message, MainWindow * window)
    {
        window->createErrorDialog (QString ((const char *) message));
    }

    static void update_playlist_length_cb (void * unused, QLabel * label)
    {
        int playlist = aud_playlist_get_active ();

        StringBuf s1 = str_format_time (aud_playlist_get_selected_length (playlist));
        StringBuf s2 = str_format_time (aud_playlist_get_total_length (playlist));

        label->setText (QString (str_concat ({s1, " / ", s2})));
    }

    static void update_codec_info_cb (void * unused, QLabel * label)
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
};

#endif
