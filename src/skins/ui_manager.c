/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "ui_manager.h"
#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "actions-equalizer.h"

/* TODO ui_main.h is only included because ui_manager.c needs the values of
   TimerMode enum; move that enum elsewhere so we can get rid of this include */
#include "ui_main.h"

#if 0
#include "sync-menu.h"
#endif
#include "plugin.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static GtkUIManager *ui_manager = NULL;
static GList * attached_menus = NULL;

/* toggle action entries */

static GtkToggleActionEntry toggleaction_entries_others[] = {

    { "autoscroll songname", NULL , N_("Autoscroll Songname"), NULL,
      N_("Autoscroll Songname"), G_CALLBACK(action_autoscroll_songname) , FALSE },

    { "stop after current song", NULL , N_("Stop after Current Song"), "<Ctrl>M",
      N_("Stop after Current Song"), G_CALLBACK(action_stop_after_current_song) , FALSE },

    { "anamode peaks", NULL , N_("Peaks"), NULL,
      N_("Peaks"), G_CALLBACK(action_anamode_peaks) , FALSE },

    { "playback repeat", NULL , N_("Repeat"), "R",
      N_("Repeat"), G_CALLBACK(action_playback_repeat) , FALSE },

    { "playback shuffle", NULL , N_("Shuffle"), "S",
      N_("Shuffle"), G_CALLBACK(action_playback_shuffle) , FALSE },

    { "playback no playlist advance", NULL , N_("No Playlist Advance"), "<Ctrl>N",
      N_("No Playlist Advance"), G_CALLBACK(action_playback_noplaylistadvance) , FALSE },

    { "show player", NULL , N_("Show Player"), "<Alt>M",
      N_("Show Player"), G_CALLBACK(action_show_player) , FALSE },

    { "show playlist editor", NULL , N_("Show Playlist Editor"), "<Alt>E",
      N_("Show Playlist Editor"), G_CALLBACK(action_show_playlist_editor) , FALSE },

    { "show equalizer", NULL , N_("Show Equalizer"), "<Alt>G",
      N_("Show Equalizer"), G_CALLBACK(action_show_equalizer) , FALSE },

    { "view always on top", NULL , N_("Always on Top"), "<Ctrl>O",
      N_("Always on Top"), G_CALLBACK(action_view_always_on_top) , FALSE },

    { "view put on all workspaces", NULL , N_("Put on All Workspaces"), "<Ctrl>S",
      N_("Put on All Workspaces"), G_CALLBACK(action_view_on_all_workspaces) , FALSE },

    { "roll up player", NULL , N_("Roll up Player"), "<Ctrl>W",
      N_("Roll up Player"), G_CALLBACK(action_roll_up_player) , FALSE },

    { "roll up playlist editor", NULL , N_("Roll up Playlist Editor"), "<Shift><Ctrl>W",
      N_("Roll up Playlist Editor"), G_CALLBACK(action_roll_up_playlist_editor) , FALSE },

    { "roll up equalizer", NULL , N_("Roll up Equalizer"), "<Ctrl><Alt>W",
      N_("Roll up Equalizer"), G_CALLBACK(action_roll_up_equalizer) , FALSE },

    { "view scaled", NULL , N_("Scale"), "<Ctrl>D",
      N_("DoubleSize"), G_CALLBACK(action_view_scaled) , FALSE },

    { "view easy move", NULL , N_("Easy Move"), "<Ctrl>E",
      N_("Easy Move"), G_CALLBACK(action_view_easymove) , FALSE }
};



/* radio action entries */

static GtkRadioActionEntry radioaction_entries_vismode[] = {
    { "vismode analyzer", NULL , N_("Analyzer"), NULL, N_("Analyzer"), VIS_ANALYZER },
    { "vismode scope", NULL , N_("Scope"), NULL, N_("Scope"), VIS_SCOPE },
    { "vismode voiceprint", NULL , N_("Voiceprint"), NULL, N_("Voiceprint"), VIS_VOICEPRINT },
    { "vismode off", NULL , N_("Off"), NULL, N_("Off"), VIS_OFF }
};

static GtkRadioActionEntry radioaction_entries_anamode[] = {
    { "anamode normal", NULL , N_("Normal"), NULL, N_("Normal"), ANALYZER_NORMAL },
    { "anamode fire", NULL , N_("Fire"), NULL, N_("Fire"), ANALYZER_FIRE },
    { "anamode vertical lines", NULL , N_("Vertical Lines"), NULL, N_("Vertical Lines"), ANALYZER_VLINES }
};

static GtkRadioActionEntry radioaction_entries_anatype[] = {
    { "anatype lines", NULL , N_("Lines"), NULL, N_("Lines"), ANALYZER_LINES },
    { "anatype bars", NULL , N_("Bars"), NULL, N_("Bars"), ANALYZER_BARS }
};

