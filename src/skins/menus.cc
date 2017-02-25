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

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/menu.h>

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "main.h"
#include "view.h"

#include "../ui-common/menu-ops.h"

#define SHIFT GDK_SHIFT_MASK
#define CTRL GDK_CONTROL_MASK
#define ALT GDK_MOD1_MASK

#define SHIFT_CTRL (GdkModifierType) (SHIFT | CTRL)
#define CTRL_ALT   (GdkModifierType) (CTRL | ALT)
#define NO_MOD     (GdkModifierType) 0

#define NO_KEY     0, NO_MOD

static GtkWidget * menus[UI_MENUS];
static GtkAccelGroup * accel;

/* note: playback, playlist, and view menus must be created before main menu */
static GtkWidget * get_menu_playback () { return menus[UI_MENU_PLAYBACK]; }
static GtkWidget * get_menu_playlist () { return menus[UI_MENU_PLAYLIST]; }
static GtkWidget * get_menu_view () { return menus[UI_MENU_VIEW]; }

static GtkWidget * get_plugin_menu_main () { return audgui_get_plugin_menu (AudMenuID::Main); }
static GtkWidget * get_plugin_menu_playlist () { return audgui_get_plugin_menu (AudMenuID::Playlist); }
static GtkWidget * get_plugin_menu_playlist_add () { return audgui_get_plugin_menu (AudMenuID::PlaylistAdd); }
static GtkWidget * get_plugin_menu_playlist_remove () { return audgui_get_plugin_menu (AudMenuID::PlaylistRemove); }

static void configure_effects () { audgui_show_prefs_for_plugin_type (PluginType::Effect); }
static void configure_output () { audgui_show_prefs_for_plugin_type (PluginType::Output); }
static void configure_visualizations () { audgui_show_prefs_for_plugin_type (PluginType::Vis); }

static void skins_volume_up () { mainwin_set_volume_diff (5); }
static void skins_volume_down () { mainwin_set_volume_diff (-5); }

static const AudguiMenuItem output_items[] = {
    MenuCommand (N_("Volume Up"), "audio-volume-high", '+', NO_MOD, skins_volume_up),
    MenuCommand (N_("Volume Down"), "audio-volume-low", '-', NO_MOD, skins_volume_down),
    MenuSep (),
    MenuCommand (N_("Effects ..."), nullptr, NO_KEY, configure_effects),
    MenuSep (),
    MenuToggle (N_("Record Stream"), nullptr, 'd', NO_MOD, nullptr, "record", nullptr, "set record"),
    MenuCommand (N_("Audio Settings ..."), "audio-card", NO_KEY, configure_output)
};

static const AudguiMenuItem main_items[] = {
    MenuCommand (N_("Open Files ..."), "document-open", 'l', NO_MOD, action_play_file),
    MenuCommand (N_("Open URL ..."), "folder-remote", 'l', CTRL, action_play_location),
    MenuCommand (N_("Search Library"), "edit-find", 'y', NO_MOD, action_search_tool),
    MenuSep (),
    MenuSub (N_("Playback"), nullptr, get_menu_playback),
    MenuSub (N_("Playlist"), nullptr, get_menu_playlist),
    MenuSub (N_("Output"), nullptr, {output_items}),
    MenuSub (N_("View"), nullptr, get_menu_view),
    MenuSep (),
    MenuSub (N_("Services"), nullptr, get_plugin_menu_main),
    MenuSep (),
    MenuCommand (N_("About ..."), "help-about", NO_KEY, audgui_show_about_window),
    MenuCommand (N_("Settings ..."), "preferences-system", 'p', CTRL, audgui_show_prefs_window),
    MenuCommand (N_("Quit"), "application-exit", 'q', CTRL, aud_quit)
};

