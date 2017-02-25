/*
 * menus.c
 * Copyright 2010-2014 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include "menus.h"

#include <QMenu>

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>
#include <libaudqt/menu.h>

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "main.h"
#include "view.h"

#include "../ui-common/menu-ops.h"

static QMenu * menus[UI_MENUS];

/* note: playback, playlist, and view menus must be created before main menu */
static QMenu * get_menu_playback () { return menus[UI_MENU_PLAYBACK]; }
static QMenu * get_menu_playlist () { return menus[UI_MENU_PLAYLIST]; }
static QMenu * get_menu_view () { return menus[UI_MENU_VIEW]; }

static QMenu * get_plugin_menu_main () { return audqt::menu_get_by_id (AudMenuID::Main); }
static QMenu * get_plugin_menu_playlist () { return audqt::menu_get_by_id (AudMenuID::Playlist); }
static QMenu * get_plugin_menu_playlist_add () { return audqt::menu_get_by_id (AudMenuID::PlaylistAdd); }
static QMenu * get_plugin_menu_playlist_remove () { return audqt::menu_get_by_id (AudMenuID::PlaylistRemove); }

static void configure_effects () { audqt::prefswin_show_plugin_page (PluginType::Effect); }
static void configure_output () { audqt::prefswin_show_plugin_page (PluginType::Output); }
static void configure_visualizations () { audqt::prefswin_show_plugin_page (PluginType::Vis); }

static void skins_volume_up () { mainwin_set_volume_diff (5); }
static void skins_volume_down () { mainwin_set_volume_diff (-5); }

static const audqt::MenuItem output_items[] = {
    audqt::MenuCommand ({N_("Volume Up"), "audio-volume-high", "+"}, skins_volume_up),
    audqt::MenuCommand ({N_("Volume Down"), "audio-volume-low", "-"}, skins_volume_down),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Effects ...")}, configure_effects),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Record Stream"), "media-record", "D"}, {nullptr, "record", "set record"}),
    audqt::MenuCommand ({N_("Audio Settings ..."), "audio-card"}, configure_output)
};

static const audqt::MenuItem main_items[] = {
    audqt::MenuCommand ({N_("Open Files ..."), "document-open", "L"}, action_play_file),
    audqt::MenuCommand ({N_("Open Folder ..."), "document-open", "Shift+L"}, action_play_folder),
    audqt::MenuCommand ({N_("Open URL ..."), "folder-remote", "Ctrl+L"}, action_play_location),
//    audqt::MenuCommand ({N_("Search Library"), "edit-find", "Y"}, action_search_tool),
    audqt::MenuSep (),
    audqt::MenuSub ({N_("Playback")}, get_menu_playback),
    audqt::MenuSub ({N_("Playlist")}, get_menu_playlist),
    audqt::MenuSub ({N_("Output")}, {output_items}),
    audqt::MenuSub ({N_("View")}, get_menu_view),
    audqt::MenuSep (),
    audqt::MenuSub ({N_("Services")}, get_plugin_menu_main),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("About ..."), "help-about"}, audqt::aboutwindow_show),
    audqt::MenuCommand ({N_("Settings ..."), "preferences-system", "Ctrl+P"}, audqt::prefswin_show),
    audqt::MenuCommand ({N_("Quit"), "application-exit", "Ctrl+Q"}, aud_quit)
};

static const audqt::MenuItem playback_items[] = {
    audqt::MenuCommand ({N_("Song Info ..."), "dialog-information", "I"}, audqt::infowin_show_current),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Repeat"), "media-playlist-repeat", "R"}, {nullptr, "repeat", "set repeat"}),
    audqt::MenuToggle ({N_("Shuffle"), "media-playlist-shuffle", "S"}, {nullptr, "shuffle", "set shuffle"}),
    audqt::MenuToggle ({N_("Shuffle by Album")}, {nullptr, "album_shuffle", "set album_shuffle"}),
    audqt::MenuToggle ({N_("No Playlist Advance"), nullptr, "Ctrl+N"}, {nullptr, "no_playlist_advance", "set no_playlist_advance"}),
    audqt::MenuToggle ({N_("Stop After This Song"), nullptr, "Ctrl+M"}, {nullptr, "stop_after_current_song", "set stop_after_current_song"}),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Play"), "media-playback-start", "X"}, aud_drct_play),
    audqt::MenuCommand ({N_("Pause"), "media-playback-pause", "C"}, aud_drct_pause),
    audqt::MenuCommand ({N_("Stop"), "media-playback-stop", "V"}, aud_drct_stop),
    audqt::MenuCommand ({N_("Previous"), "media-skip-backward", "Z"}, aud_drct_pl_prev),
    audqt::MenuCommand ({N_("Next"), "media-skip-forward", "B"}, aud_drct_pl_next),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Set A-B Repeat"), nullptr, "A"}, action_ab_set),
    audqt::MenuCommand ({N_("Clear A-B Repeat"), nullptr, "Shift+A"}, action_ab_clear),
#if 0
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Jump to Song ..."), "go-jump", "J"}, audgui_jump_to_track),
    audqt::MenuCommand ({N_("Jump to Time ..."), "go-jump", "Ctrl+J"}, audgui_jump_to_time)
#endif
};