static GtkRadioActionEntry radioaction_entries_scomode[] = {
    { "scomode dot", NULL , N_("Dot Scope"), NULL, N_("Dot Scope"), SCOPE_DOT },
    { "scomode line", NULL , N_("Line Scope"), NULL, N_("Line Scope"), SCOPE_LINE },
    { "scomode solid", NULL , N_("Solid Scope"), NULL, N_("Solid Scope"), SCOPE_SOLID }
};

static GtkRadioActionEntry radioaction_entries_vprmode[] = {
    { "vprmode normal", NULL , N_("Normal"), NULL, N_("Normal"), VOICEPRINT_NORMAL },
    { "vprmode fire", NULL , N_("Fire"), NULL, N_("Fire"), VOICEPRINT_FIRE },
    { "vprmode ice", NULL , N_("Ice"), NULL, N_("Ice"), VOICEPRINT_ICE }
};

static GtkRadioActionEntry radioaction_entries_wshmode[] = {
    { "wshmode normal", NULL , N_("Normal"), NULL, N_("Normal"), VU_NORMAL },
    { "wshmode smooth", NULL , N_("Smooth"), NULL, N_("Smooth"), VU_SMOOTH }
};

static GtkRadioActionEntry radioaction_entries_anafoff[] = {
    { "anafoff slowest", NULL , N_("Slowest"), NULL, N_("Slowest"), FALLOFF_SLOWEST },
    { "anafoff slow", NULL , N_("Slow"), NULL, N_("Slow"), FALLOFF_SLOW },
    { "anafoff medium", NULL , N_("Medium"), NULL, N_("Medium"), FALLOFF_MEDIUM },
    { "anafoff fast", NULL , N_("Fast"), NULL, N_("Fast"), FALLOFF_FAST },
    { "anafoff fastest", NULL , N_("Fastest"), NULL, N_("Fastest"), FALLOFF_FASTEST }
};

static GtkRadioActionEntry radioaction_entries_peafoff[] = {
    { "peafoff slowest", NULL , N_("Slowest"), NULL, N_("Slowest"), FALLOFF_SLOWEST },
    { "peafoff slow", NULL , N_("Slow"), NULL, N_("Slow"), FALLOFF_SLOW },
    { "peafoff medium", NULL , N_("Medium"), NULL, N_("Medium"), FALLOFF_MEDIUM },
    { "peafoff fast", NULL , N_("Fast"), NULL, N_("Fast"), FALLOFF_FAST },
    { "peafoff fastest", NULL , N_("Fastest"), NULL, N_("Fastest"), FALLOFF_FASTEST }
};

static GtkRadioActionEntry radioaction_entries_viewtime[] = {
    { "view time elapsed", NULL , N_("Time Elapsed"), "<Ctrl>E", N_("Time Elapsed"), TIMER_ELAPSED },
    { "view time remaining", NULL , N_("Time Remaining"), "<Ctrl>R", N_("Time Remaining"), TIMER_REMAINING }
};



/* normal actions */

static GtkActionEntry action_entries_playback[] = {

    { "playback", NULL, N_("Playback") },

    { "playback play", GTK_STOCK_MEDIA_PLAY , N_("Play"), "X",
      N_("Play"), G_CALLBACK(action_playback_play) },

    { "playback pause", GTK_STOCK_MEDIA_PAUSE , N_("Pause"), "C",
      N_("Pause"), G_CALLBACK(action_playback_pause) },

    { "playback stop", GTK_STOCK_MEDIA_STOP , N_("Stop"), "V",
      N_("Stop"), G_CALLBACK(action_playback_stop) },

    { "playback previous", GTK_STOCK_MEDIA_PREVIOUS , N_("Previous"), "Z",
      N_("Previous"), G_CALLBACK(action_playback_previous) },

    { "playback next", GTK_STOCK_MEDIA_NEXT , N_("Next"), "B",
      N_("Next"), G_CALLBACK(action_playback_next) }
};


static GtkActionEntry action_entries_visualization[] = {
    { "visualization", NULL, N_("Visualization") },
    { "vismode", NULL, N_("Visualization Mode") },
    { "anamode", NULL, N_("Analyzer Mode") },
    { "scomode", NULL, N_("Scope Mode") },
    { "vprmode", NULL, N_("Voiceprint Mode") },
    { "wshmode", NULL, N_("WindowShade VU Mode") },
    { "anafoff", NULL, N_("Analyzer Falloff") },
    { "peafoff", NULL, N_("Peaks Falloff") }
};

