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
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/drct.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/menu.h>

static void DUMMY () { AUDDBG ("implement me\n"); }

static QMenu * services_menu () { return audqt::menu_get_by_id (AUD_MENU_MAIN); }

static void open_files () { audqt::fileopener_show (false); }
static void add_files () { audqt::fileopener_show (true); }

static void rm_dupes_title () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Title); }
static void rm_dupes_filename () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Filename); }
static void rm_dupes_path () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Path); }

static void sort_track () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Track); }
static void sort_title () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Title); }
static void sort_artist () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Artist); }
static void sort_album () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Album); }
static void sort_album_artist () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); }
static void sort_date () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Date); }
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
static void pl_close () { audqt::playlist_confirm_delete (aud_playlist_get_active ()); }

static void volume_up () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }
static void volume_down () { aud_drct_set_volume_main (aud_drct_get_volume_main () - 5); }

static void configure_effects () { audqt::prefswin_show_plugin_page (PLUGIN_TYPE_EFFECT); }

void MainWindow::setupActions ()
{
    static constexpr audqt::MenuItem file_items[] = {
        audqt::MenuCommand (N_("_Open Files ..."), open_files, "Ctrl+O", "document-open"),
        audqt::MenuCommand (N_("_Add Files ..."), add_files, "Ctrl+Shift+O", "list-add"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("A_bout ..."), aud_ui_show_about_window, nullptr, "help-about"),
        audqt::MenuCommand (N_("_Settings ..."), aud_ui_show_prefs_window, nullptr, "preferences-system"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Log Inspector ..."), audqt::log_inspector_show),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Quit"), aud_quit, "Ctrl+Q", "application-exit")
    };

    static constexpr audqt::MenuItem playback_items[] = {
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

    static constexpr audqt::MenuItem dupe_items[] = {
        audqt::MenuCommand (N_("By _Title"), rm_dupes_title),
        audqt::MenuCommand (N_("By _Filename"), rm_dupes_filename),
        audqt::MenuCommand (N_("By File _Path"), rm_dupes_path),
    };

    static constexpr audqt::MenuItem sort_items[] = {
        audqt::MenuCommand (N_("By Track _Number"), sort_track),
        audqt::MenuCommand (N_("By _Title"), sort_title),
        audqt::MenuCommand (N_("By _Artist"), sort_artist),
        audqt::MenuCommand (N_("By Al_bum"), sort_album),
        audqt::MenuCommand (N_("By Albu_m Artist"), sort_album_artist),
        audqt::MenuCommand (N_("By Release _Date"), sort_date),
        audqt::MenuCommand (N_("By _Length"), sort_length),
        audqt::MenuCommand (N_("By _File Path"), sort_path),
        audqt::MenuCommand (N_("By _Custom Title"), sort_custom_title),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("R_everse Order"), sort_reverse, nullptr, "view-sort-descending"),
        audqt::MenuCommand (N_("_Random Order"), sort_random)
    };

    static constexpr audqt::MenuItem sort_selected_items[] = {
        audqt::MenuCommand (N_("By Track _Number"), sort_sel_track),
        audqt::MenuCommand (N_("By _Title"), sort_sel_title),
        audqt::MenuCommand (N_("By _Artist"), sort_sel_artist),
        audqt::MenuCommand (N_("By Al_bum"), sort_sel_album),
        audqt::MenuCommand (N_("By Albu_m Artist"), sort_sel_album_artist),
        audqt::MenuCommand (N_("By Release _Date"), sort_sel_date),
        audqt::MenuCommand (N_("By _Length"), sort_sel_length),
        audqt::MenuCommand (N_("By _File Path"), sort_sel_path),
        audqt::MenuCommand (N_("By _Custom Title"), sort_sel_custom_title),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("R_everse Order"), sort_sel_reverse, nullptr, "view-sort-descending"),
        audqt::MenuCommand (N_("_Random Order"), sort_sel_random)
    };

    static constexpr audqt::MenuItem playlist_items[] = {
        audqt::MenuCommand (N_("_Play This Playlist"), pl_play, "Shift+Enter", "media-playback-start"),
        audqt::MenuCommand (N_("_Refresh"), pl_refresh, "F5", "view-refresh"),
        audqt::MenuSep (),
        audqt::MenuSub (N_("_Sort"), sort_items, "view-sort-ascending"),
        audqt::MenuSub (N_("Sort Se_lected"), sort_selected_items, "view-sort-ascending"),
        audqt::MenuSub (N_("Remove _Duplicates"), dupe_items, "edit-copy"),
        audqt::MenuCommand (N_("Remove _Unavailable Files"), pl_remove_failed, nullptr, "dialog-warning"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_New"), pl_new, "Ctrl+T", "document-new"),
        audqt::MenuCommand (N_("Ren_ame ..."), DUMMY, "F2", "insert-text"),
        audqt::MenuCommand (N_("Remo_ve"), pl_close, "Ctrl+W", "edit-delete"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Import ..."), DUMMY, nullptr, "document-open"),
        audqt::MenuCommand (N_("_Export ..."), DUMMY, nullptr, "document-save"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("Playlist _Manager ..."), DUMMY, "Ctrl+P", "audio-x-generic"),
        audqt::MenuCommand (N_("_Queue Manager ..."), audqt::queue_manager_show, "Ctrl+U")
    };

    static constexpr audqt::MenuItem output_items[] = {
        audqt::MenuCommand (N_("Volume _Up"), volume_up, "Ctrl++", "audio-volume-high"),
        audqt::MenuCommand (N_("Volume _Down"), volume_down, "Ctrl+-", "audio-volume-low"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Equalizer"), audqt::equalizer_show, "Ctrl+E", "multimedia-volume-control"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("E_ffects ..."), configure_effects)
    };

    static constexpr audqt::MenuItem main_items[] = {
        audqt::MenuSub (N_("_File"), file_items),
        audqt::MenuSub (N_("_Playback"), playback_items),
        audqt::MenuSub (N_("P_laylist"), playlist_items),
        audqt::MenuSub (N_("_Services"), services_menu),
        audqt::MenuSub (N_("_Output"), output_items),
    };

    QMenuBar * mb = new QMenuBar (this);
    audqt::menubar_build (main_items, mb);
    setMenuBar (mb);
}
