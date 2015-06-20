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

#include <QMenuBar>

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugins.h>
#include <libaudcore/drct.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/menu.h>

static QMenu * services_menu () { return audqt::menu_get_by_id (AudMenuID::Main); }

static void open_files () { audqt::fileopener_show (audqt::FileMode::Open); }
static void add_files () { audqt::fileopener_show (audqt::FileMode::Add); }
static void open_folder () { audqt::fileopener_show (audqt::FileMode::OpenFolder); }
static void add_folder () { audqt::fileopener_show (audqt::FileMode::AddFolder); }

static void rm_dupes_title () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Title); }
static void rm_dupes_filename () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Filename); }
static void rm_dupes_path () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Path); }

static void sort_track () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Track); }
static void sort_title () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Title); }
static void sort_artist () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Artist); }
static void sort_album () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Album); }
static void sort_album_artist () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); }
static void sort_date () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Date); }
static void sort_genre () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Genre); }
static void sort_length () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Length); }
static void sort_path () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Path); }
static void sort_custom_title () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); }
static void sort_reverse () { aud_playlist_reverse (aud_playlist_get_active ()); }
static void sort_random () { aud_playlist_randomize (aud_playlist_get_active ()); }

static void sort_sel_track () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Track); }
static void sort_sel_title () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Title); }
static void sort_sel_artist () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Artist); }
static void sort_sel_album () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Album); }
static void sort_sel_album_artist () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); }
static void sort_sel_date () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Date); }
static void sort_sel_genre () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Genre); }
static void sort_sel_length () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Length); }
static void sort_sel_path () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Path); }
static void sort_sel_custom_title () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); }
static void sort_sel_reverse () { aud_playlist_reverse (aud_playlist_get_active ()); }
static void sort_sel_random () { aud_playlist_randomize (aud_playlist_get_active ()); }

static void pl_new ()
{
    aud_playlist_insert (-1);
    aud_playlist_set_active (aud_playlist_count () - 1);
}

static void pl_play () { aud_playlist_play (aud_playlist_get_active ()); }
static void pl_refresh () { aud_playlist_rescan (aud_playlist_get_active ()); }
static void pl_remove_failed () { aud_playlist_remove_failed (aud_playlist_get_active ()); }
static void pl_rename () { hook_call ("qtui rename playlist", nullptr); }
static void pl_close () { audqt::playlist_confirm_delete (aud_playlist_get_active ()); }

static void volume_up () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }
static void volume_down () { aud_drct_set_volume_main (aud_drct_get_volume_main () - 5); }

static void configure_effects () { audqt::prefswin_show_plugin_page (PluginType::Effect); }