static GtkActionEntry action_entries_playlist[] = {

    { "playlist", NULL, N_("Playlist") },

    { "playlist new", GTK_STOCK_NEW , N_("New Playlist"), "<Shift>N",
      N_("New Playlist"), G_CALLBACK(action_playlist_new) },

    { "playlist select next", GTK_STOCK_MEDIA_NEXT, N_("Select Next Playlist"), "Tab",
      N_("Select Next Playlist"), G_CALLBACK(action_playlist_next) },

    { "playlist select previous", GTK_STOCK_MEDIA_PREVIOUS, N_("Select Previous Playlist"), "<Shift>Tab",
      N_("Select Previous Playlist"), G_CALLBACK(action_playlist_prev) },

    { "playlist delete", GTK_STOCK_DELETE , N_("Delete Playlist"), "<Shift>D",
      N_("Delete Playlist"), G_CALLBACK(action_playlist_delete) },

        {"playlist load", GTK_STOCK_OPEN, N_("Import Playlist"), "O",
          N_("Loads a playlist file into the selected playlist."), G_CALLBACK(action_playlist_load_list) },

        {"playlist save", GTK_STOCK_SAVE, N_("Export Playlist"), "<Shift>S",
          N_("Saves the selected playlist."), G_CALLBACK(action_playlist_save_list) },

        { "playlist save all", GTK_STOCK_SAVE, N_("Save All Playlists"),
         "<Alt>S", N_("Saves all the playlists that are open. Note that this "
         "is done automatically when Audacious quits."),
         action_playlist_save_all_playlists},

        { "playlist refresh", GTK_STOCK_REFRESH, N_("Refresh List"), "F5",
          N_("Refreshes metadata associated with a playlist entry."),
          G_CALLBACK(action_playlist_refresh_list) },

        { "playlist manager", AUD_STOCK_PLAYLIST , N_("List Manager"), "P",
          N_("Opens the playlist manager."),
          G_CALLBACK(action_open_list_manager) }
};

static GtkActionEntry action_entries_view[] = {
 {"view", NULL, N_("View")},
 {"iface menu", NULL, N_("Interface")}};

static GtkActionEntry action_entries_playlist_add[] = {
        { "playlist add url", GTK_STOCK_NETWORK, N_("Add Internet Address..."), "<Ctrl>H",
          N_("Adds a remote track to the playlist."),
          G_CALLBACK(action_playlist_add_url) },

        { "playlist add files", GTK_STOCK_ADD, N_("Add Files..."), "F",
          N_("Adds files to the playlist."),
          G_CALLBACK(action_playlist_add_files) },
};

static GtkActionEntry action_entries_playlist_select[] = {
        { "playlist search and select", GTK_STOCK_FIND, N_("Search and Select"), "<Ctrl>F",
          N_("Searches the playlist and selects playlist entries based on specific criteria."),
          G_CALLBACK(action_playlist_search_and_select) },

        { "playlist invert selection", NULL , N_("Invert Selection"), NULL,
          N_("Inverts the selected and unselected entries."),
          G_CALLBACK(action_playlist_invert_selection) },

        { "playlist select all", NULL , N_("Select All"), "<Ctrl>A",
          N_("Selects all of the playlist entries."),
          G_CALLBACK(action_playlist_select_all) },

        { "playlist select none", NULL , N_("Select None"), "<Shift><Ctrl>A",
          N_("Deselects all of the playlist entries."),
          G_CALLBACK(action_playlist_select_none) },
};

static GtkActionEntry action_entries_playlist_delete[] = {
    { "playlist remove all", GTK_STOCK_CLEAR, N_("Remove All"), NULL,
      N_("Removes all entries from the playlist."),
      G_CALLBACK(action_playlist_remove_all) },

    { "playlist clear queue", GTK_STOCK_CLEAR, N_("Clear Queue"), "<Shift>Q",
      N_("Clears the queue associated with this playlist."),
      G_CALLBACK(action_playlist_clear_queue) },

    { "playlist remove unavailable", GTK_STOCK_DIALOG_ERROR , N_("Remove Unavailable Files"), NULL,
      N_("Removes unavailable files from the playlist."),
      G_CALLBACK(action_playlist_remove_unavailable) },

    { "playlist remove dups menu", NULL , N_("Remove Duplicates") },

    { "playlist remove dups by title", NULL , N_("By Title"), NULL,
      N_("Removes duplicate entries from the playlist by title."),
      G_CALLBACK(action_playlist_remove_dupes_by_title) },

    { "playlist remove dups by filename", NULL , N_("By Filename"), NULL,
      N_("Removes duplicate entries from the playlist by filename."),
      G_CALLBACK(action_playlist_remove_dupes_by_filename) },

    { "playlist remove dups by full path", NULL , N_("By Path + Filename"), NULL,
      N_("Removes duplicate entries from the playlist by their full path."),
      G_CALLBACK(action_playlist_remove_dupes_by_full_path) },

    { "playlist remove unselected", GTK_STOCK_REMOVE, N_("Remove Unselected"), NULL,
      N_("Remove unselected entries from the playlist."),
      G_CALLBACK(action_playlist_remove_unselected) },

    { "playlist remove selected", GTK_STOCK_REMOVE, N_("Remove Selected"), "Delete",
      N_("Remove selected entries from the playlist."),
      G_CALLBACK(action_playlist_remove_selected) },
};

