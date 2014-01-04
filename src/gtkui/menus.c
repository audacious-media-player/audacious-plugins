/*
 * menus.c
 * Copyright 2011-2014 John Lindgren
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/menu.h>

#include "gtkui.h"
#include "playlist_util.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

#define SHIFT GDK_SHIFT_MASK
#define CTRL GDK_CONTROL_MASK
#define ALT GDK_MOD1_MASK

int menu_tab_playlist_id = -1; /* should really be stored in the menu somehow */

static void open_files (void) {audgui_run_filebrowser (TRUE); }
static void open_url (void) {audgui_show_add_url_window (TRUE); }
static void add_files (void) {audgui_run_filebrowser (FALSE); }
static void add_url (void) {audgui_show_add_url_window (FALSE); }

static void configure_effects (void) {aud_show_prefs_for_plugin_type (PLUGIN_TYPE_EFFECT); }
static void configure_visualizations (void) {aud_show_prefs_for_plugin_type (PLUGIN_TYPE_VIS); }

static void pl_sort_track (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TRACK); }
static void pl_sort_title (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); }
static void pl_sort_artist (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ARTIST); }
static void pl_sort_album (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ALBUM); }
static void pl_sort_date (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_DATE); }
static void pl_sort_length (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_LENGTH); }
static void pl_sort_path (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); }
static void pl_sort_custom (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FORMATTED_TITLE); }
static void pl_reverse (void) {aud_playlist_reverse (aud_playlist_get_active ()); }
static void pl_random (void) {aud_playlist_randomize (aud_playlist_get_active ()); }

static void pl_sort_selected_track (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TRACK); }
static void pl_sort_selected_title (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); }
static void pl_sort_selected_artist (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ARTIST); }
static void pl_sort_selected_album (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ALBUM); }
static void pl_sort_selected_date (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_DATE); }
static void pl_sort_selected_length (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_LENGTH); }
static void pl_sort_selected_path (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); }
static void pl_sort_selected_custom (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FORMATTED_TITLE); }
static void pl_reverse_selected (void) {aud_playlist_reverse_selected (aud_playlist_get_active ()); }
static void pl_random_selected (void) {aud_playlist_randomize_selected (aud_playlist_get_active ()); }

static void pl_new (void)
{
    aud_playlist_insert (-1);
    aud_playlist_set_active (aud_playlist_count () - 1);
}

static void pl_play (void) {aud_drct_play_playlist (aud_playlist_get_active ()); }
static void pl_refresh (void) {aud_playlist_rescan (aud_playlist_get_active ()); }
static void pl_remove_failed (void) {aud_playlist_remove_failed (aud_playlist_get_active ()); }
static void pl_remove_dupes_by_title (void) {aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); }
static void pl_remove_dupes_by_filename (void) {aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FILENAME); }
static void pl_remove_dupes_by_path (void) {aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); }
static void pl_rename (void) {start_rename_playlist (aud_playlist_get_active ()); }
static void pl_close (void) {audgui_confirm_playlist_delete (aud_playlist_get_active ()); }
static void pl_refresh_sel (void) {aud_playlist_rescan_selected (aud_playlist_get_active ()); }
static void pl_select_all (void) {aud_playlist_select_all (aud_playlist_get_active (), TRUE); }

static void pl_tab_play (void)
{
    int playlist = aud_playlist_by_unique_id (menu_tab_playlist_id);
    if (playlist >= 0)
        aud_drct_play_playlist (playlist);
}

static void pl_tab_rename (void)
{
    int playlist = aud_playlist_by_unique_id (menu_tab_playlist_id);
    if (playlist >= 0)
        start_rename_playlist (playlist);
}

static void pl_tab_close (void)
{
    int playlist = aud_playlist_by_unique_id (menu_tab_playlist_id);
    if (playlist >= 0)
        audgui_confirm_playlist_delete (playlist);
}

static GtkWidget * get_services_main (void) {return aud_get_plugin_menu (AUD_MENU_MAIN); }
static GtkWidget * get_services_pl (void) {return aud_get_plugin_menu (AUD_MENU_PLAYLIST_RCLICK); }

static void volume_up (void)
{
    int vol = 0;
    aud_drct_get_volume_main (& vol);
    aud_drct_set_volume_main (vol + 5);
}

static void volume_down (void)
{
    int vol = 0;
    aud_drct_get_volume_main (& vol);
    aud_drct_set_volume_main (vol - 5);
}

static const AudguiMenuItem file_items[] = {
 {N_("_Open Files ..."), "document-open", 'o', CTRL, .func = open_files},
 {N_("Open _URL ..."), "folder-remote", 'l', CTRL, .func = open_url},
 {N_("_Add Files ..."), "list-add", 'o', SHIFT | CTRL, .func = add_files},
 {N_("Add U_RL ..."), "folder-remote", 'l', SHIFT | CTRL, .func = add_url},
 {.sep = TRUE},
 {N_("Search _Library"), "edit-find", 'y', CTRL, .func = activate_search_tool},
 {.sep = TRUE},
 {N_("A_bout ..."), "help-about", .func = audgui_show_about_window},
 {N_("_Settings ..."), "preferences-system", .func = aud_show_prefs_window},
 {N_("_Quit"), "application-exit", 'q', CTRL, .func = aud_drct_quit}};