static const AudguiMenuItem playback_items[] = {
    MenuCommand (N_("Song Info ..."), "dialog-information", 'i', NO_MOD, audgui_infowin_show_current),
    MenuSep (),
    MenuToggle (N_("Repeat"), nullptr, 'r', NO_MOD, nullptr, "repeat", nullptr, "set repeat"),
    MenuToggle (N_("Shuffle"), nullptr, 's', NO_MOD, nullptr, "shuffle", nullptr, "set shuffle"),
    MenuToggle (N_("Shuffle by Album"), nullptr, NO_KEY, nullptr, "album_shuffle", nullptr, "set album_shuffle"),
    MenuToggle (N_("No Playlist Advance"), nullptr, 'n', CTRL, nullptr, "no_playlist_advance", nullptr, "set no_playlist_advance"),
    MenuToggle (N_("Stop After This Song"), nullptr, 'm', CTRL, nullptr, "stop_after_current_song", nullptr, "set stop_after_current_song"),
    MenuSep (),
    MenuCommand (N_("Play"), "media-playback-start", 'x', NO_MOD, aud_drct_play),
    MenuCommand (N_("Pause"), "media-playback-pause", 'c', NO_MOD, aud_drct_pause),
    MenuCommand (N_("Stop"), "media-playback-stop", 'v', NO_MOD, aud_drct_stop),
    MenuCommand (N_("Previous"), "media-skip-backward", 'z', NO_MOD, aud_drct_pl_prev),
    MenuCommand (N_("Next"), "media-skip-forward", 'b', NO_MOD, aud_drct_pl_next),
    MenuSep (),
    MenuCommand (N_("Set A-B Repeat"), nullptr, 'a', NO_MOD, action_ab_set),
    MenuCommand (N_("Clear A-B Repeat"), nullptr, 'a', SHIFT, action_ab_clear),
    MenuSep (),
    MenuCommand (N_("Jump to Song ..."), "go-jump", 'j', NO_MOD, audgui_jump_to_track),
    MenuCommand (N_("Jump to Time ..."), "go-jump", 'j', CTRL, audgui_jump_to_time)
};

static const AudguiMenuItem playlist_items[] = {
    MenuCommand (N_("Play/Resume"), "media-playback-start", GDK_KEY_Return, SHIFT, pl_play),
    MenuSep (),
    MenuCommand (N_("New Playlist"), "document-new", 'n', SHIFT, pl_new),
    MenuCommand (N_("Rename Playlist ..."), "insert-text", GDK_KEY_F2, NO_MOD, action_playlist_rename),
    MenuCommand (N_("Remove Playlist"), "edit-delete", 'd', SHIFT, action_playlist_delete),
    MenuSep (),
    MenuCommand (N_("Previous Playlist"), "media-skip-backward", GDK_KEY_Tab, SHIFT, pl_prev),
    MenuCommand (N_("Next Playlist"), "media-skip-forward", GDK_KEY_Tab, NO_MOD, pl_next),
    MenuSep (),
    MenuCommand (N_("Import Playlist ..."), "document-open", 'o', NO_MOD, audgui_import_playlist),
    MenuCommand (N_("Export Playlist ..."), "document-save", 's', SHIFT, audgui_export_playlist),
    MenuSep (),
    MenuCommand (N_("Playlist Manager ..."), "audio-x-generic", 'p', NO_MOD, action_playlist_manager),
    MenuCommand (N_("Queue Manager ..."), nullptr, 'u', CTRL, audgui_queue_manager_show),
    MenuSep (),
    MenuCommand (N_("Refresh Playlist"), "view-refresh", GDK_KEY_F5, NO_MOD, pl_refresh)
};

static const AudguiMenuItem view_items[] = {
    MenuToggle (N_("Show Playlist Editor"), nullptr, 'e', ALT, "skins", "playlist_visible", view_apply_show_playlist, "skins set playlist_visible"),
    MenuToggle (N_("Show Equalizer"), nullptr, 'g', ALT, "skins", "equalizer_visible", view_apply_show_equalizer, "skins set equalizer_visible"),
    MenuSep (),
    MenuToggle (N_("Show Remaining Time"), nullptr, 'r', CTRL, "skins", "show_remaining_time", view_apply_show_remaining, "skins set show_remaining_time"),
    MenuSep (),
    MenuToggle (N_("Double Size"), nullptr, 'd', CTRL, "skins", "double_size", view_apply_double_size, "skins set double_size"),
    MenuToggle (N_("Always on Top"), nullptr, 'o', CTRL, "skins", "always_on_top", view_apply_on_top, "skins set always_on_top"),
    MenuToggle (N_("On All Workspaces"), nullptr, 's', CTRL, "skins", "sticky", view_apply_sticky, "skins set sticky"),
    MenuSep (),
    MenuToggle (N_("Roll Up Player"), nullptr, 'w', CTRL, "skins", "player_shaded", view_apply_player_shaded, "skins set player_shaded"),
    MenuToggle (N_("Roll Up Playlist Editor"), nullptr, 'w', SHIFT_CTRL, "skins", "playlist_shaded", view_apply_playlist_shaded, "skins set playlist_shaded"),
    MenuToggle (N_("Roll Up Equalizer"), nullptr, 'w', CTRL_ALT, "skins", "equalizer_shaded", view_apply_equalizer_shaded, "skins set equalizer_shaded"),
    MenuSep (),
    MenuCommand (N_("_Visualizations ..."), nullptr, NO_KEY, configure_visualizations)
};