static GtkActionEntry action_entries_playlist_sort[] = {
    { "playlist randomize list", AUD_STOCK_RANDOMIZEPL , N_("Randomize List"), "<Ctrl><Shift>R",
      N_("Randomizes the playlist."),
      G_CALLBACK(action_playlist_randomize_list) },

    { "playlist reverse list", GTK_STOCK_GO_UP , N_("Reverse List"), NULL,
      N_("Reverses the playlist."),
      G_CALLBACK(action_playlist_reverse_list) },

    { "playlist sort menu", GTK_STOCK_GO_DOWN , N_("Sort List") },

    { "playlist sort by title", NULL , N_("By Title"), NULL,
      N_("Sorts the list by title."),
      G_CALLBACK(action_playlist_sort_by_title) },

        { "playlist sort by album", NULL, N_("By Album"), NULL,
          N_("Sorts the list by album."),
          G_CALLBACK(action_playlist_sort_by_album) },

    { "playlist sort by artist", NULL , N_("By Artist"), NULL,
      N_("Sorts the list by artist."),
      G_CALLBACK(action_playlist_sort_by_artist) },

    { "playlist sort by filename", NULL , N_("By Filename"), NULL,
      N_("Sorts the list by filename."),
      G_CALLBACK(action_playlist_sort_by_filename) },

    { "playlist sort by full path", NULL , N_("By Path + Filename"), NULL,
      N_("Sorts the list by full pathname."),
      G_CALLBACK(action_playlist_sort_by_full_path) },

    { "playlist sort by date", NULL , N_("By Date"), NULL,
      N_("Sorts the list by modification time."),
      G_CALLBACK(action_playlist_sort_by_date) },

    { "playlist sort by track number", NULL , N_("By Track Number"), NULL,
      N_("Sorts the list by track number."),
      G_CALLBACK(action_playlist_sort_by_track_number) },

    { "playlist sort selected menu", GTK_STOCK_GO_DOWN , N_("Sort Selected") },

    { "playlist sort selected by title", NULL , N_("By Title"), NULL,
      N_("Sorts the list by title."),
      G_CALLBACK(action_playlist_sort_selected_by_title) },

        { "playlist sort selected by album", NULL, N_("By Album"), NULL,
          N_("Sorts the list by album."),
          G_CALLBACK(action_playlist_sort_selected_by_album) },

    { "playlist sort selected by artist", NULL, N_("By Artist"), NULL,
      N_("Sorts the list by artist."),
      G_CALLBACK(action_playlist_sort_selected_by_artist) },

    { "playlist sort selected by filename", NULL , N_("By Filename"), NULL,
      N_("Sorts the list by filename."),
      G_CALLBACK(action_playlist_sort_selected_by_filename) },

    { "playlist sort selected by full path", NULL , N_("By Path + Filename"), NULL,
      N_("Sorts the list by full pathname."),
      G_CALLBACK(action_playlist_sort_selected_by_full_path) },

    { "playlist sort selected by date", NULL , N_("By Date"), NULL,
      N_("Sorts the list by modification time."),
      G_CALLBACK(action_playlist_sort_selected_by_date) },

    { "playlist sort selected by track number", NULL , N_("By Track Number"), NULL,
      N_("Sorts the list by track number."),
      G_CALLBACK(action_playlist_sort_selected_by_track_number) },
};

static GtkActionEntry action_entries_others[] = {

    { "dummy", NULL, "dummy" },

        /* XXX Carbon support */
        { "file", NULL, N_("File") },
        { "help", NULL, N_("Help") },

    { "plugins-menu", AUD_STOCK_PLUGIN, N_("Plugin Services") },

    { "current track info", GTK_STOCK_INFO , N_("View Track Details"), "I",
      N_("View track details"), G_CALLBACK(action_current_track_info) },

    { "playlist track info", GTK_STOCK_INFO , N_("View Track Details"), "<Alt>I",
      N_("View track details"), G_CALLBACK(action_playlist_track_info) },

    { "about audacious", GTK_STOCK_DIALOG_INFO , N_("About Audacious"), NULL,
      N_("About Audacious"), G_CALLBACK(action_about_audacious) },

    { "play file", GTK_STOCK_OPEN , N_("Play File"), "L",
      N_("Load and play a file"), G_CALLBACK(action_play_file) },

    { "play location", GTK_STOCK_NETWORK , N_("Play Location"), "<Ctrl>L",
      N_("Play media from the selected location"), G_CALLBACK(action_play_location) },

    { "plugins", NULL , N_("Plugin services") },

    { "preferences", GTK_STOCK_PREFERENCES , N_("Preferences"), "<Ctrl>P",
      N_("Open preferences window"), G_CALLBACK(action_preferences) },

    { "quit", GTK_STOCK_QUIT , N_("_Quit"), NULL,
      N_("Quit Audacious"), G_CALLBACK(action_quit) },

    { "ab set", NULL , N_("Set A-B"), "A",
      N_("Set A-B"), G_CALLBACK(action_ab_set) },

    { "ab clear", NULL , N_("Clear A-B"), "<Shift>A",
      N_("Clear A-B"), G_CALLBACK(action_ab_clear) },

    { "jump to playlist start", GTK_STOCK_GOTO_TOP , N_("Jump to Playlist Start"), "<Ctrl>Z",
      N_("Jump to Playlist Start"), G_CALLBACK(action_jump_to_playlist_start) },

    { "jump to file", GTK_STOCK_JUMP_TO , N_("Jump to File"), "J",
      N_("Jump to File"), G_CALLBACK(action_jump_to_file) },

    { "jump to time", GTK_STOCK_JUMP_TO , N_("Jump to Time"), "<Ctrl>J",
      N_("Jump to Time"), G_CALLBACK(action_jump_to_time) },

    { "queue toggle", AUD_STOCK_QUEUETOGGLE , N_("Queue Toggle"), "Q",
      N_("Enables/disables the entry in the playlist's queue."),
      G_CALLBACK(action_queue_toggle) },

    {"playlist copy", GTK_STOCK_COPY, N_("Copy"), "<Ctrl>C", NULL, (GCallback)
     action_playlist_copy},
    {"playlist cut", GTK_STOCK_CUT, N_("Cut"), "<Ctrl>X", NULL, (GCallback)
     action_playlist_cut},
    {"playlist paste", GTK_STOCK_PASTE, N_("Paste"), "<Ctrl>V", NULL,
     (GCallback) action_playlist_paste},
};


