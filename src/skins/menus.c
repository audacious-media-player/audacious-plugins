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

#include <gdk/gdkkeysyms.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/menu.h>

#include "actions-equalizer.h"
#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "preset-browser.h"
#include "view.h"

#define SHIFT GDK_SHIFT_MASK
#define CTRL GDK_CONTROL_MASK
#define ALT GDK_MOD1_MASK

static GtkWidget * menus[UI_MENUS];
static GtkAccelGroup * accel;

/* note: playback, playlist, and view menus must be created before main menu */
static GtkWidget * get_menu_playback (void) {return menus[UI_MENU_PLAYBACK]; }
static GtkWidget * get_menu_playlist (void) {return menus[UI_MENU_PLAYLIST]; }
static GtkWidget * get_menu_view (void) {return menus[UI_MENU_VIEW]; }

static GtkWidget * get_plugin_menu_main (void) {return aud_get_plugin_menu (AUD_MENU_MAIN); }
static GtkWidget * get_plugin_menu_playlist (void) {return aud_get_plugin_menu (AUD_MENU_PLAYLIST); }
static GtkWidget * get_plugin_menu_playlist_add (void) {return aud_get_plugin_menu (AUD_MENU_PLAYLIST_ADD); }
static GtkWidget * get_plugin_menu_playlist_remove (void) {return aud_get_plugin_menu (AUD_MENU_PLAYLIST_REMOVE); }

static const AudguiMenuItem main_items[] = {
    {N_("Open Files ..."), "document-open", 'l', .func = action_play_file},
    {N_("Open URL ..."), "folder-remote", 'l', CTRL, .func = action_play_location},
    {.sep = TRUE},
    {N_("Playback"), .get_sub = get_menu_playback},
    {N_("Playlist"), .get_sub = get_menu_playlist},
    {N_("View"), .get_sub = get_menu_view},
    {.sep = TRUE},
    {N_("Services"), .get_sub = get_plugin_menu_main},
    {.sep = TRUE},
    {N_("About ..."), "help-about", .func = audgui_show_about_window},
    {N_("Settings ..."), "preferences-system", 'p', CTRL, .func = aud_show_prefs_window},
    {N_("Quit"), "application-exit", 'q', CTRL, .func = aud_drct_quit}
};

static const AudguiMenuItem playback_items[] = {
    {N_("Song Info ..."), "dialog-information", 'i', .func = audgui_infowin_show_current},
    {.sep = TRUE},
    {N_("Repeat"), NULL, 'r', .cname = "repeat", .hook = "set repeat"},
    {N_("Shuffle"), NULL, 's', .cname = "shuffle", .hook = "set shuffle"},
    {N_("No Playlist Advance"), NULL, 'n', CTRL, .cname = "no_playlist_advance", .hook = "set no_playlist_advance"},
    {N_("Stop After This Song"), NULL, 'm', CTRL, .cname = "stop_after_current_song", .hook = "set stop_after_current_song"},
    {.sep = TRUE},
    {N_("Play"), "media-playback-start", 'x', .func = aud_drct_play},
    {N_("Pause"), "media-playback-pause", 'c', .func = aud_drct_pause},
    {N_("Stop"), "media-playback-stop", 'v', .func = aud_drct_stop},
    {N_("Previous"), "media-skip-backward", 'z', .func = aud_drct_pl_prev},
    {N_("Next"), "media-skip-forward", 'b', .func = aud_drct_pl_next},
    {.sep = TRUE},
    {N_("Set A-B Repeat"), NULL, 'a', .func = action_ab_set},
    {N_("Clear A-B Repeat"), NULL, 'a', SHIFT, .func = action_ab_clear},
    {.sep = TRUE},
    {N_("Jump to Song ..."), "go-jump", 'j',.func = audgui_jump_to_track},
    {N_("Jump to Time ..."), "go-jump", 'j', CTRL, .func = audgui_jump_to_time}
};