static const AudguiMenuItem playlist_add_items[] = {
    MenuSub (N_("Services"), nullptr, get_plugin_menu_playlist_add),
    MenuSep (),
    MenuCommand (N_("Add URL ..."), "folder-remote", 'h', CTRL, action_playlist_add_url),
    MenuCommand (N_("Add Files ..."), "list-add", 'f', NO_MOD, action_playlist_add_files)
};

static const AudguiMenuItem dupe_items[] = {
    MenuCommand (N_("By Title"), nullptr, NO_KEY, rm_dupes_title),
    MenuCommand (N_("By File Name"), nullptr, NO_KEY, rm_dupes_filename),
    MenuCommand (N_("By File Path"), nullptr, NO_KEY, rm_dupes_path)
};

static const AudguiMenuItem playlist_remove_items[] = {
    MenuSub (N_("Services"), nullptr, get_plugin_menu_playlist_remove),
    MenuSep (),
    MenuCommand (N_("Remove All"), "edit-delete", NO_KEY, pl_remove_all),
    MenuCommand (N_("Clear Queue"), "edit-clear", 'q', SHIFT, pl_queue_clear),
    MenuSep (),
    MenuCommand (N_("Remove Unavailable Files"), "dialog-warning", NO_KEY, pl_remove_failed),
    MenuSub (N_("Remove Duplicates"), "edit-copy", {dupe_items}),
    MenuSep (),
    MenuCommand (N_("Remove Unselected"), "list-remove", NO_KEY, pl_remove_unselected),
    MenuCommand (N_("Remove Selected"), "list-remove", GDK_KEY_Delete, NO_MOD, pl_remove_selected)
};

static const AudguiMenuItem playlist_select_items[] = {
    MenuCommand (N_("Search and Select"), "edit-find", 'f', CTRL, action_playlist_search_and_select),
    MenuSep (),
    MenuCommand (N_("Invert Selection"), nullptr, NO_KEY, pl_select_invert),
    MenuCommand (N_("Select None"), nullptr, 'a', SHIFT_CTRL, pl_select_none),
    MenuCommand (N_("Select All"), "edit-select-all", 'a', CTRL, pl_select_all),
};

static const AudguiMenuItem sort_items[] = {
    MenuCommand (N_("By Track Number"), nullptr, NO_KEY, sort_track),
    MenuCommand (N_("By Title"), nullptr, NO_KEY, sort_title),
    MenuCommand (N_("By Artist"), nullptr, NO_KEY, sort_artist),
    MenuCommand (N_("By Album"), nullptr, NO_KEY, sort_album),
    MenuCommand (N_("By Album Artist"), nullptr, NO_KEY, sort_album_artist),
    MenuCommand (N_("By Release Date"), nullptr, NO_KEY, sort_date),
    MenuCommand (N_("By Genre"), nullptr, NO_KEY, sort_genre),
    MenuCommand (N_("By Length"), nullptr, NO_KEY, sort_length),
    MenuCommand (N_("By File Name"), nullptr, NO_KEY, sort_filename),
    MenuCommand (N_("By File Path"), nullptr, NO_KEY, sort_path),
    MenuCommand (N_("By Custom Title"), nullptr, NO_KEY, sort_custom_title),
    MenuCommand (N_("By Comment"), nullptr, NO_KEY, sort_comment)
};

