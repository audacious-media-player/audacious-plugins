/*
 * menus.cc
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

#include "menus.h"
#include "../ui-common/menu-ops.h"

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
static QMenu * services_menu_pl () { return audqt::menu_get_by_id (AudMenuID::Playlist); }

static void open_files () { audqt::fileopener_show (audqt::FileMode::Open); }
static void add_files () { audqt::fileopener_show (audqt::FileMode::Add); }
static void open_folder () { audqt::fileopener_show (audqt::FileMode::OpenFolder); }
static void add_folder () { audqt::fileopener_show (audqt::FileMode::AddFolder); }
static void open_url () { audqt::urlopener_show (true); }
static void add_url () { audqt::urlopener_show (false); }

static void pl_find () { hook_call ("qtui find", nullptr); }
static void pl_rename () { hook_call ("qtui rename playlist", nullptr); }
static void pl_close () { audqt::playlist_confirm_delete (Playlist::active_playlist ()); }

static void configure_effects () { audqt::prefswin_show_plugin_page (PluginType::Effect); }
static void configure_output () { audqt::prefswin_show_plugin_page (PluginType::Output); }
static void configure_visualizations () { audqt::prefswin_show_plugin_page (PluginType::Vis); }

static void show_search_tool () { hook_call ("qtui show search tool", nullptr); }
static void show_playlist_manager () { hook_call ("qtui show playlist manager", nullptr); }
static void toggle_menubar () { hook_call ("qtui toggle menubar", nullptr); }
static void toggle_infoarea () { hook_call ("qtui toggle infoarea", nullptr); }
static void toggle_infoarea_vis () { hook_call ("qtui toggle infoarea_vis", nullptr); }
static void toggle_statusbar () { hook_call ("qtui toggle statusbar", nullptr); }
static void toggle_remaining_time () { hook_call ("qtui toggle remaining time", nullptr); }

QMenuBar * qtui_build_menubar (QWidget * parent)
{
    static const audqt::MenuItem file_items[] = {
        audqt::MenuCommand ({N_("_Open Files ..."), "document-open", "Ctrl+O"}, open_files),
        audqt::MenuCommand ({N_("_Open Folder ..."), "document-open"}, open_folder),
        audqt::MenuCommand ({N_("Open _URL ..."), "folder-remote", "Ctrl+L"}, open_url),
        audqt::MenuCommand ({N_("_Add Files ..."), "list-add", "Ctrl+Shift+O"}, add_files),
        audqt::MenuCommand ({N_("_Add Folder ..."), "list-add"}, add_folder),
        audqt::MenuCommand ({N_("Add U_RL ..."), "folder-remote", "Ctrl+Shift+L"}, add_url),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("Search _Library"), "edit-find", "Ctrl+Y"}, show_search_tool),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("A_bout ..."), "help-about"}, aud_ui_show_about_window),
        audqt::MenuCommand ({N_("_Settings ..."), "preferences-system"}, aud_ui_show_prefs_window),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Log Inspector ...")}, audqt::log_inspector_show),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Quit"), "application-exit", "Ctrl+Q"}, aud_quit)
    };

    static const audqt::MenuItem playback_items[] = {
        audqt::MenuCommand ({N_("_Play"), "media-playback-start", "Ctrl+Return"}, aud_drct_play),
        audqt::MenuCommand ({N_("Paus_e"), "media-playback-pause", "Ctrl+,"}, aud_drct_pause),
        audqt::MenuCommand ({N_("_Stop"), "media-playback-stop", "Ctrl+."}, aud_drct_stop),
        audqt::MenuCommand ({N_("Pre_vious"), "media-skip-backward", "Alt+Up"}, aud_drct_pl_prev),
        audqt::MenuCommand ({N_("_Next"), "media-skip-forward", "Alt+Down"}, aud_drct_pl_next),
        audqt::MenuSep (),
        audqt::MenuToggle ({N_("_Repeat"), "media-playlist-repeat", "Ctrl+R"}, {nullptr, "repeat", "set repeat"}),
        audqt::MenuToggle ({N_("S_huffle"), "media-playlist-shuffle", "Ctrl+S"}, {nullptr, "shuffle", "set shuffle"}),
        audqt::MenuToggle ({N_("Shuffle by Albu_m")}, {nullptr, "album_shuffle", "set album_shuffle"}),
        audqt::MenuToggle ({N_("N_o Playlist Advance"), nullptr, "Ctrl+N"}, {nullptr, "no_playlist_advance", "set no_playlist_advance"}),
        audqt::MenuToggle ({N_("Stop A_fter This Song"), nullptr, "Ctrl+M"}, {nullptr, "stop_after_current_song", "set stop_after_current_song"}),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("Song _Info ..."), "dialog-information", "Ctrl+I"}, audqt::infowin_show_current)
    };

    static const audqt::MenuItem dupe_items[] = {
        audqt::MenuCommand ({N_("By _Title")}, rm_dupes_title),
        audqt::MenuCommand ({N_("By _File Name")}, rm_dupes_filename),
        audqt::MenuCommand ({N_("By File _Path")}, rm_dupes_path),
    };

    static const audqt::MenuItem sort_items[] = {
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
        audqt::MenuCommand ({N_("By C_omment")}, sort_comment),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("R_everse Order"), "view-sort-descending"}, sort_reverse),
        audqt::MenuCommand ({N_("_Random Order")}, sort_random)
    };

    static const audqt::MenuItem sort_selected_items[] = {
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
        audqt::MenuCommand ({N_("By C_omment")}, sort_sel_comment),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("R_everse Order"), "view-sort-descending"}, sort_sel_reverse),
        audqt::MenuCommand ({N_("_Random Order")}, sort_sel_random)
    };

    static const audqt::MenuItem playlist_items[] = {
        audqt::MenuCommand ({N_("_Play/Resume"), "media-playback-start", "Shift+Return"}, pl_play),
        audqt::MenuCommand ({N_("_Refresh"), "view-refresh", "F5"}, pl_refresh),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Find ..."), "edit-find", "Ctrl+F"}, pl_find),
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
        audqt::MenuCommand ({N_("Playlist _Manager ..."), "audio-x-generic", "Ctrl+P"}, show_playlist_manager),
        audqt::MenuCommand ({N_("_Queue Manager ..."), nullptr, "Ctrl+U"}, audqt::queue_manager_show)
    };

    static const audqt::MenuItem output_items[] = {
        audqt::MenuCommand ({N_("Volume _Up"), "audio-volume-high", "Ctrl++"}, volume_up),
        audqt::MenuCommand ({N_("Volume _Down"), "audio-volume-low", "Ctrl+-"}, volume_down),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Equalizer ..."), "multimedia-volume-control", "Ctrl+E"}, audqt::equalizer_show),
        audqt::MenuCommand ({N_("E_ffects ..."), "preferences-system"}, configure_effects),
        audqt::MenuSep (),
        audqt::MenuToggle ({N_("Record Stream"), "media-record", "Ctrl+D"}, {nullptr, "record", "set record"}),
        audqt::MenuCommand ({N_("Audio Settings ..."), "audio-card"}, configure_output)
    };

    static const audqt::MenuItem view_items[] = {
        audqt::MenuToggle ({N_("Show _Menu Bar"), nullptr, "Shift+Ctrl+M"}, {"qtui", "menu_visible"}, toggle_menubar),
        audqt::MenuToggle ({N_("Show I_nfo Bar"), nullptr, "Shift+Ctrl+I"}, {"qtui", "infoarea_visible"}, toggle_infoarea),
        audqt::MenuToggle ({N_("Show Info Bar Vis_ualization")}, {"qtui", "infoarea_show_vis"}, toggle_infoarea_vis),
        audqt::MenuToggle ({N_("Show _Status Bar"), nullptr, "Shift+Ctrl+S"}, {"qtui", "statusbar_visible"}, toggle_statusbar),
        audqt::MenuSep (),
        audqt::MenuToggle ({N_("Show _Remaining Time"), nullptr, "Shift+Ctrl+R"}, {"qtui", "show_remaining_time", "qtui toggle remaining time"}, toggle_remaining_time),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Visualizations ..."), "preferences-system"}, configure_visualizations)
    };

    static const audqt::MenuItem main_items[] = {
        audqt::MenuSub ({N_("_File")}, file_items),
        audqt::MenuSub ({N_("_Playback")}, playback_items),
        audqt::MenuSub ({N_("P_laylist")}, playlist_items),
        audqt::MenuSub ({N_("_Services")}, services_menu),
        audqt::MenuSub ({N_("_Output")}, output_items),
        audqt::MenuSub ({N_("_View")}, view_items)
    };

    return audqt::menubar_build (main_items, parent);
}

QMenu * qtui_build_pl_menu (QWidget * parent)
{
    static const audqt::MenuItem pl_items[] = {
        audqt::MenuCommand ({N_("Song _Info ..."), "dialog-information", "Alt+I"}, pl_song_info),
        audqt::MenuCommand ({N_("_Queue/Unqueue"), nullptr, "Alt+Q"}, pl_queue_toggle),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("_Open Containing Folder"), "folder"}, pl_open_folder),
        audqt::MenuCommand ({N_("_Refresh Selected"), "view-refresh", "F6"}, pl_refresh_sel),
        audqt::MenuSep (),
        audqt::MenuCommand ({N_("Cu_t"), "edit-cut", "Ctrl+X"}, pl_cut),
        audqt::MenuCommand ({N_("_Copy"), "edit-copy", "Ctrl+C"}, pl_copy),
        audqt::MenuCommand ({N_("_Paste"), "edit-paste", "Ctrl+V"}, pl_paste),
        audqt::MenuCommand ({N_("Paste at _End"), "edit-paste", "Shift+Ctrl+V"}, pl_paste_end),
        audqt::MenuCommand ({N_("Select _All"), "edit-select-all"}, pl_select_all),
        audqt::MenuSep (),
        audqt::MenuSub ({N_("_Services")}, services_menu_pl)
    };

    return audqt::menu_build (pl_items, parent);
}