static const audqt::MenuItem playlist_items[] = {
    audqt::MenuCommand ({N_("Play/Resume"), "media-playback-start", "Shift+Return"}, pl_play),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("New Playlist"), "document-new", "Shift+N"}, pl_new),
    audqt::MenuCommand ({N_("Rename Playlist ..."), "insert-text", "F2"}, action_playlist_rename),
    audqt::MenuCommand ({N_("Remove Playlist"), "edit-delete", "Shift+D"}, action_playlist_delete),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Previous Playlist"), "media-skip-backward", "Shift+Tab"}, pl_prev),
    audqt::MenuCommand ({N_("Next Playlist"), "media-skip-forward", "Tab"}, pl_next),
    audqt::MenuSep (),
#if 0
    audqt::MenuCommand ({N_("Import Playlist ..."), "document-open", "O"}, audgui_import_playlist),
    audqt::MenuCommand ({N_("Export Playlist ..."), "document-save", "Shift+S"}, audgui_export_playlist),
    audqt::MenuSep (),
#endif
    audqt::MenuCommand ({N_("Playlist Manager ..."), "audio-x-generic", "P"}, action_playlist_manager),
    audqt::MenuCommand ({N_("Queue Manager ..."), nullptr, "Ctrl+U"}, audqt::queue_manager_show),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Refresh Playlist"), "view-refresh", "F5"}, pl_refresh)
};

static const audqt::MenuItem view_items[] = {
    audqt::MenuToggle ({N_("Show Playlist Editor"), nullptr, "Alt+E"}, {"skins", "playlist_visible", "skins set playlist_visible"}, view_apply_show_playlist),
    audqt::MenuToggle ({N_("Show Equalizer"), nullptr, "Alt+G"}, {"skins", "equalizer_visible", "skins set equalizer_visible"}, view_apply_show_equalizer),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Show Remaining Time"), nullptr, "Ctrl+R"}, {"skins", "show_remaining_time", "skins set show_remaining_time"}, view_apply_show_remaining),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Double Size"), nullptr, "Ctrl+D"}, {"skins", "double_size", "skins set double_size"}, view_apply_double_size),
#if 0
    audqt::MenuToggle ({N_("Always on Top"), nullptr, "Ctrl+O"}, {"skins", "always_on_top", "skins set always_on_top"}, view_apply_on_top),
    audqt::MenuToggle ({N_("On All Workspaces"), nullptr, "Ctrl+S"}, {"skins", "sticky", "skins set sticky"}, view_apply_sticky),
#endif
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Roll Up Player"), nullptr, "Ctrl+W"}, {"skins", "player_shaded", "skins set player_shaded"}, view_apply_player_shaded),
    audqt::MenuToggle ({N_("Roll Up Playlist Editor"), nullptr, "Shift+Ctrl+W"}, {"skins", "playlist_shaded", "skins set playlist_shaded"}, view_apply_playlist_shaded),
    audqt::MenuToggle ({N_("Roll Up Equalizer"), nullptr, "Ctrl+Alt+W"}, {"skins", "equalizer_shaded", "skins set equalizer_shaded"}, view_apply_equalizer_shaded),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("_Visualizations ...")}, configure_visualizations)
};

static const audqt::MenuItem playlist_add_items[] = {
    audqt::MenuSub ({N_("Services")}, get_plugin_menu_playlist_add),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Add URL ..."), "folder-remote", "Ctrl+H"}, action_playlist_add_url),
    audqt::MenuCommand ({N_("Add Folder ..."), "list-add", "Shift+F"}, action_playlist_add_folder),
    audqt::MenuCommand ({N_("Add Files ..."), "list-add", "F"}, action_playlist_add_files)
};

static const audqt::MenuItem dupe_items[] = {
    audqt::MenuCommand ({N_("By Title")}, rm_dupes_title),
    audqt::MenuCommand ({N_("By File Name")}, rm_dupes_filename),
    audqt::MenuCommand ({N_("By File Path")}, rm_dupes_path)
};

static const audqt::MenuItem playlist_remove_items[] = {
    audqt::MenuSub ({N_("Services")}, get_plugin_menu_playlist_remove),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Remove All"), "edit-delete"}, pl_remove_all),
    audqt::MenuCommand ({N_("Clear Queue"), "edit-clear", "Shift+Q"}, pl_queue_clear),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Remove Unavailable Files"), "dialog-warning"}, pl_remove_failed),
    audqt::MenuSub ({N_("Remove Duplicates"), "edit-copy"}, {dupe_items}),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Remove Unselected"), "list-remove"}, pl_remove_unselected),
    audqt::MenuCommand ({N_("Remove Selected"), "list-remove", "Delete"}, pl_remove_selected)
};