static GtkActionEntry action_entries_equalizer[] = {

    { "equ preset load menu", NULL, N_("Load") },
    { "equ preset import menu", NULL, N_("Import") },
    { "equ preset save menu", NULL, N_("Save") },
    { "equ preset delete menu", NULL, N_("Delete") },

    { "equ load preset", NULL, N_("Preset"), NULL,
      N_("Load preset"), G_CALLBACK(action_equ_load_preset) },

    { "equ load auto preset", NULL, N_("Auto-load preset"), NULL,
      N_("Load auto-load preset"), G_CALLBACK(action_equ_load_auto_preset) },

    { "equ load default preset", NULL, N_("Default"), NULL,
      N_("Load default preset into equalizer"), G_CALLBACK(action_equ_load_default_preset) },

    { "equ zero preset", NULL, N_("Zero"), NULL,
      N_("Set equalizer preset levels to zero"), G_CALLBACK(action_equ_zero_preset) },

    { "equ load preset file", NULL, N_("From file"), NULL,
      N_("Load preset from file"), G_CALLBACK(action_equ_load_preset_file) },

    { "equ load preset eqf", NULL, N_("From WinAMP EQF file"), NULL,
      N_("Load preset from WinAMP EQF file"), G_CALLBACK(action_equ_load_preset_eqf) },

    { "equ import winamp presets", NULL, N_("WinAMP Presets"), NULL,
      N_("Import WinAMP presets"), G_CALLBACK(action_equ_import_winamp_presets) },

    { "equ save preset", NULL, N_("Preset"), NULL,
      N_("Save preset"), G_CALLBACK(action_equ_save_preset) },

    { "equ save auto preset", NULL, N_("Auto-load preset"), NULL,
      N_("Save auto-load preset"), G_CALLBACK(action_equ_save_auto_preset) },

    { "equ save default preset", NULL, N_("Default"), NULL,
      N_("Save default preset"), G_CALLBACK(action_equ_save_default_preset) },

    { "equ save preset file", NULL, N_("To file"), NULL,
      N_("Save preset to file"), G_CALLBACK(action_equ_save_preset_file) },

    { "equ save preset eqf", NULL, N_("To WinAMP EQF file"), NULL,
      N_("Save preset to WinAMP EQF file"), G_CALLBACK(action_equ_save_preset_eqf) },

    { "equ delete preset", NULL, N_("Preset"), NULL,
      N_("Delete preset"), G_CALLBACK(action_equ_delete_preset) },

    { "equ delete auto preset", NULL, N_("Auto-load preset"), NULL,
      N_("Delete auto-load preset"), G_CALLBACK(action_equ_delete_auto_preset) }
};



/* ***************************** */


static GtkActionGroup *
ui_manager_new_action_group( const gchar * group_name )
{
  GtkActionGroup *group = gtk_action_group_new( group_name );
  gtk_action_group_set_translation_domain( group , PACKAGE_NAME );
  return group;
}