static const AudguiMenuItem playlist_items[] = {
    {N_("Play This Playlist"), "media-playback-start", GDK_KEY_Return, SHIFT, .func = action_playlist_play},
    {.sep = TRUE},
    {N_("New Playlist"), "document-new", 'n', SHIFT, .func = action_playlist_new},
    {N_("Rename Playlist ..."), "insert-text", GDK_KEY_F2, .func = action_playlist_rename},
    {N_("Remove Playlist"), "edit-delete", 'd', SHIFT, .func = action_playlist_delete},
    {.sep = TRUE},
    {N_("Previous Playlist"), "media-skip-backward", GDK_KEY_Tab, SHIFT, .func = action_playlist_prev},
    {N_("Next Playlist"), "media-skip-forward", GDK_KEY_Tab, .func = action_playlist_next},
    {.sep = TRUE},
    {N_("Import Playlist ..."), "document-open", 'o', .func = audgui_import_playlist},
    {N_("Export Playlist ..."), "document-save", 's', SHIFT, .func = audgui_export_playlist},
    {.sep = TRUE},
    {N_("Playlist Manager ..."), "audio-x-generic", 'p', .func = audgui_playlist_manager},
    {N_("Queue Manager ..."), NULL, 'u', CTRL, .func = audgui_queue_manager_show},
    {.sep = TRUE},
    {N_("Refresh Playlist"), "view-refresh", GDK_KEY_F5, .func = action_playlist_refresh_list}
};

static const AudguiMenuItem view_items[] = {
    {N_("Show Playlist Editor"), NULL, 'e', ALT, .csect = "skins", "playlist_visible",
     .hook = "skins set playlist_visible", .func = view_apply_show_playlist},
    {N_("Show Equalizer"), NULL, 'g', ALT, .csect = "skins", "equalizer_visible",
     .hook = "skins set equalizer_visible", .func = view_apply_show_equalizer},
    {.sep = TRUE},
    {N_("Show Remaining Time"), NULL, 'r', CTRL, .csect = "skins", "show_remaining_time",
     .hook = "skins set show_remaining_time", .func = view_apply_show_remaining},
    {.sep = TRUE},
    {N_("Always on Top"), NULL, 'o', CTRL, .csect = "skins", "always_on_top",
     .hook = "skins set always_on_top", .func = view_apply_on_top},
    {N_("On All Workspaces"), NULL, 's', CTRL, .csect = "skins", "sticky",
     .hook = "skins set sticky", .func = view_apply_sticky},
    {.sep = TRUE},
    {N_("Roll Up Player"), NULL, 'w', CTRL, .csect = "skins", "player_shaded",
     .hook = "skins set player_shaded", .func = view_apply_player_shaded},
    {N_("Roll Up Playlist Editor"), NULL, 'w', SHIFT | CTRL, .csect = "skins", "playlist_shaded",
     .hook = "skins set playlist_shaded", .func = view_apply_playlist_shaded},
    {N_("Roll Up Equalizer"), NULL, 'w', CTRL | ALT, .csect = "skins", "equalizer_shaded",
     .hook = "skins set equalizer_shaded", .func = view_apply_equalizer_shaded}
};

static const AudguiMenuItem playlist_add_items[] = {
    {N_("Services"), .get_sub = get_plugin_menu_playlist_add},
    {.sep = TRUE},
    {N_("Add URL ..."), "folder-remote", 'h', CTRL, .func = action_playlist_add_url},
    {N_("Add Files ..."), "list-add", 'f', .func = action_playlist_add_files}
};

static const AudguiMenuItem dupe_items[] = {
    {N_("By Title"), .func = action_playlist_remove_dupes_by_title},
    {N_("By Filename"), .func = action_playlist_remove_dupes_by_filename},
    {N_("By File Path"), .func = action_playlist_remove_dupes_by_full_path}
};