static const AudguiMenuItem playback_items[] = {
 {N_("_Play"), "media-playback-start", GDK_KEY_Return, CTRL, .func = aud_drct_play},
 {N_("Paus_e"), "media-playback-pause", ',', CTRL, .func = aud_drct_pause},
 {N_("_Stop"), "media-playback-stop", '.', CTRL, .func = aud_drct_stop},
 {N_("Pre_vious"), "media-skip-backward", GDK_KEY_Up, ALT, .func = aud_drct_pl_prev},
 {N_("_Next"), "media-skip-forward", GDK_KEY_Down, ALT, .func = aud_drct_pl_next},
 {.sep = TRUE},
 {N_("_Repeat"), NULL, 'r', CTRL, .cname = "repeat", .hook = "set repeat"},
 {N_("S_huffle"), NULL, 's', CTRL, .cname = "shuffle", .hook = "set shuffle"},
 {N_("N_o Playlist Advance"), NULL, 'n', CTRL,
  .cname = "no_playlist_advance", .hook = "set no_playlist_advance"},
 {N_("Stop A_fter This Song"), NULL, 'm', CTRL,
  .cname = "stop_after_current_song", .hook = "set stop_after_current_song"},
 {.sep = TRUE},
 {N_("Song _Info ..."), "dialog-information", 'i', CTRL, .func = audgui_infowin_show_current},
 {N_("Jump to _Time ..."), "go-jump", 'k', CTRL, .func = audgui_jump_to_time},
 {N_("_Jump to Song ..."), "go-jump", 'j', CTRL, .func = audgui_jump_to_track},
 {.sep = TRUE},
 {N_("Set Repeat Point _A"), NULL, '1', CTRL, .func = set_ab_repeat_a},
 {N_("Set Repeat Point _B"), NULL, '2', CTRL, .func = set_ab_repeat_b},
 {N_("_Clear Repeat Points"), NULL, '3', CTRL, .func = clear_ab_repeat}};

static const AudguiMenuItem dupe_items[] = {
 {N_("By _Title"), .func = pl_remove_dupes_by_title},
 {N_("By _Filename"), .func = pl_remove_dupes_by_filename},
 {N_("By File _Path"), .func = pl_remove_dupes_by_path}};

static const AudguiMenuItem sort_items[] = {
 {N_("By Track _Number"), .func = pl_sort_track},
 {N_("By _Title"), .func = pl_sort_title},
 {N_("By _Artist"), .func = pl_sort_artist},
 {N_("By Al_bum"), .func = pl_sort_album},
 {N_("By Release _Date"), .func = pl_sort_date},
 {N_("By _Length"), .func = pl_sort_length},
 {N_("By _File Path"), .func = pl_sort_path},
 {N_("By _Custom Title"), .func = pl_sort_custom},
 {.sep = TRUE},
 {N_("R_everse Order"), "view-sort-descending", .func = pl_reverse},
 {N_("_Random Order"), .func = pl_random}};

 static const AudguiMenuItem sort_selected_items[] = {
 {N_("By Track _Number"), .func = pl_sort_selected_track},
 {N_("By _Title"), .func = pl_sort_selected_title},
 {N_("By _Artist"), .func = pl_sort_selected_artist},
 {N_("By Al_bum"), .func = pl_sort_selected_album},
 {N_("By Release _Date"), .func = pl_sort_selected_date},
 {N_("By _Length"), .func = pl_sort_selected_length},
 {N_("By _File Path"), .func = pl_sort_selected_path},
 {N_("By _Custom Title"), .func = pl_sort_selected_custom},
 {.sep = TRUE},
 {N_("R_everse Order"), "view-sort-descending", .func = pl_reverse_selected},
 {N_("_Random Order"), .func = pl_random_selected}};