void
ui_manager_init ( void )
{
  /* toggle actions */
  toggleaction_group_others = ui_manager_new_action_group("toggleaction_others");
  gtk_action_group_add_toggle_actions(
    toggleaction_group_others , toggleaction_entries_others ,
    G_N_ELEMENTS(toggleaction_entries_others) , NULL );

  /* radio actions */
  radioaction_group_anamode = ui_manager_new_action_group("radioaction_anamode");
  gtk_action_group_add_radio_actions(
    radioaction_group_anamode , radioaction_entries_anamode ,
    G_N_ELEMENTS(radioaction_entries_anamode) , 0 , G_CALLBACK(action_anamode) , NULL );

  radioaction_group_anatype = ui_manager_new_action_group("radioaction_anatype");
  gtk_action_group_add_radio_actions(
    radioaction_group_anatype , radioaction_entries_anatype ,
    G_N_ELEMENTS(radioaction_entries_anatype) , 0 , G_CALLBACK(action_anatype) , NULL );

  radioaction_group_scomode = ui_manager_new_action_group("radioaction_scomode");
  gtk_action_group_add_radio_actions(
    radioaction_group_scomode , radioaction_entries_scomode ,
    G_N_ELEMENTS(radioaction_entries_scomode) , 0 , G_CALLBACK(action_scomode) , NULL );

  radioaction_group_vprmode = ui_manager_new_action_group("radioaction_vprmode");
  gtk_action_group_add_radio_actions(
    radioaction_group_vprmode , radioaction_entries_vprmode ,
    G_N_ELEMENTS(radioaction_entries_vprmode) , 0 , G_CALLBACK(action_vprmode) , NULL );

  radioaction_group_wshmode = ui_manager_new_action_group("radioaction_wshmode");
  gtk_action_group_add_radio_actions(
    radioaction_group_wshmode , radioaction_entries_wshmode ,
    G_N_ELEMENTS(radioaction_entries_wshmode) , 0 , G_CALLBACK(action_wshmode) , NULL );

  radioaction_group_anafoff = ui_manager_new_action_group("radioaction_anafoff");
  gtk_action_group_add_radio_actions(
    radioaction_group_anafoff , radioaction_entries_anafoff ,
    G_N_ELEMENTS(radioaction_entries_anafoff) , 0 , G_CALLBACK(action_anafoff) , NULL );

  radioaction_group_peafoff = ui_manager_new_action_group("radioaction_peafoff");
  gtk_action_group_add_radio_actions(
    radioaction_group_peafoff , radioaction_entries_peafoff ,
    G_N_ELEMENTS(radioaction_entries_peafoff) , 0 , G_CALLBACK(action_peafoff) , NULL );

  radioaction_group_vismode = ui_manager_new_action_group("radioaction_vismode");
  gtk_action_group_add_radio_actions(
    radioaction_group_vismode , radioaction_entries_vismode ,
    G_N_ELEMENTS(radioaction_entries_vismode) , 0 , G_CALLBACK(action_vismode) , NULL );

  radioaction_group_viewtime = ui_manager_new_action_group("radioaction_viewtime");
  gtk_action_group_add_radio_actions(
    radioaction_group_viewtime , radioaction_entries_viewtime ,
    G_N_ELEMENTS(radioaction_entries_viewtime) , 0 , G_CALLBACK(action_viewtime) , NULL );

  /* normal actions */
  action_group_playback = ui_manager_new_action_group("action_playback");
    gtk_action_group_add_actions(
    action_group_playback , action_entries_playback ,
    G_N_ELEMENTS(action_entries_playback) , NULL );

  action_group_playlist = ui_manager_new_action_group("action_playlist");
    gtk_action_group_add_actions(
    action_group_playlist , action_entries_playlist ,
    G_N_ELEMENTS(action_entries_playlist) , NULL );

  action_group_visualization = ui_manager_new_action_group("action_visualization");
    gtk_action_group_add_actions(
    action_group_visualization , action_entries_visualization ,
    G_N_ELEMENTS(action_entries_visualization) , NULL );

  action_group_view = ui_manager_new_action_group("action_view");
    gtk_action_group_add_actions(
    action_group_view , action_entries_view ,
    G_N_ELEMENTS(action_entries_view) , NULL );

  action_group_others = ui_manager_new_action_group("action_others");
    gtk_action_group_add_actions(
    action_group_others , action_entries_others ,
    G_N_ELEMENTS(action_entries_others) , NULL );

  action_group_playlist_add = ui_manager_new_action_group("action_playlist_add");
  gtk_action_group_add_actions(
    action_group_playlist_add, action_entries_playlist_add,
    G_N_ELEMENTS(action_entries_playlist_add), NULL );

  action_group_playlist_select = ui_manager_new_action_group("action_playlist_select");
  gtk_action_group_add_actions(
    action_group_playlist_select, action_entries_playlist_select,
    G_N_ELEMENTS(action_entries_playlist_select), NULL );

  action_group_playlist_delete = ui_manager_new_action_group("action_playlist_delete");
  gtk_action_group_add_actions(
    action_group_playlist_delete, action_entries_playlist_delete,
    G_N_ELEMENTS(action_entries_playlist_delete), NULL );

  action_group_playlist_sort = ui_manager_new_action_group("action_playlist_sort");
  gtk_action_group_add_actions(
    action_group_playlist_sort, action_entries_playlist_sort,
    G_N_ELEMENTS(action_entries_playlist_sort), NULL );

  action_group_equalizer = ui_manager_new_action_group("action_equalizer");
  gtk_action_group_add_actions(
    action_group_equalizer, action_entries_equalizer,
    G_N_ELEMENTS(action_entries_equalizer), NULL);

  /* ui */
  ui_manager = gtk_ui_manager_new();
  gtk_ui_manager_insert_action_group( ui_manager , toggleaction_group_others , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_anamode , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_anatype , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_scomode , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_vprmode , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_wshmode , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_anafoff , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_peafoff , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_vismode , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , radioaction_group_viewtime , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_playback , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_playlist , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_visualization , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_view , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_others , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_playlist_add , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_playlist_select , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_playlist_delete , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_playlist_sort , 0 );
  gtk_ui_manager_insert_action_group( ui_manager , action_group_equalizer , 0 );

  return;
}