static const AudguiMenuItem playlist_remove_items[] = {
    {N_("Services"), .get_sub = get_plugin_menu_playlist_remove},
    {.sep = TRUE},
    {N_("Remove All"), "edit-delete", .func = action_playlist_remove_all},
    {N_("Clear Queue"), "edit-clear", 'q', SHIFT, .func = action_playlist_clear_queue},
    {.sep = TRUE},
    {N_("Remove Unavailable Files"), "dialog-warning", .func = action_playlist_remove_unavailable},
    {N_("Remove Duplicates"), "edit-copy", .items = dupe_items, ARRAY_LEN (dupe_items)},
    {.sep = TRUE},
    {N_("Remove Unselected"), "list-remove", .func = action_playlist_remove_unselected},
    {N_("Remove Selected"), "list-remove", GDK_KEY_Delete, .func = action_playlist_remove_selected}
};

static const AudguiMenuItem playlist_select_items[] = {
    {N_("Search and Select"), "edit-find", 'f', CTRL, .func = action_playlist_search_and_select},
    {.sep = TRUE},
    {N_("Invert Selection"), .func = action_playlist_invert_selection},
    {N_("Select None"), NULL, 'a', SHIFT | CTRL, .func = action_playlist_select_none},
    {N_("Select All"), "edit-select-all", 'a', CTRL, .func = action_playlist_select_all},
};

static const AudguiMenuItem sort_items[] = {
    {N_("By Title"), .func = action_playlist_sort_by_title},
    {N_("By Album"), .func = action_playlist_sort_by_album},
    {N_("By Artist"), .func = action_playlist_sort_by_artist},
    {N_("By Filename"), .func = action_playlist_sort_by_filename},
    {N_("By File Path"), .func = action_playlist_sort_by_full_path},
    {N_("By Release Date"), .func = action_playlist_sort_by_date},
    {N_("By Track Number"), .func = action_playlist_sort_by_track_number}
};

static const AudguiMenuItem sort_selected_items[] = {
    {N_("By Title"), .func = action_playlist_sort_selected_by_title},
    {N_("By Album"), .func = action_playlist_sort_selected_by_album},
    {N_("By Artist"), .func = action_playlist_sort_selected_by_artist},
    {N_("By Filename"), .func = action_playlist_sort_selected_by_filename},
    {N_("By File Path"), .func = action_playlist_sort_selected_by_full_path},
    {N_("By Release Date"), .func = action_playlist_sort_selected_by_date},
    {N_("By Track Number"), .func = action_playlist_sort_selected_by_track_number}
};

static const AudguiMenuItem playlist_sort_items[] = {
    {N_("Randomize List"), NULL, 'r', SHIFT | CTRL, .func = action_playlist_randomize_list},
    {N_("Reverse List"), "view-sort-descending", .func = action_playlist_reverse_list},
    {.sep = TRUE},
    {N_("Sort Selected"), "view-sort-ascending", .items = sort_selected_items, ARRAY_LEN (sort_selected_items)},
    {N_("Sort List"), "view-sort-ascending", .items = sort_items, ARRAY_LEN (sort_items)}
};

static const AudguiMenuItem playlist_context_items[] = {
    {N_("Song Info ..."), "dialog-information", 'i', ALT, .func = action_playlist_track_info},
    {.sep = TRUE},
    {N_("Cut"), "edit-cut", 'x', CTRL, .func = action_playlist_cut},
    {N_("Copy"), "edit-copy", 'c', CTRL, .func = action_playlist_copy},
    {N_("Paste"), "edit-paste", 'v', CTRL, .func = action_playlist_paste},
    {.sep = TRUE},
    {N_("Queue/Unqueue"), NULL, 'q', .func = action_queue_toggle},
    {.sep = TRUE},
    {N_("Services"), .get_sub = get_plugin_menu_playlist}
};

