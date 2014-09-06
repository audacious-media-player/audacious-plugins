/*
 * main_window_actions.h
 * Copyright 2014 William Pitcock <nenolod@dereferenced.org>
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

#ifndef MAIN_WINDOW_ACTIONS_H
#define MAIN_WINDOW_ACTIONS_H

void MainWindow::setupActions ()
{
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

#endif