#ifdef GDK_WINDOWING_QUARTZ
static GtkWidget *carbon_menubar;
#endif

void
ui_manager_create_menus ( void )
{
  GError *gerr = NULL;

  /* attach xml menu definitions */
  gtk_ui_manager_add_ui_from_file( ui_manager , DATA_DIR "/ui/mainwin.ui" , &gerr );

  if ( gerr != NULL )
  {
    g_critical( "Error creating UI<ui/mainwin.ui>: %s" , gerr->message );
    g_error_free( gerr );
    return;
  }

#ifdef GDK_WINDOWING_QUARTZ
  gtk_ui_manager_add_ui_from_file( ui_manager , DATA_DIR "/ui/carbon-menubar.ui" , &gerr );

  if ( gerr != NULL )
  {
    g_critical( "Error creating UI<ui/carbon-menubar.ui>: %s" , gerr->message );
    g_error_free( gerr );
    return;
  }

  carbon_menubar = ui_manager_get_popup_menu( ui_manager , "/carbon-menubar/main-menu" );
  sync_menu_takeover_menu(GTK_MENU_SHELL(carbon_menubar));
#endif

  gtk_ui_manager_add_ui_from_file( ui_manager , DATA_DIR "/ui/playlist.ui" , &gerr );

  if ( gerr != NULL )
  {
    g_critical( "Error creating UI<ui/playlist.ui>: %s" , gerr->message );
    g_error_free( gerr );
    return;
  }

  gtk_ui_manager_add_ui_from_file( ui_manager , DATA_DIR "/ui/equalizer.ui" , &gerr );

  if ( gerr != NULL )
  {
    g_critical( "Error creating UI<ui/equalizer.ui>: %s" , gerr->message );
    g_error_free( gerr );
    return;
  }
}

static GtkWidget * menus[UI_MENUS] = {NULL, NULL, NULL, NULL, NULL, NULL,
 NULL, NULL, NULL, NULL, NULL, NULL};

static GtkWidget * create_menu (gint id)
{
    const struct
    {
        const gchar * name;
        const gchar * plug_name;
        gint plug_id;
    }
    templates[UI_MENUS] =
    {
        {"/mainwin-menus/main-menu", "/mainwin-menus/main-menu/plugins-menu",
         AUDACIOUS_MENU_MAIN},
        {"/mainwin-menus/main-menu/playback", NULL, 0},
        {"/mainwin-menus/main-menu/playlist", NULL, 0},
        {"/mainwin-menus/songname-menu", NULL, 0},
        {"/mainwin-menus/main-menu/view", NULL, 0},
        {"/mainwin-menus/main-menu/visualization", NULL, 0},
        {"/playlist-menus/add-menu", "/playlist-menus/add-menu/plugins-menu",
         AUDACIOUS_MENU_PLAYLIST_ADD},
        {"/playlist-menus/del-menu", "/playlist-menus/del-menu/plugins-menu",
         AUDACIOUS_MENU_PLAYLIST_REMOVE},
        {"/playlist-menus/select-menu", "/playlist-menus/select-menu/"
         "plugins-menu", AUDACIOUS_MENU_PLAYLIST_SELECT},
        {"/playlist-menus/misc-menu", "/playlist-menus/misc-menu/plugins-menu",
         AUDACIOUS_MENU_PLAYLIST_MISC},
        {"/playlist-menus/playlist-menu", "/playlist-menus/playlist-menu/"
         "plugins-menu", AUDACIOUS_MENU_PLAYLIST},
        {"/playlist-menus/playlist-rightclick-menu", "/playlist-menus/"
         "playlist-rightclick-menu/plugins-menu", AUDACIOUS_MENU_PLAYLIST_RCLICK},
        {"/equalizer-menus/preset-menu", NULL, 0},
    };

    if (menus[id] == NULL)
    {
        menus[id] = ui_manager_get_popup_menu(ui_manager, templates[id].name);

        if (templates[id].plug_name != NULL)
        {
            GtkWidget * item = gtk_ui_manager_get_widget (ui_manager,
             templates[id].plug_name);
            GtkWidget * sub = aud_get_plugin_menu (templates[id].plug_id);

            gtk_menu_item_set_submenu (GTK_MENU_ITEM(item), sub);
            attached_menus = g_list_prepend (attached_menus, sub);
        }

        if (id == UI_MENU_MAIN)
            gtk_menu_item_set_submenu ((GtkMenuItem *) gtk_ui_manager_get_widget
             (ui_manager, "/mainwin-menus/main-menu/view/iface menu"),
             audgui_create_iface_menu ());
    }

    return menus[id];
}

