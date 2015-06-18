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
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>
#include <libaudqt/menu.h>

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "view.h"

static QMenu * menus[UI_MENUS];

/* note: playback, playlist, and view menus must be created before main menu */
static QMenu * get_menu_playback () {return menus[UI_MENU_PLAYBACK]; }
static QMenu * get_menu_playlist () {return menus[UI_MENU_PLAYLIST]; }
static QMenu * get_menu_view () {return menus[UI_MENU_VIEW]; }

static QMenu * get_plugin_menu_main () {return audqt::menu_get_by_id (AudMenuID::Main); }
static QMenu * get_plugin_menu_playlist () {return audqt::menu_get_by_id (AudMenuID::Playlist); }
static QMenu * get_plugin_menu_playlist_add () {return audqt::menu_get_by_id (AudMenuID::PlaylistAdd); }
static QMenu * get_plugin_menu_playlist_remove () {return audqt::menu_get_by_id (AudMenuID::PlaylistRemove); }

static const audqt::MenuItem main_items[] = {
    audqt::MenuCommand ({N_("Open Files ..."), "document-open", "L"}, action_play_file),
    audqt::MenuCommand ({N_("Open URL ..."), "folder-remote", "Ctrl+L"}, action_play_location),
    audqt::MenuCommand ({N_("Search Library"), "edit-find", "Y"}, action_search_tool),
    audqt::MenuSep (),
    audqt::MenuSub ({N_("Playback")}, get_menu_playback),
    audqt::MenuSub ({N_("Playlist")}, get_menu_playlist),
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
    audqt::MenuToggle ({N_("Repeat"), nullptr, "R"}, {nullptr, "repeat", "set repeat"}),
    audqt::MenuToggle ({N_("Shuffle"), nullptr, "S"}, {nullptr, "shuffle", "set shuffle"}),
    audqt::MenuToggle ({N_("No Playlist Advance"), nullptr, "Ctrl+N"}, {nullptr, "no_playlist_advance", "set no_playlist_advance"}),
    audqt::MenuToggle ({N_("Stop After This Song"), nullptr, "Ctrl+M"}, {nullptr, "stop_after_current_song", "set stop_after_current_song"}),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Play"), "media-playback-start", "X"}, aud_drct_play),
    audqt::MenuCommand ({N_("Pause"), "media-playback-pause", "C"}, aud_drct_pause),
    audqt::MenuCommand ({N_("Stop"), "media-playback-stop", "V"}, aud_drct_stop),
    audqt::MenuCommand ({N_("Previous"), "media-skip-backward", "Z"}, aud_drct_pl_prev),
    audqt::MenuCommand ({N_("Next"), "media-skip-forward", "B"}, aud_drct_pl_next),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Set A-B Repeat"), nullptr, "B"}, action_ab_set),
    audqt::MenuCommand ({N_("Clear A-B Repeat"), nullptr, "Shift+A"}, action_ab_clear),
#if 0
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Jump to Song ..."), "go-jump", "J"}, audgui_jump_to_track),
    audqt::MenuCommand ({N_("Jump to Time ..."), "go-jump", "Ctrl+J"}, audgui_jump_to_time)
#endif
};

static const audqt::MenuItem playlist_items[] = {
    audqt::MenuCommand ({N_("Play/Resume"), "media-playback-start", "Shift+Return"}, action_playlist_play),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("New Playlist"), "document-new", "Shift+N"}, action_playlist_new),
    audqt::MenuCommand ({N_("Rename Playlist ..."), "insert-text", "F2"}, action_playlist_rename),
    audqt::MenuCommand ({N_("Remove Playlist"), "edit-delete", "Shift+D"}, action_playlist_delete),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Previous Playlist"), "media-skip-backward", "Shift+Tab"}, action_playlist_prev),
    audqt::MenuCommand ({N_("Next Playlist"), "media-skip-forward", "Tab"}, action_playlist_next),
    audqt::MenuSep (),