static const AudguiMenuItem eq_preset_items[] = {
    {N_("Load Preset ..."), "document-open", .func = action_equ_load_preset},
    {N_("Load Auto Preset ..."), .func = action_equ_load_auto_preset},
    {N_("Load Default"), .func = action_equ_load_default_preset},
    {N_("Load Preset File ..."), .func = eq_preset_load_file},
    {N_("Load EQF File ..."), .func = eq_preset_load_eqf},
    {.sep = TRUE},
    {N_("Save Preset ..."), "document-save", .func = action_equ_save_preset},
    {N_("Save Auto Preset ..."), .func = action_equ_save_auto_preset},
    {N_("Save Default"), .func = action_equ_save_default_preset},
    {N_("Save Preset File ..."), .func = eq_preset_save_file},
    {N_("Save EQF File ..."), .func = eq_preset_save_eqf},
    {.sep = TRUE},
    {N_("Delete Preset ..."), "edit-delete", .func = action_equ_delete_preset},
    {N_("Delete Auto Preset ..."), .func = action_equ_delete_auto_preset},
    {.sep = TRUE},
    {N_("Import Winamp Presets ..."), "document-open", .func = eq_preset_import_winamp},
    {.sep = TRUE},
    {N_("Reset to Zero"), "edit-clear", .func = action_equ_zero_preset}
};

void menu_init (void)
{
    static const struct {
        const AudguiMenuItem * items;
        int n_items;
    } table[] = {
        {main_items, ARRAY_LEN (main_items)},
        {playback_items, ARRAY_LEN (playback_items)},
        {playlist_items, ARRAY_LEN (playlist_items)},
        {view_items, ARRAY_LEN (view_items)},
        {playlist_add_items, ARRAY_LEN (playlist_add_items)},
        {playlist_remove_items, ARRAY_LEN (playlist_remove_items)},
        {playlist_select_items, ARRAY_LEN (playlist_select_items)},
        {playlist_sort_items, ARRAY_LEN (playlist_sort_items)},
        {playlist_context_items, ARRAY_LEN (playlist_context_items)},
        {eq_preset_items, ARRAY_LEN (eq_preset_items)}
    };

    accel = gtk_accel_group_new ();

    for (int i = UI_MENUS; i --; )
    {
        menus[i] = gtk_menu_new ();
        audgui_menu_init (menus[i], table[i].items, table[i].n_items, accel);
        g_signal_connect (menus[i], "destroy", (GCallback) gtk_widget_destroyed, & menus[i]);
    }
}

void menu_cleanup (void)
{
    for (int i = 0; i < UI_MENUS; i ++)
    {
        if (menus[i])
            gtk_widget_destroy (menus[i]);
    }

    g_object_unref (accel);
    accel = NULL;
}

GtkAccelGroup * menu_get_accel_group (void)
{
    return accel;
}

static void get_monitor_geometry (GdkScreen * screen, gint x, gint y, GdkRectangle * geom)
{
    int monitors = gdk_screen_get_n_monitors (screen);

    for (int i = 0; i < monitors; i ++)
    {
        gdk_screen_get_monitor_geometry (screen, i, geom);

        if (x >= geom->x && x < geom->x + geom->width && y >= geom->y && y < geom->y + geom->height)
            return;
    }

    /* fall back to entire screen */
    geom->x = 0;
    geom->y = 0;
    geom->width = gdk_screen_get_width (screen);
    geom->height = gdk_screen_get_height (screen);
}

typedef struct {
    int x, y;
    bool_t leftward, upward;
} MenuPosition;

static void position_menu (GtkMenu * menu, int * x, int * y, bool_t * push_in, void * data)
{
    const MenuPosition * pos = data;

    GdkRectangle geom;
    get_monitor_geometry (gtk_widget_get_screen ((GtkWidget *) menu), pos->x, pos->y, & geom);

    GtkRequisition request;
    gtk_widget_get_preferred_size ((GtkWidget *) menu, NULL, & request);

    if (pos->leftward)
        * x = MAX (pos->x - request.width, geom.x);
    else
        * x = MIN (pos->x, geom.x + geom.width - request.width);

    if (pos->upward)
        * y = MAX (pos->y - request.height, geom.y);
    else
        * y = MIN (pos->y, geom.y + geom.height - request.height);
}

void menu_popup (int id, int x, int y, bool_t leftward, bool_t upward,
 int button, int time)
{
    const MenuPosition pos = {x, y, leftward, upward};
    gtk_menu_popup ((GtkMenu *) menus[id], NULL, NULL, position_menu, (void *) & pos, button, time);
}
