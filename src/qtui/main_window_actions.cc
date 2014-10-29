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
        audqt::MenuCommand (_("_Open Files ..."), [] () { audqt::fileopener_show (false); }, "Ctrl+O", "document-open"),
        audqt::MenuCommand (_("_Add Files ..."), [] () { audqt::fileopener_show (true); }, "Ctrl+Shift+O", "list-add"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("A_bout ..."), aud_ui_show_about_window, nullptr, "help-about"),
        audqt::MenuCommand (_("_Settings ..."), aud_ui_show_prefs_window, nullptr, "preferences-system"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("_Log Inspector ..."), audqt::log_inspector_show),
        audqt::MenuSep (),
        audqt::MenuCommand (_("_Quit"), aud_quit, "Ctrl+Q", "application-exit")
    };

    const audqt::MenuItem playback_items[] = {
        audqt::MenuCommand (_("_Play"), aud_drct_play, "Ctrl+Enter", "media-playback-start"),
        audqt::MenuCommand (_("Paus_e"), aud_drct_pause, "Ctrl+,", "media-playback-pause"),
        audqt::MenuCommand (_("_Stop"), aud_drct_stop, "Ctrl+.", "media-playback-stop"),
        audqt::MenuCommand (_("Pre_vious"), aud_drct_pl_prev, "Alt+Up", "media-skip-backward"),
        audqt::MenuCommand (_("_Next"), aud_drct_pl_next, "Alt+Down", "media-skip-forward"),
        audqt::MenuSep (),
        audqt::MenuToggle (_("_Repeat"), nullptr, "Ctrl+R", nullptr, nullptr, "repeat", nullptr, "set repeat"),
        audqt::MenuToggle (_("S_huffle"), nullptr, "Ctrl+S", nullptr, nullptr, "shuffle", nullptr, "set shuffle"),
        audqt::MenuToggle (_("N_o Playlist Advance"), nullptr, "Ctrl+N", nullptr, nullptr, "no_playlist_advance", nullptr, "set no_playlist_advance"),
        audqt::MenuToggle (_("Stop A_fter This Song"), nullptr, "Ctrl+M", nullptr, nullptr, "stop_after_current_song", nullptr, "set stop_after_current_song"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("Song _Info ..."), audqt::infowin_show_current, "Ctrl+I", "dialog-information"),
    };

    const audqt::MenuItem dupe_items[] = {
        audqt::MenuCommand (_("By _Title"), [] () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Title); }),
        audqt::MenuCommand (_("By _Filename"), [] () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Filename); }),
        audqt::MenuCommand (_("By File _Path"), [] () { aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), Playlist::Path); }),
    };

    const audqt::MenuItem sort_items[] = {
        audqt::MenuCommand (_("By Track _Number"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Track); }),
        audqt::MenuCommand (_("By _Title"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Title); }),
        audqt::MenuCommand (_("By _Artist"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Artist); }),
        audqt::MenuCommand (_("By Al_bum"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Album); }),
        audqt::MenuCommand (_("By Albu_m Artist"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); }),
        audqt::MenuCommand (_("By Release _Date"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Date); }),
        audqt::MenuCommand (_("By _Length"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Length); }),
        audqt::MenuCommand (_("By _File Path"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::Path); }),
        audqt::MenuCommand (_("By _Custom Title"), [] () { aud_playlist_sort_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); }),
        audqt::MenuSep (),
        audqt::MenuCommand (_("R_everse Order"), [] () { aud_playlist_reverse (aud_playlist_get_active ()); }, nullptr, "view-sort-descending"),
        audqt::MenuCommand (_("_Random Order"), [] () { aud_playlist_randomize (aud_playlist_get_active ()); })
    };

    const audqt::MenuItem sort_selected_items[] = {
        audqt::MenuCommand (_("By Track _Number"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Track); }),
        audqt::MenuCommand (_("By _Title"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Title); }),
        audqt::MenuCommand (_("By _Artist"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Artist); }),
        audqt::MenuCommand (_("By Al_bum"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Album); }),
        audqt::MenuCommand (_("By Albu_m Artist"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::AlbumArtist); }),
        audqt::MenuCommand (_("By Release _Date"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Date); }),
        audqt::MenuCommand (_("By _Length"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Length); }),
        audqt::MenuCommand (_("By _File Path"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::Path); }),
        audqt::MenuCommand (_("By _Custom Title"), [] () { aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), Playlist::FormattedTitle); }),
        audqt::MenuSep (),
        audqt::MenuCommand (_("R_everse Order"), [] () { aud_playlist_reverse_selected (aud_playlist_get_active ()); }, nullptr, "view-sort-descending"),
        audqt::MenuCommand (_("_Random Order"), [] () { aud_playlist_randomize_selected (aud_playlist_get_active ()); })
    };

    const audqt::MenuItem playlist_items[] = {
        audqt::MenuCommand (_("_Play This Playlist"), [] () { aud_playlist_play (aud_playlist_get_active ()); }, "Shift+Enter", "media-playback-start"),
        audqt::MenuCommand (_("_Refresh"), [] () { aud_playlist_rescan (aud_playlist_get_active ()); }, "F5", "view-refresh"),
        audqt::MenuSep (),
        audqt::MenuSub (_("_Sort"), sort_items, "view-sort-ascending"),
        audqt::MenuSub (_("Sort Se_lected"), sort_selected_items, "view-sort-ascending"),
        audqt::MenuSub (_("Remove _Duplicates"), dupe_items, "edit-copy"),
        audqt::MenuCommand (_("Remove _Unavailable Files"), [] () { aud_playlist_remove_failed (aud_playlist_get_active ()); }, nullptr, "dialog-warning"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("_New"), [] () { aud_playlist_insert (-1); aud_playlist_set_active (aud_playlist_count () - 1); }, "Ctrl+T", "document-new"),
        audqt::MenuCommand (_("Ren_ame ..."), DUMMY, "F2", "insert-text"),
        audqt::MenuCommand (_("Remo_ve"), [] () { audqt::playlist_confirm_delete (aud_playlist_get_active ()); }, "Ctrl+W", "edit-delete"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("_Import ..."), DUMMY, nullptr, "document-open"),
        audqt::MenuCommand (_("_Export ..."), DUMMY, nullptr, "document-save"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("Playlist _Manager ..."), DUMMY, "Ctrl+P", "audio-x-generic"),
        audqt::MenuCommand (_("_Queue Manager ..."), audqt::queue_manager_show, "Ctrl+U")
    };

    const audqt::MenuItem output_items[] = {
        audqt::MenuCommand (_("Volume _Up"), [] () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }, "Ctrl++", "audio-volume-high"),
        audqt::MenuCommand (_("Volume _Down"), [] () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }, "Ctrl+-", "audio-volume-low"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("_Equalizer"), audqt::equalizer_show, "Ctrl+E", "multimedia-volume-control"),
        audqt::MenuSep (),
        audqt::MenuCommand (_("E_ffects ..."), [] () { audqt::prefswin_show_plugin_page (PLUGIN_TYPE_EFFECT); })
    };

    const audqt::MenuItem main_items[] = {
        audqt::MenuSub (_("_File"), file_items),
        audqt::MenuSub (_("_Playback"), playback_items),
        audqt::MenuSub (_("P_laylist"), playlist_items),
        audqt::MenuSub (_("_Services"), services_menu),
        audqt::MenuSub (_("_Output"), output_items),
    };

    QMenuBar * mb = new QMenuBar (this);
    audqt::menubar_build (main_items, mb);
    setMenuBar (mb);
}