#if 0
    audqt::MenuCommand ({N_("Import Playlist ..."), "document-open", "O"}, audgui_import_playlist),
    audqt::MenuCommand ({N_("Export Playlist ..."), "document-save", "Shift+S"}, audgui_export_playlist),
    audqt::MenuSep (),
#endif
    audqt::MenuCommand ({N_("Playlist Manager ..."), "audio-x-generic", "P"}, action_playlist_manager),
    audqt::MenuCommand ({N_("Queue Manager ..."), nullptr, "Ctrl+U"}, audqt::queue_manager_show),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Refresh Playlist"), "view-refresh", "F5"}, action_playlist_refresh_list)
};

static const audqt::MenuItem view_items[] = {
    audqt::MenuToggle ({N_("Show Playlist Editor"), nullptr, "Alt+E"}, {"skins", "playlist_visible", "skins set playlist_visible"}, view_apply_show_playlist),
    audqt::MenuToggle ({N_("Show Equalizer"), nullptr, "Alt+G"}, {"skins", "equalizer_visible", "skins set equalizer_visible"}, view_apply_show_equalizer),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Show Remaining Time"), nullptr, "Ctrl+R"}, {"skins", "show_remaining_time", "skins set show_remaining_time"}, view_apply_show_remaining),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Always on Top"), nullptr, "Ctrl+O"}, {"skins", "always_on_top", "skins set always_on_top"}, view_apply_on_top),
    audqt::MenuToggle ({N_("On All Workspaces"), nullptr, "Ctrl+S"}, {"skins", "sticky", "skins set sticky"}, view_apply_sticky),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Roll Up Player"), nullptr, "Ctrl+W"}, {"skins", "player_shaded", "skins set player_shaded"}, view_apply_player_shaded),
    audqt::MenuToggle ({N_("Roll Up Playlist Editor"), nullptr, "Shift+Ctrl+W"}, {"skins", "playlist_shaded", "skins set playlist_shaded"}, view_apply_playlist_shaded),
    audqt::MenuToggle ({N_("Roll Up Equalizer"), nullptr, "Ctrl+Alt+W"}, {"skins", "equalizer_shaded", "skins set equalizer_shaded"}, view_apply_equalizer_shaded),
    audqt::MenuSep (),
    audqt::MenuToggle ({N_("Double Size"), nullptr, "Ctrl+D"}, {"skins", "double_size", "skins set double_size"}, view_apply_double_size)
};

static const audqt::MenuItem playlist_add_items[] = {
    audqt::MenuSub ({N_("Services")}, get_plugin_menu_playlist_add),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Add URL ..."), "folder-remote", "Ctrl+H"}, action_playlist_add_url),
    audqt::MenuCommand ({N_("Add Files ..."), "list-add", "F"}, action_playlist_add_files)
};

static const audqt::MenuItem dupe_items[] = {
    audqt::MenuCommand ({N_("By Title")}, action_playlist_remove_dupes_by_title),
    audqt::MenuCommand ({N_("By File Name")}, action_playlist_remove_dupes_by_filename),
    audqt::MenuCommand ({N_("By File Path")}, action_playlist_remove_dupes_by_full_path)
};

static const audqt::MenuItem playlist_remove_items[] = {
    audqt::MenuSub ({N_("Services")}, get_plugin_menu_playlist_remove),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Remove All"), "edit-delete"}, action_playlist_remove_all),
    audqt::MenuCommand ({N_("Clear Queue"), "edit-clear", "Shift+Q"}, action_playlist_clear_queue),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Remove Unavailable Files"), "dialog-warning"}, action_playlist_remove_unavailable),
    audqt::MenuSub ({N_("Remove Duplicates"), "edit-copy"}, {dupe_items}),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Remove Unselected"), "list-remove"}, action_playlist_remove_unselected),
    audqt::MenuCommand ({N_("Remove Selected"), "list-remove", "Delete"}, action_playlist_remove_selected)
};