static const audqt::MenuItem playlist_select_items[] = {
    audqt::MenuCommand ({N_("Invert Selection")}, pl_select_invert),
    audqt::MenuCommand ({N_("Select None"), nullptr, "Shift+Ctrl+A"}, pl_select_none),
    audqt::MenuCommand ({N_("Select All"), "edit-select-all", "Ctrl+A"}, pl_select_all),
};

static const audqt::MenuItem sort_items[] = {
    audqt::MenuCommand ({N_("By Track Number")}, sort_track),
    audqt::MenuCommand ({N_("By Title")}, sort_title),
    audqt::MenuCommand ({N_("By Artist")}, sort_artist),
    audqt::MenuCommand ({N_("By Album")}, sort_album),
    audqt::MenuCommand ({N_("By Album Artist")}, sort_album_artist),
    audqt::MenuCommand ({N_("By Release Date")}, sort_date),
    audqt::MenuCommand ({N_("By Genre")}, sort_genre),
    audqt::MenuCommand ({N_("By Length")}, sort_length),
    audqt::MenuCommand ({N_("By File Name")}, sort_filename),
    audqt::MenuCommand ({N_("By File Path")}, sort_path),
    audqt::MenuCommand ({N_("By Custom Title")}, sort_custom_title),
    audqt::MenuCommand ({N_("By Comment")}, sort_comment)
};

static const audqt::MenuItem sort_selected_items[] = {
    audqt::MenuCommand ({N_("By Track Number")}, sort_sel_track),
    audqt::MenuCommand ({N_("By Title")}, sort_sel_title),
    audqt::MenuCommand ({N_("By Artist")}, sort_sel_artist),
    audqt::MenuCommand ({N_("By Album")}, sort_sel_album),
    audqt::MenuCommand ({N_("By Album Artist")}, sort_sel_album_artist),
    audqt::MenuCommand ({N_("By Genre")}, sort_sel_genre),
    audqt::MenuCommand ({N_("By Release Date")}, sort_sel_date),
    audqt::MenuCommand ({N_("By Length")}, sort_sel_length),
    audqt::MenuCommand ({N_("By File Name")}, sort_sel_filename),
    audqt::MenuCommand ({N_("By File Path")}, sort_sel_path),
    audqt::MenuCommand ({N_("By Custom Title")}, sort_sel_custom_title),
    audqt::MenuCommand ({N_("By Comment")}, sort_sel_comment)
};

static const audqt::MenuItem playlist_sort_items[] = {
    audqt::MenuCommand ({N_("Randomize List"), nullptr, "Shift+Ctrl+R"}, sort_random),
    audqt::MenuCommand ({N_("Reverse List"), "view-sort-descending"}, sort_reverse),
    audqt::MenuSep (),
    audqt::MenuSub ({N_("Sort Selected"), "view-sort-ascending"}, {sort_selected_items}),
    audqt::MenuSub ({N_("Sort List"), "view-sort-ascending"}, {sort_items})
};

static const audqt::MenuItem playlist_context_items[] = {
    audqt::MenuCommand ({N_("Song Info ..."), "dialog-information", "Alt+I"}, pl_song_info),
    audqt::MenuCommand ({N_("Open Containing Folder"), "folder"}, pl_open_folder),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Cut"), "edit-cut", "Ctrl+X"}, pl_cut),
    audqt::MenuCommand ({N_("Copy"), "edit-copy", "Ctrl+C"}, pl_copy),
    audqt::MenuCommand ({N_("Paste"), "edit-paste", "Ctrl+V"}, pl_paste),
    audqt::MenuCommand ({N_("Paste at End"), "edit-paste", "Shift+Ctrl+V"}, pl_paste_end),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Queue/Unqueue"), nullptr, "Q"}, pl_queue_toggle),
    audqt::MenuSep (),
    audqt::MenuSub ({N_("Services")}, get_plugin_menu_playlist)
};

void menu_init (QWidget * parent)
{
    static const ArrayRef<audqt::MenuItem> table[] = {
        {main_items},
        {playback_items},
        {playlist_items},
        {view_items},
        {playlist_add_items},
        {playlist_remove_items},
        {playlist_select_items},
        {playlist_sort_items},
        {playlist_context_items}
    };

    for (int i = UI_MENUS; i --; )
        menus[i] = audqt::menu_build (table[i], parent);
}

void menu_popup (int id, int x, int y, bool leftward, bool upward)
{
    if (leftward || upward)
    {
        QSize size = menus[id]->sizeHint ();
        if (leftward)
            x -= size.width ();
        if (upward)
            y -= size.height ();
    }

    menus[id]->popup (QPoint (x, y));
}