static void destroy_menus (void)
{
    for (gint i = 0; i < G_N_ELEMENTS (menus); i ++)
    {
        if (menus[i] != NULL)
        {
            gtk_widget_destroy (menus[i]);
            menus[i] = NULL;
        }
    }
}

GtkAccelGroup *
ui_manager_get_accel_group ( void )
{
  return gtk_ui_manager_get_accel_group( ui_manager );
}


GtkWidget *
ui_manager_get_popup_menu ( GtkUIManager * self , const gchar * path )
{
  GtkWidget *menu_item = gtk_ui_manager_get_widget( self , path );

  if (GTK_IS_MENU_ITEM(menu_item))
    return gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));
  else
    return NULL;
}

static void get_monitor_geometry (GdkScreen * screen, gint x, gint y,
 GdkRectangle * geom)
{
    gint monitors = gdk_screen_get_n_monitors (screen);
    gint count;

    for (count = 0; count < monitors; count ++)
    {
        gdk_screen_get_monitor_geometry (screen, count, geom);

        if (x >= geom->x && x < geom->x + geom->width && y >= geom->y && y <
         geom->y + geom->height)
            return;
    }

    /* fall back to entire screen */
    geom->x = 0;
    geom->y = 0;
    geom->width = gdk_screen_get_width (screen);
    geom->height = gdk_screen_get_height (screen);
}

static void menu_positioner(GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
 void *data)
{
    gint orig_x = ((gint *) data)[0];
    gint orig_y = ((gint *) data)[1];
    gint leftward = ((gint *) data)[2];
    gint upward = ((gint *) data)[3];
    GdkRectangle geom;
    GtkRequisition request;

    get_monitor_geometry (gtk_widget_get_screen ((GtkWidget *) menu), orig_x,
     orig_y, & geom);
    gtk_widget_size_request(GTK_WIDGET(menu), &request);

    if (leftward)
        * x = MAX (orig_x - request.width, geom.x);
    else
        * x = MIN (orig_x, geom.x + geom.width - request.width);

    if (upward)
        * y = MAX (orig_y - request.height, geom.y);
    else
        * y = MIN (orig_y, geom.y + geom.height - request.height);
}

/* FIX ME: This may not handle multiple screens correctly with status icon. */
void ui_popup_menu_show(gint id, gint x, gint y, gboolean leftward, gboolean
 upward, gint button, gint time)
{
    gint position[4] = {x, y, leftward, upward};

    gtk_menu_popup (GTK_MENU(create_menu (id)), NULL, NULL, menu_positioner,
     position, button, time);
}

void
ui_manager_destroy( void )
{
    g_list_foreach (attached_menus, (GFunc) gtk_menu_detach, NULL);
    g_list_free (attached_menus);
    attached_menus = NULL;

    destroy_menus ();

    g_object_unref(G_OBJECT(toggleaction_group_others));
    g_object_unref(G_OBJECT(radioaction_group_anamode));
    g_object_unref(G_OBJECT(radioaction_group_anatype));
    g_object_unref(G_OBJECT(radioaction_group_scomode));
    g_object_unref(G_OBJECT(radioaction_group_vprmode));
    g_object_unref(G_OBJECT(radioaction_group_wshmode));
    g_object_unref(G_OBJECT(radioaction_group_anafoff));
    g_object_unref(G_OBJECT(radioaction_group_peafoff));
    g_object_unref(G_OBJECT(radioaction_group_vismode));
    g_object_unref(G_OBJECT(radioaction_group_viewtime));
    g_object_unref(G_OBJECT(action_group_playback));
    g_object_unref(G_OBJECT(action_group_playlist));
    g_object_unref(G_OBJECT(action_group_visualization));
    g_object_unref(G_OBJECT(action_group_view));
    g_object_unref(G_OBJECT(action_group_others));
    g_object_unref(G_OBJECT(action_group_playlist_add));
    g_object_unref(G_OBJECT(action_group_playlist_select));
    g_object_unref(G_OBJECT(action_group_playlist_delete));
    g_object_unref(G_OBJECT(action_group_playlist_sort));
    g_object_unref(G_OBJECT(action_group_equalizer));
    g_object_unref(G_OBJECT(ui_manager));
}