static const audqt::MenuItem playlist_select_items[] = {
    audqt::MenuCommand ({N_("Invert Selection")}, action_playlist_invert_selection),
    audqt::MenuCommand ({N_("Select None"), nullptr, "Shift+Ctrl+A"}, action_playlist_select_none),
    audqt::MenuCommand ({N_("Select All"), "edit-select-all", "Ctrl+A"}, action_playlist_select_all),
};

static const audqt::MenuItem sort_items[] = {
    audqt::MenuCommand ({N_("By Track Number")}, action_playlist_sort_by_track_number),
    audqt::MenuCommand ({N_("By Title")}, action_playlist_sort_by_title),
    audqt::MenuCommand ({N_("By Artist")}, action_playlist_sort_by_artist),
    audqt::MenuCommand ({N_("By Album")}, action_playlist_sort_by_album),
    audqt::MenuCommand ({N_("By Album Artist")}, action_playlist_sort_by_album_artist),
    audqt::MenuCommand ({N_("By Release Date")}, action_playlist_sort_by_date),
    audqt::MenuCommand ({N_("By Genre")}, action_playlist_sort_by_genre),
    audqt::MenuCommand ({N_("By Length")}, action_playlist_sort_by_length),
    audqt::MenuCommand ({N_("By File Name")}, action_playlist_sort_by_filename),
    audqt::MenuCommand ({N_("By File Path")}, action_playlist_sort_by_full_path),
    audqt::MenuCommand ({N_("By Custom Title")}, action_playlist_sort_by_custom_title)
};

static const audqt::MenuItem sort_selected_items[] = {
    audqt::MenuCommand ({N_("By Track Number")}, action_playlist_sort_selected_by_track_number),
    audqt::MenuCommand ({N_("By Title")}, action_playlist_sort_selected_by_title),
    audqt::MenuCommand ({N_("By Artist")}, action_playlist_sort_selected_by_artist),
    audqt::MenuCommand ({N_("By Album")}, action_playlist_sort_selected_by_album),
    audqt::MenuCommand ({N_("By Album Artist")}, action_playlist_sort_selected_by_album_artist),
    audqt::MenuCommand ({N_("By Genre")}, action_playlist_sort_selected_by_genre),
    audqt::MenuCommand ({N_("By Release Date")}, action_playlist_sort_selected_by_date),
    audqt::MenuCommand ({N_("By Length")}, action_playlist_sort_selected_by_length),
    audqt::MenuCommand ({N_("By File Name")}, action_playlist_sort_selected_by_filename),
    audqt::MenuCommand ({N_("By File Path")}, action_playlist_sort_selected_by_full_path),
    audqt::MenuCommand ({N_("By Custom Title")}, action_playlist_sort_selected_by_custom_title)
};

static const audqt::MenuItem playlist_sort_items[] = {
    audqt::MenuCommand ({N_("Randomize List"), nullptr, "Shift+Ctrl+R"}, action_playlist_randomize_list),
    audqt::MenuCommand ({N_("Reverse List"), "view-sort-descending"}, action_playlist_reverse_list),
    audqt::MenuSep (),
    audqt::MenuSub ({N_("Sort Selected"), "view-sort-ascending"}, {sort_selected_items}),
    audqt::MenuSub ({N_("Sort List"), "view-sort-ascending"}, {sort_items})
};

static const audqt::MenuItem playlist_context_items[] = {
    audqt::MenuCommand ({N_("Song Info ..."), "dialog-information", "Alt+I"}, action_playlist_track_info),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Cut"), "edit-cut", "Ctrl+X"}, action_playlist_cut),
    audqt::MenuCommand ({N_("Copy"), "edit-copy", "Ctrl+C"}, action_playlist_copy),
    audqt::MenuCommand ({N_("Paste"), "edit-paste", "Ctrl+V"}, action_playlist_paste),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Queue/Unqueue"), nullptr, "Q"}, action_queue_toggle),
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