static const AudguiMenuItem playlist_items[] = {
 {N_("_Play This Playlist"), "media-playback-start", GDK_KEY_Return, SHIFT | CTRL, .func = pl_play},
 {N_("_Refresh"), "view-refresh", GDK_KEY_F5, .func = pl_refresh},
 {.sep = TRUE},
 {N_("_Sort"), "view-sort-ascending", .items = sort_items, ARRAY_LEN (sort_items)},
 {N_("Sort Se_lected"), "view-sort-ascending", .items = sort_selected_items, ARRAY_LEN (sort_selected_items)},
 {N_("Remove _Duplicates"), "list-remove", .items = dupe_items, ARRAY_LEN (dupe_items)},
 {N_("Remove _Unavailable Files"), "list-remove", .func = pl_remove_failed},
 {.sep = TRUE},
 {N_("_New"), "document-new", 't', CTRL, .func = pl_new},
 {N_("Ren_ame ..."), "insert-text", GDK_KEY_F2, .func = pl_rename},
 {N_("Remo_ve"), "edit-delete", 'w', CTRL, .func = pl_close},
 {.sep = TRUE},
 {N_("_Import ..."), "document-open", .func = audgui_import_playlist},
 {N_("_Export ..."), "document-save", .func = audgui_export_playlist},
 {.sep = TRUE},
 {N_("Playlist _Manager ..."), "audio-x-generic", 'p', CTRL, .func = audgui_playlist_manager},
 {N_("_Queue Manager ..."), NULL, 'u', CTRL, .func = audgui_queue_manager_show}};

static const AudguiMenuItem output_items[] = {
 {N_("Volume _Up"), "audio-volume-high", '+', CTRL, .func = volume_up},
 {N_("Volume _Down"), "audio-volume-low", '-', CTRL, .func = volume_down},
 {.sep = TRUE},
 {N_("_Equalizer"), "multimedia-volume-control", 'e', CTRL, .func = audgui_show_equalizer_window},
 {.sep = TRUE},
 {N_("E_ffects ..."), .func = configure_effects}};

static const AudguiMenuItem view_items[] = {
 {N_("Show _Menu Bar"), NULL, 'm', SHIFT | CTRL,
  .csect = "gtkui", "menu_visible", .func = show_hide_menu},
 {N_("Show I_nfo Bar"), NULL, 'i', SHIFT | CTRL,
  .csect = "gtkui", "infoarea_visible", .func = show_hide_infoarea},
 {N_("Show Info Bar Vis_ualization"),
  .csect = "gtkui", "infoarea_show_vis", .func = show_hide_infoarea_vis},
 {N_("Show _Status Bar"), NULL, 's', SHIFT | CTRL,
  .csect = "gtkui", "statusbar_visible", .func = show_hide_statusbar},
 {.sep = TRUE},
 {N_("Show _Remaining Time"), NULL, 'r', SHIFT | CTRL,
  .csect = "gtkui", "show_remaining_time"},
 {.sep = TRUE},
 {N_("_Visualizations ..."), .func = configure_visualizations}};

static const AudguiMenuItem main_items[] = {
 {N_("_File"), .items = file_items, ARRAY_LEN (file_items)},
 {N_("_Playback"), .items = playback_items, ARRAY_LEN (playback_items)},
 {N_("P_laylist"), .items = playlist_items, ARRAY_LEN (playlist_items)},
 {N_("_Services"), .get_sub = get_services_main},
 {N_("_Output"), .items = output_items, ARRAY_LEN (output_items)},
 {N_("_View"), .items = view_items, ARRAY_LEN (view_items)}};

static const AudguiMenuItem rclick_items[] = {
 {N_("Song _Info ..."), "dialog-information", 'i', ALT, .func = playlist_song_info},
 {N_("_Queue/Unqueue"), NULL, 'q', ALT, .func = playlist_queue_toggle},
 {N_("_Refresh"), "view-refresh", GDK_KEY_F6, .func = pl_refresh_sel},
 {.sep = TRUE},
 {N_("Cu_t"), "edit-cut", .func = playlist_cut},
 {N_("_Copy"), "edit-copy", .func = playlist_copy},
 {N_("_Paste"), "edit-paste", .func = playlist_paste},
 {N_("Select _All"), "edit-select-all", .func = pl_select_all},
 {.sep = TRUE},
 {N_("_Services"), .get_sub = get_services_pl}};

static const AudguiMenuItem tab_items[] = {
 {N_("_Play"), "media-playback-start", .func = pl_tab_play},
 {N_("_Rename ..."), "insert-text", .func = pl_tab_rename},
 {N_("Remo_ve"), "edit-delete", .func = pl_tab_close}};


GtkWidget * make_menu_bar (GtkAccelGroup * accel)
{
    GtkWidget * bar = gtk_menu_bar_new ();
    audgui_menu_init (bar, main_items, ARRAY_LEN (main_items), accel);
    return bar;
}

GtkWidget * make_menu_main (GtkAccelGroup * accel)
{
    GtkWidget * shell = gtk_menu_new ();
    audgui_menu_init (shell, main_items, ARRAY_LEN (main_items), accel);
    return shell;
}

GtkWidget * make_menu_rclick (GtkAccelGroup * accel)
{
    GtkWidget * shell = gtk_menu_new ();
    audgui_menu_init (shell, rclick_items, ARRAY_LEN (rclick_items), accel);
    return shell;
}

GtkWidget * make_menu_tab (GtkAccelGroup * accel)
{
    GtkWidget * shell = gtk_menu_new ();
    audgui_menu_init (shell, tab_items, ARRAY_LEN (tab_items), accel);
    return shell;
}
