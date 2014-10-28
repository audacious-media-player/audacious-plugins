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

static QMenu * services_menu () { return audqt::menu_get_by_id (AUD_MENU_MAIN); }

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
        audqt::MenuCommand (N_("_Quit"), aud_quit, "Ctrl+Q", "application-exit")
    };

    const audqt::MenuItem playback_items[] = {
        audqt::MenuCommand (N_("_Play"), aud_drct_play, "Ctrl+Enter", "media-playback-start"),
        audqt::MenuCommand (N_("Paus_e"), aud_drct_pause, "Ctrl+,", "media-playback-pause"),
        audqt::MenuCommand (N_("_Stop"), aud_drct_stop, "Ctrl+.", "media-playback-stop"),
        audqt::MenuCommand (N_("Pre_vious"), aud_drct_pl_prev, "Alt+Up", "media-skip-backward"),
        audqt::MenuCommand (N_("_Next"), aud_drct_pl_next, "Alt+Down", "media-skip-forward"),
        audqt::MenuSep (),
        audqt::MenuToggle (N_("_Repeat"), nullptr, "Ctrl+R", nullptr, nullptr, "repeat", nullptr, "set repeat"),
        audqt::MenuToggle (N_("S_huffle"), nullptr, "Ctrl+S", nullptr, nullptr, "shuffle", nullptr, "set shuffle"),
        audqt::MenuToggle (N_("N_o Playlist Advance"), nullptr, "Ctrl+N", nullptr, nullptr, "no_playlist_advance", nullptr, "set no_playlist_advance"),
        audqt::MenuToggle (N_("Stop A_fter This Song"), nullptr, "Ctrl+M", nullptr, nullptr, "stop_after_current_song", nullptr, "set stop_after_current_song"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("Song _Info ..."), audqt::infowin_show_current, "Ctrl+I", "dialog-information"),
    };

    const audqt::MenuItem main_items[] = {
        audqt::MenuSub (N_("_File"), file_items),
        audqt::MenuSub (N_("_Playback"), playback_items),
        audqt::MenuSub (N_("_Services"), services_menu),
    };

    QMenuBar * mb = new QMenuBar (this);
    audqt::menubar_build (main_items, mb);

    setMenuBar (mb);

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
    connect(ui->actionEffects, &QAction::triggered, [] () { audqt::prefswin_show_plugin_page (PLUGIN_TYPE_EFFECT); });
}