void MainWindow::setupActions ()
{
    static constexpr audqt::MenuItem file_items[] = {
        audqt::MenuCommand ({N_("_Open Files ..."), "document-open", "Ctrl+O"}, open_files),
        audqt::MenuCommand ({N_("_Open Folder ..."), "document-open"}, open_folder),
        audqt::MenuCommand ({N_("_Add Files ..."), "list-add", "Ctrl+Shift+O"}, add_files),
        audqt::MenuCommand ({N_("_Add Folder ..."), "list-add"}, add_folder),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("A_bout ..."), "help-about"}, aud_ui_show_about_window),
        audqt::MenuCommand ({N_("_Settings ..."), "preferences-system"}, aud_ui_show_prefs_window),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Log Inspector ...")}, audqt::log_inspector_show),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Quit"), "application-exit", "Ctrl+Q"}, aud_quit)
    };

    static constexpr audqt::MenuItem playback_items[] = {
        audqt::MenuCommand ({N_("_Play"), "media-playback-start", "Ctrl+Return"}, aud_drct_play),
        audqt::MenuCommand ({N_("Paus_e"), "media-playback-pause", "Ctrl+,"}, aud_drct_pause),
        audqt::MenuCommand ({N_("_Stop"), "media-playback-stop", "Ctrl+."}, aud_drct_stop),
        audqt::MenuCommand ({N_("Pre_vious"), "media-skip-backward", "Alt+Up"}, aud_drct_pl_prev),
        audqt::MenuCommand ({N_("_Next"), "media-skip-forward", "Alt+Down"}, aud_drct_pl_next),
        audqt::MenuSep (),
        audqt::MenuToggle ({N_("_Repeat"), nullptr, "Ctrl+R"}, {nullptr, "repeat", "set repeat"}),
        audqt::MenuToggle ({N_("S_huffle"), nullptr, "Ctrl+S"}, {nullptr, "shuffle", "set shuffle"}),
        audqt::MenuToggle ({N_("N_o Playlist Advance"), nullptr, "Ctrl+N"}, {nullptr, "no_playlist_advance", "set no_playlist_advance"}),
        audqt::MenuToggle ({N_("Stop A_fter This Song"), nullptr, "Ctrl+M"}, {nullptr, "stop_after_current_song", "set stop_after_current_song"}),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("Song _Info ..."), "dialog-information", "Ctrl+I"}, audqt::infowin_show_current)
    };

    static constexpr audqt::MenuItem dupe_items[] = {
        audqt::MenuCommand ({N_("By _Title")}, rm_dupes_title),
        audqt::MenuCommand ({N_("By _File Name")}, rm_dupes_filename),
        audqt::MenuCommand ({N_("By File _Path")}, rm_dupes_path),
    };

    static constexpr audqt::MenuItem sort_items[] = {
        audqt::MenuCommand ({N_("By Track _Number")}, sort_track),
        audqt::MenuCommand ({N_("By _Title")}, sort_title),
        audqt::MenuCommand ({N_("By _Artist")}, sort_artist),
        audqt::MenuCommand ({N_("By Al_bum")}, sort_album),
        audqt::MenuCommand ({N_("By Albu_m Artist")}, sort_album_artist),
        audqt::MenuCommand ({N_("By Release _Date")}, sort_date),
        audqt::MenuCommand ({N_("By _Genre")}, sort_genre),
        audqt::MenuCommand ({N_("By _Length")}, sort_length),
        audqt::MenuCommand ({N_("By _File Path")}, sort_path),
        audqt::MenuCommand ({N_("By _Custom Title")}, sort_custom_title),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("R_everse Order"), "view-sort-descending"}, sort_reverse),
        audqt::MenuCommand ({N_("_Random Order")}, sort_random)
    };

    static constexpr audqt::MenuItem sort_selected_items[] = {
        audqt::MenuCommand ({N_("By Track _Number")}, sort_sel_track),
        audqt::MenuCommand ({N_("By _Title")}, sort_sel_title),
        audqt::MenuCommand ({N_("By _Artist")}, sort_sel_artist),
        audqt::MenuCommand ({N_("By Al_bum")}, sort_sel_album),
        audqt::MenuCommand ({N_("By Albu_m Artist")}, sort_sel_album_artist),
        audqt::MenuCommand ({N_("By Release _Date")}, sort_sel_date),
        audqt::MenuCommand ({N_("By _Genre")}, sort_sel_genre),
        audqt::MenuCommand ({N_("By _Length")}, sort_sel_length),
        audqt::MenuCommand ({N_("By _File Path")}, sort_sel_path),
        audqt::MenuCommand ({N_("By _Custom Title")}, sort_sel_custom_title),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("R_everse Order"), "view-sort-descending"}, sort_sel_reverse),
        audqt::MenuCommand ({N_("_Random Order")}, sort_sel_random)
    };

    static constexpr audqt::MenuItem playlist_items[] = {
        audqt::MenuCommand ({N_("_Play/Resume"), "media-playback-start", "Shift+Return"}, pl_play),
        audqt::MenuCommand ({N_("_Refresh"), "view-refresh", "F5"}, pl_refresh),
        audqt::MenuSep (),
        audqt::MenuSub ({N_("_Sort"), "view-sort-ascending"}, sort_items),
        audqt::MenuSub ({N_("Sort Se_lected"), "view-sort-ascending"}, sort_selected_items),
        audqt::MenuSub ({N_("Remove _Duplicates"), "edit-copy"}, dupe_items),
        audqt::MenuCommand ({N_("Remove _Unavailable Files"), "dialog-warning"}, pl_remove_failed),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_New"), "document-new", "Ctrl+T"}, pl_new),
        audqt::MenuCommand ({N_("Ren_ame ..."), "insert-text", "F2"}, pl_rename),
        audqt::MenuCommand ({N_("Remo_ve"), "edit-delete", "Ctrl+W"}, pl_close),
        audqt::MenuSep (),
        //audqt::MenuCommand ({N_("_Import ..."), "document-open"}, TODO),
        //audqt::MenuCommand ({N_("_Export ..."), "document-save"}, TODO),
        audqt::MenuSep (),
        //audqt::MenuCommand ({N_("Playlist _Manager ..."), "audio-x-generic", "Ctrl+P"}, TODO),
        audqt::MenuCommand ({N_("_Queue Manager ..."), nullptr, "Ctrl+U"}, audqt::queue_manager_show)
    };

    static constexpr audqt::MenuItem output_items[] = {
        audqt::MenuCommand ({N_("Volume _Up"), "audio-volume-high", "Ctrl++"}, volume_up),
        audqt::MenuCommand ({N_("Volume _Down"), "audio-volume-low", "Ctrl+-"}, volume_down),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Equalizer"), "multimedia-volume-control", "Ctrl+E"}, audqt::equalizer_show),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("E_ffects ...")}, configure_effects)
    };

    static constexpr audqt::MenuItem main_items[] = {
        audqt::MenuSub ({N_("_File")}, file_items),
        audqt::MenuSub ({N_("_Playback")}, playback_items),
        audqt::MenuSub ({N_("P_laylist")}, playlist_items),
        audqt::MenuSub ({N_("_Services")}, services_menu),
        audqt::MenuSub ({N_("_Output")}, output_items),
    };

    setMenuBar (audqt::menubar_build (main_items, this));
}
