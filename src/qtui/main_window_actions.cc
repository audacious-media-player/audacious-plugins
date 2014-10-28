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
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/drct.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/menu.h>

#define DUMMY [] () { AUDDBG ("implement me\n"); }

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

    const audqt::MenuItem dupe_items[] = {
        audqt::MenuCommand (N_("By _Title"), [] () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Title); }),
        audqt::MenuCommand (N_("By _Filename"), [] () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Filename); }),
        audqt::MenuCommand (N_("By File _Path"), [] () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Path); }),
    };

    const audqt::MenuItem sort_items[] = {
        audqt::MenuCommand (N_("By Track _Number"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Track); }),
        audqt::MenuCommand (N_("By _Title"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Title); }),
        audqt::MenuCommand (N_("By _Artist"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Artist); }),
        audqt::MenuCommand (N_("By Al_bum"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Album); }),
        audqt::MenuCommand (N_("By Albu_m Artist"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); }),
        audqt::MenuCommand (N_("By Release _Date"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Date); }),
        audqt::MenuCommand (N_("By _Length"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Length); }),
        audqt::MenuCommand (N_("By _File Path"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Path); }),
        audqt::MenuCommand (N_("By _Custom Title"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); }),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("R_everse Order"), [] () { aud_playlist_reverse (aud_playlist_get_active ()); }, nullptr, "view-sort-descending"),
        audqt::MenuCommand (N_("_Random Order"), [] () { aud_playlist_randomize (aud_playlist_get_active ()); })
    };

    const audqt::MenuItem sort_selected_items[] = {
        audqt::MenuCommand (N_("By Track _Number"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Track); }),
        audqt::MenuCommand (N_("By _Title"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Title); }),
        audqt::MenuCommand (N_("By _Artist"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Artist); }),
        audqt::MenuCommand (N_("By Al_bum"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Album); }),
        audqt::MenuCommand (N_("By Albu_m Artist"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); }),
        audqt::MenuCommand (N_("By Release _Date"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Date); }),
        audqt::MenuCommand (N_("By _Length"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Length); }),
        audqt::MenuCommand (N_("By _File Path"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Path); }),
        audqt::MenuCommand (N_("By _Custom Title"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); }),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("R_everse Order"), [] () { aud_playlist_reverse_selected (aud_playlist_get_active ()); }, nullptr, "view-sort-descending"),
        audqt::MenuCommand (N_("_Random Order"), [] () { aud_playlist_randomize_selected (aud_playlist_get_active ()); })
    };

    const audqt::MenuItem playlist_items[] = {
        audqt::MenuCommand (N_("_Play This Playlist"), [] () { aud_playlist_play (aud_playlist_get_active ()); }, "Shift+Enter", "media-playback-start"),
        audqt::MenuCommand (N_("_Refresh"), [] () { aud_playlist_rescan (aud_playlist_get_active ()); }, "F5", "view-refresh"),
        audqt::MenuSep (),
        audqt::MenuSub (N_("_Sort"), sort_items, "view-sort-ascending"),
        audqt::MenuSub (N_("Sort Se_lected"), sort_selected_items, "view-sort-ascending"),
        audqt::MenuSub (N_("Remove _Duplicates"), dupe_items, "edit-copy"),
        audqt::MenuCommand (N_("Remove _Unavailable Files"), [] () { aud_playlist_remove_failed (aud_playlist_get_active ()); }, nullptr, "dialog-warning"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_New"), [] () { aud_playlist_insert (-1); aud_playlist_set_active (aud_playlist_count () - 1); }, "Ctrl+T", "document-new"),
        audqt::MenuCommand (N_("Ren_ame ..."), DUMMY, "F2", "insert-text"),
        audqt::MenuCommand (N_("Remo_ve"), [] () { audqt::playlist_confirm_delete (aud_playlist_get_active ()); }, "Ctrl+W", "edit-delete"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Import ..."), DUMMY, nullptr, "document-open"),
        audqt::MenuCommand (N_("_Export ..."), DUMMY, nullptr, "document-save"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("Playlist _Manager ..."), DUMMY, "Ctrl+P", "audio-x-generic"),
        audqt::MenuCommand (N_("_Queue Manager ..."), audqt::queue_manager_show, "Ctrl+U")
    };

    const audqt::MenuItem output_items[] = {
        audqt::MenuCommand (N_("Volume _Up"), [] () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }, "Ctrl++", "audio-volume-high"),
        audqt::MenuCommand (N_("Volume _Down"), [] () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }, "Ctrl+-", "audio-volume-low"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("_Equalizer"), audqt::equalizer_show, "Ctrl+E", "multimedia-volume-control"),
        audqt::MenuSep (),
        audqt::MenuCommand (N_("E_ffects ..."), [] () { audqt::prefswin_show_plugin_page (PLUGIN_TYPE_EFFECT); })
    };

    const audqt::MenuItem main_items[] = {
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
