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

#include "main_window.h"

#include <libaudcore/drct.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>

#include <libaudqt/libaudqt.h>

void MainWindow::setupActions ()
{
    connect (actionAbout, &QAction::triggered, aud_ui_show_about_window);
    connect (actionPreferences, &QAction::triggered, aud_ui_show_prefs_window);
    connect (actionQuit, &QAction::triggered, aud_quit);

    connect (actionRepeat, &QAction::toggled, [] (bool checked)
        { aud_set_bool (nullptr, "repeat", checked); });
    connect (actionShuffle, &QAction::triggered, [] (bool checked)
        { aud_set_bool (nullptr, "shuffle", checked); });
    connect (actionNoPlaylistAdvance, &QAction::triggered, [] (bool checked)
        { aud_set_bool (nullptr, "no_playlist_advance", checked); });
    connect (actionStopAfterThisSong, &QAction::triggered, [] (bool checked)
        { aud_set_bool (nullptr, "stop_after_current_song", checked); });

    connect (actionOpenFiles, &QAction::triggered, audqt::fileopener_show);
    connect (actionAddFiles,  &QAction::triggered, [] ()
        { audqt::fileopener_show (true); });

    connect (actionPlay,      &QAction::triggered, aud_drct_play);
    connect (actionPause,     &QAction::triggered, aud_drct_pause);
    connect (actionStop,      &QAction::triggered, aud_drct_stop);
    connect (actionPrevious,  &QAction::triggered, aud_drct_pl_prev);
    connect (actionNext,      &QAction::triggered, aud_drct_pl_next);

    connect (actionEqualizer, &QAction::triggered, audqt::equalizer_show);

    connect(actionPlaylistNew, &QAction::triggered, [] () {
        aud_playlist_insert (-1);
        aud_playlist_set_active (aud_playlist_count () - 1);
    });

    connect(actionPlaylistRemove, &QAction::triggered, [] () {
        audqt::playlist_confirm_delete (aud_playlist_get_active ());
    });

    connect(actionPlaylistReverse, &QAction::triggered, [] () { aud_playlist_reverse (aud_playlist_get_active ()); });
    connect(actionPlaylistRandomize, &QAction::triggered, [] () { aud_playlist_randomize (aud_playlist_get_active ()); });

    connect(actionSortTrackNumber, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TRACK); });
    connect(actionSortTitle, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); });
    connect(actionSortArtist, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ARTIST); });
    connect(actionSortAlbum, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ALBUM); });
    connect(actionSortDate, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_DATE); });
    connect(actionSortLength, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_LENGTH); });
    connect(actionSortPath, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); });
    connect(actionSortCustomTitle, &QAction::triggered, [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FORMATTED_TITLE); });

    connect(actionSortSelectedTrackNumber, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TRACK); });
    connect(actionSortSelectedTitle, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); });
    connect(actionSortSelectedArtist, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ARTIST); });
    connect(actionSortSelectedAlbum, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ALBUM); });
    connect(actionSortSelectedDate, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_DATE); });
    connect(actionSortSelectedLength, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_LENGTH); });
    connect(actionSortSelectedPath, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); });
    connect(actionSortSelectedCustomTitle, &QAction::triggered, [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FORMATTED_TITLE); });

    connect(actionSelectedReverse, &QAction::triggered, [] () { aud_playlist_reverse_selected (aud_playlist_get_active ()); });
    connect(actionSelectedRandomize, &QAction::triggered, [] () { aud_playlist_randomize_selected (aud_playlist_get_active ()); });

    /* plugin menus */
    QMenuBar * mb = this->menuBar();
    mb->addAction (audqt::menu_get_by_id (AUD_MENU_MAIN)->menuAction ());

    connect(actionEffects, &QAction::triggered, [] () { audqt::prefswin_show_plugin_page (PLUGIN_TYPE_EFFECT); });
}
