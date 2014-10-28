/*
 * main_window_actions.cc
 * Copyright 2014 Michał Lipski and William Pitcock
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

#include "ui_main_window.h"
#include "main_window.h"

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/drct.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/menu.h>

void MainWindow::setupActions ()
{
    const audqt::MenuItem file_items[] = {
        audqt::MenuCommand (N_("_Open Files ..."), [] () { audqt::fileopener_show (false); }, "Ctrl+O", "document-open"),
        audqt::MenuCommand (N_("_Add Files ..."), [] () { audqt::fileopener_show (true); }, "Ctrl+Shift+O", "list-add"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("A_bout ..."), aud_ui_show_about_window, nullptr, "help-about"),
        audqt::MenuCommand (N_("_Settings ..."), aud_ui_show_prefs_window, nullptr, "preferences-system"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Log Inspector ..."), audqt::log_inspector_show),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Quit"), aud_quit, "CTRL+Q", "application-exit")
    };

    const audqt::MenuItem main_items[] = {
        audqt::MenuSub (N_("_File"), file_items),
    };

    QMenuBar * mb = new QMenuBar (this);
    audqt::menubar_build (main_items, mb);

    setMenuBar (mb);

    connect (ui->actionAbout, &QAction::triggered, aud_ui_show_about_window);
    connect (ui->actionPreferences, &QAction::triggered, aud_ui_show_prefs_window);
    connect (ui->actionQuit, &QAction::triggered, aud_quit);

    connect (ui->actionRepeat, &QAction::toggled, [] (bool checked)
        { aud_set_bool (nullptr, "repeat", checked); });
    connect (ui->actionShuffle, &QAction::triggered, [] (bool checked)
        { aud_set_bool (nullptr, "shuffle", checked); });
    connect (ui->actionNoPlaylistAdvance, &QAction::triggered, [] (bool checked)
        { aud_set_bool (nullptr, "no_playlist_advance", checked); });
    connect (ui->actionStopAfterThisSong, &QAction::triggered, [] (bool checked)
        { aud_set_bool (nullptr, "stop_after_current_song", checked); });

    connect (ui->actionOpenFiles, &QAction::triggered, audqt::fileopener_show);
    connect (ui->actionAddFiles,  &QAction::triggered, [] ()
        { audqt::fileopener_show (true); });

    connect (ui->actionLogInspector, &QAction::triggered, audqt::log_inspector_show);

    connect (ui->actionPlay,      &QAction::triggered, aud_drct_play);
    connect (ui->actionPause,     &QAction::triggered, aud_drct_pause);
    connect (ui->actionStop,      &QAction::triggered, aud_drct_stop);
    connect (ui->actionPrevious,  &QAction::triggered, aud_drct_pl_prev);
    connect (ui->actionNext,      &QAction::triggered, aud_drct_pl_next);

    connect (ui->actionEqualizer, &QAction::triggered, audqt::equalizer_show);

    connect(ui->actionPlaylistNew, &QAction::triggered, [] () {
        aud_playlist_insert (-1);
        aud_playlist_set_active (aud_playlist_count () - 1);
    });

    connect(ui->actionPlaylistRemove, &QAction::triggered, [] () {
        audqt::playlist_confirm_delete (aud_playlist_get_active ());
    });

    connect(ui->actionPlaylistReverse, &QAction::triggered, [] () { aud_playlist_reverse (aud_playlist_get_active ()); });
    connect(ui->actionPlaylistRandomize, &QAction::triggered, [] () { aud_playlist_randomize (aud_playlist_get_active ()); });

    connect(ui->actionSortTrackNumber, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Track); });
    connect(ui->actionSortTitle, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Title); });
    connect(ui->actionSortArtist, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Artist); });
    connect(ui->actionSortAlbum, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Album); });
    connect(ui->actionSortAlbumArtist, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); });
    connect(ui->actionSortDate, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Date); });
    connect(ui->actionSortLength, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Length); });
    connect(ui->actionSortPath, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Path); });
    connect(ui->actionSortCustomTitle, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); });

    connect(ui->actionSortSelectedTrackNumber, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Track); });
    connect(ui->actionSortSelectedTitle, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Title); });
    connect(ui->actionSortSelectedArtist, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Artist); });
    connect(ui->actionSortSelectedAlbum, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Album); });
    connect(ui->actionSortSelectedAlbumArtist, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); });
    connect(ui->actionSortSelectedDate, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Date); });
    connect(ui->actionSortSelectedLength, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Length); });
    connect(ui->actionSortSelectedPath, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Path); });
    connect(ui->actionSortSelectedCustomTitle, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); });

    connect(ui->actionSelectedReverse, &QAction::triggered, [] () { aud_playlist_reverse_selected (aud_playlist_get_active ()); });
    connect(ui->actionSelectedRandomize, &QAction::triggered, [] () { aud_playlist_randomize_selected (aud_playlist_get_active ()); });

    connect(ui->actionSongInfo, &QAction::triggered, audqt::infowin_show_current);

    connect(ui->actionQueueManager, &QAction::triggered, audqt::queue_manager_show);

    /* plugin menus */
    mb->addAction (audqt::menu_get_by_id (AUD_MENU_MAIN)->menuAction ());

    connect(ui->actionEffects, &QAction::triggered, [] () { audqt::prefswin_show_plugin_page (PLUGIN_TYPE_EFFECT); });
}