static const AudguiMenuItem sort_selected_items[] = {
    MenuCommand (N_("By Track Number"), nullptr, NO_KEY, sort_sel_track),
    MenuCommand (N_("By Title"), nullptr, NO_KEY, sort_sel_title),
    MenuCommand (N_("By Artist"), nullptr, NO_KEY, sort_sel_artist),
    MenuCommand (N_("By Album"), nullptr, NO_KEY, sort_sel_album),
    MenuCommand (N_("By Album Artist"), nullptr, NO_KEY, sort_sel_album_artist),
    MenuCommand (N_("By Genre"), nullptr, NO_KEY, sort_sel_genre),
    MenuCommand (N_("By Release Date"), nullptr, NO_KEY, sort_sel_date),
    MenuCommand (N_("By Length"), nullptr, NO_KEY, sort_sel_length),
    MenuCommand (N_("By File Name"), nullptr, NO_KEY, sort_sel_filename),
    MenuCommand (N_("By File Path"), nullptr, NO_KEY, sort_sel_path),
    MenuCommand (N_("By Custom Title"), nullptr, NO_KEY, sort_sel_custom_title),
    MenuCommand (N_("By Comment"), nullptr, NO_KEY, sort_comment)
};

static const AudguiMenuItem playlist_sort_items[] = {
    MenuCommand (N_("Randomize List"), nullptr, 'r', SHIFT_CTRL, sort_random),
    MenuCommand (N_("Reverse List"), "view-sort-descending", NO_KEY, sort_reverse),
    MenuSep (),
    MenuSub (N_("Sort Selected"), "view-sort-ascending", {sort_selected_items}),
    MenuSub (N_("Sort List"), "view-sort-ascending", {sort_items})
};

static const AudguiMenuItem playlist_context_items[] = {
    MenuCommand (N_("Song Info ..."), "dialog-information", 'i', ALT, pl_song_info),
    MenuCommand (N_("Open Containing Folder"), "folder", NO_KEY, pl_open_folder),
    MenuSep (),
    MenuCommand (N_("Cut"), "edit-cut", 'x', CTRL, pl_cut),
    MenuCommand (N_("Copy"), "edit-copy", 'c', CTRL, pl_copy),
    MenuCommand (N_("Paste"), "edit-paste", 'v', CTRL, pl_paste),
    MenuCommand (N_("Paste at End"), "edit-paste", 'v', SHIFT_CTRL, pl_paste_end),
    MenuSep (),
    MenuCommand (N_("Queue/Unqueue"), nullptr, 'q', NO_MOD, pl_queue_toggle),
    MenuSep (),
    MenuSub (N_("Services"), nullptr, get_plugin_menu_playlist)
};

void menu_init ()
{
    static const ArrayRef<AudguiMenuItem> table[] = {
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

    accel = gtk_accel_group_new ();

    for (int i = UI_MENUS; i --; )
    {
        menus[i] = gtk_menu_new ();
        audgui_menu_init (menus[i], table[i], accel);
        g_signal_connect (menus[i], "destroy", (GCallback) gtk_widget_destroyed, & menus[i]);
    }
}

void menu_cleanup ()
{
    for (int i = 0; i < UI_MENUS; i ++)
    {
        if (menus[i])
            gtk_widget_destroy (menus[i]);
    }

    g_object_unref (accel);
    accel = nullptr;
}

GtkAccelGroup * menu_get_accel_group ()
{
    return accel;
}

typedef struct {
    int x, y;
    gboolean leftward, upward;
} MenuPosition;

static void position_menu (GtkMenu * menu, int * x, int * y, gboolean * push_in, void * data)
{
    const MenuPosition * pos = (MenuPosition *) data;

    GdkRectangle geom;
    audgui_get_monitor_geometry (gtk_widget_get_screen ((GtkWidget *) menu), pos->x, pos->y, & geom);

    GtkRequisition request;
    gtk_widget_size_request ((GtkWidget *) menu, & request);

    if (pos->leftward)
        * x = aud::max (pos->x - request.width, geom.x);
    else
        * x = aud::min (pos->x, geom.x + geom.width - request.width);

    if (pos->upward)
        * y = aud::max (pos->y - request.height, geom.y);
    else
        * y = aud::min (pos->y, geom.y + geom.height - request.height);
}

void menu_popup (int id, int x, int y, gboolean leftward, gboolean upward,
 int button, int time)
{
    const MenuPosition pos = {x, y, leftward, upward};
    gtk_menu_popup ((GtkMenu *) menus[id], nullptr, nullptr, position_menu, (void *) & pos, button, time);
}
