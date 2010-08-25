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

#include "config.h"

#include "ui_manager.h"
#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "ui_playlist_notebook.h"

#include <audacious/misc.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static GtkUIManager *ui_manager = NULL;

/* toggle action entries */

static GtkToggleActionEntry toggleaction_entries_others[] = {
    {"stop after current song", NULL, N_("Stop after Current Song"), "<Ctrl>M",
     N_("Stop after Current Song"), G_CALLBACK(action_stop_after_current_song), FALSE},

    {"playback repeat", NULL, N_("Repeat"), "R",
     N_("Repeat"), G_CALLBACK(action_playback_repeat), FALSE},

    {"playback shuffle", NULL, N_("Shuffle"), "S",
     N_("Shuffle"), G_CALLBACK(action_playback_shuffle), FALSE},

    {"playback no playlist advance", NULL, N_("No Playlist Advance"), "<Ctrl>N",
     N_("No Playlist Advance"), G_CALLBACK(action_playback_noplaylistadvance), FALSE},

    {"view playlists", NULL, N_("Show playlists"), "<Shift><Ctrl>P",
     N_("Show/hide playlists"), G_CALLBACK(action_view_playlist), FALSE},

    {"view infoarea", NULL, N_("Show infoarea"), "<Shift><Ctrl>I",
     N_("Show/hide infoarea"), G_CALLBACK(action_view_infoarea), FALSE},

    {"view menu", NULL, N_("Show main menu"), "<Shift><Ctrl>M",
     N_("Show/hide main menu"), G_CALLBACK(action_view_menu), FALSE},

    {"view statusbar", NULL, N_("Show statusbar"), "<Shift><Ctrl>S",
     N_("Show/hide statusbar"), G_CALLBACK(action_view_statusbar), FALSE},
};

/* normal actions */

static GtkActionEntry action_entries_playback[] = {

    {"playback", NULL, N_("Playback")},

    {"playback play", GTK_STOCK_MEDIA_PLAY, N_("Play"), "X",
     N_("Play"), G_CALLBACK(action_playback_play)},

    {"playback pause", GTK_STOCK_MEDIA_PAUSE, N_("Pause"), "C",
     N_("Pause"), G_CALLBACK(action_playback_pause)},

    {"playback stop", GTK_STOCK_MEDIA_STOP, N_("Stop"), "V",
     N_("Stop"), G_CALLBACK(action_playback_stop)},

    {"playback previous", GTK_STOCK_MEDIA_PREVIOUS, N_("Previous"), "Z",
     N_("Previous"), G_CALLBACK(action_playback_previous)},

    {"playback next", GTK_STOCK_MEDIA_NEXT, N_("Next"), "B",
     N_("Next"), G_CALLBACK(action_playback_next)}
};

static GtkActionEntry action_entries_playlist[] = {

    {"playlist", NULL, N_("Playlist")},

    {"playlist new", GTK_STOCK_NEW, N_("New Playlist"), "<Shift>N",
     N_("New Playlist"), G_CALLBACK(action_playlist_new)},

    {"playlist delete", GTK_STOCK_DELETE, N_("Delete Playlist"), "<Shift>D",
     N_("Delete Playlist"), G_CALLBACK(action_playlist_delete)},

    {"playlist load", GTK_STOCK_OPEN, N_("Import Playlist ..."), "O",
     N_("Loads a playlist file into the selected playlist."), G_CALLBACK(action_playlist_load_list)},

    {"playlist save", GTK_STOCK_SAVE, N_("Export Playlist ..."), "<Shift>S",
     N_("Saves the selected playlist."), G_CALLBACK(action_playlist_save_list)},

    {"playlist save all", GTK_STOCK_SAVE, N_("Save All Playlists"), "<Alt>S",
     N_("Saves all the playlists that are open. Note that this "
        "is done automatically when Audacious quits."),
     G_CALLBACK(action_playlist_save_all_playlists)},

    {"playlist refresh", GTK_STOCK_REFRESH, N_("Refresh"), "F5",
     N_("Refreshes metadata associated with a playlist entry."),
     G_CALLBACK(action_playlist_refresh_list)},

    {"playlist manager", AUD_STOCK_PLAYLIST, N_("Playlist Manager"), "P",
     N_("Opens the playlist manager."),
     G_CALLBACK(action_open_list_manager)},

    {"playlist add url", GTK_STOCK_NETWORK, N_("Add URL ..."), "<Ctrl>H",
     N_("Adds a remote track to the playlist."),
     G_CALLBACK(action_playlist_add_url)},

    {"playlist add files", GTK_STOCK_ADD, N_("Add Files ..."), "F",
     N_("Adds files to the playlist."),
     G_CALLBACK(action_playlist_add_files)},

    {"playlist remove all", GTK_STOCK_CLEAR, N_("Remove All"), NULL,
     N_("Removes all entries from the playlist."),
     G_CALLBACK(action_playlist_remove_all)},

    {"playlist remove unselected", GTK_STOCK_REMOVE, N_("Remove Unselected"), NULL,
     N_("Remove unselected entries from the playlist."),
     G_CALLBACK(action_playlist_remove_unselected)},

    {"playlist remove selected", GTK_STOCK_REMOVE, N_("Remove Selected"), "Delete",
     N_("Remove selected entries from the playlist."),
     G_CALLBACK(action_playlist_remove_selected)},

    {"playlist title edit", GTK_STOCK_EDIT, N_("Edit title"), "F2",
     N_("Edites the playlist title."),
     G_CALLBACK(ui_playlist_notebook_edit_tab_title)},

 {"playlist sort", GTK_STOCK_SORT_ASCENDING, N_("Sort"), NULL, NULL, NULL},
 {"playlist sort track", NULL, N_("By Track Number"), NULL, NULL, (GCallback)
  playlist_sort_track},
 {"playlist sort title", NULL, N_("By Title"), NULL, NULL, (GCallback)
  playlist_sort_title},
 {"playlist sort artist", NULL, N_("By Artist"), NULL, NULL, (GCallback)
  playlist_sort_artist},
 {"playlist sort album", NULL, N_("By Album"), NULL, NULL, (GCallback)
  playlist_sort_album},
 {"playlist sort path", NULL, N_("By File Path"), NULL, NULL, (GCallback)
  playlist_sort_path},
 {"playlist reverse", GTK_STOCK_SORT_DESCENDING, N_("Reverse Order"), NULL,
  NULL, (GCallback) playlist_reverse}};

static GtkActionEntry action_entries_output[] =
{
    {"output", NULL, N_("Output")},

    {"effects menu", NULL, N_("Effects")},

    {"equalizer show", NULL, N_("Equalizer"), "<Ctrl>E", NULL, (GCallback)
     audgui_show_equalizer_window},
};

static GtkActionEntry action_entries_view[] = {
 {"view", NULL, N_("View")},
 {"iface menu", NULL, N_("Interface")}};

static GtkActionEntry action_entries_others[] = {

    {"dummy", NULL, "dummy"},

    /* XXX Carbon support */
    {"file", NULL, N_("File")},
    {"help", NULL, N_("Help")},

    {"plugins-menu", NULL, N_("Components")},

    {"current track info", GTK_STOCK_INFO, N_("View Track Details"), "I",
     N_("View track details"), G_CALLBACK(action_current_track_info)},

    {"playlist track info", GTK_STOCK_INFO, N_("View Track Details"), "<Alt>I",
     N_("View track details"), G_CALLBACK(action_playlist_track_info)},

    {"about audacious", GTK_STOCK_DIALOG_INFO, N_("About Audacious"), NULL,
     N_("About Audacious"), G_CALLBACK(action_about_audacious)},

    {"play file", GTK_STOCK_OPEN, N_("Open Files ..."), "L",
     N_("Load and play a file"), G_CALLBACK(action_play_file)},

    {"play location", GTK_STOCK_NETWORK, N_("Open URL ..."), "<Ctrl>L",
     N_("Play media from the selected location"), G_CALLBACK(action_play_location)},

    {"plugins", NULL, N_("Plugin services")},

    {"preferences", GTK_STOCK_PREFERENCES, N_("Preferences"), "<Ctrl>P",
     N_("Open preferences window"), G_CALLBACK(action_preferences)},

    {"quit", GTK_STOCK_QUIT, N_("_Quit"), NULL,
     N_("Quit Audacious"), G_CALLBACK(action_quit)},

    {"ab set", NULL, N_("Set A-B"), "A",
     N_("Set A-B"), G_CALLBACK(action_ab_set)},

    {"ab clear", NULL, N_("Clear A-B"), "<Shift>A",
     N_("Clear A-B"), G_CALLBACK(action_ab_clear)},

    {"jump to playlist start", GTK_STOCK_GOTO_TOP, N_("Jump to Playlist Start"), "<Ctrl>Z",
     N_("Jump to Playlist Start"), G_CALLBACK(action_jump_to_playlist_start)},

    {"jump to file", GTK_STOCK_JUMP_TO, N_("Jump to File"), "J",
     N_("Jump to File"), G_CALLBACK(action_jump_to_file)},

    {"jump to time", GTK_STOCK_JUMP_TO, N_("Jump to Time"), "<Ctrl>J",
     N_("Jump to Time"), G_CALLBACK(action_jump_to_time)},

    {"queue toggle", AUD_STOCK_QUEUETOGGLE, N_("Queue Toggle"), "Q",
     N_("Enables/disables the entry in the playlist's queue."),
     G_CALLBACK(action_queue_toggle)},

    {"playlist copy", GTK_STOCK_COPY, N_("Copy"), "<Ctrl>C",
     NULL, G_CALLBACK(action_playlist_copy)},

    {"playlist cut", GTK_STOCK_CUT, N_("Cut"), "<Ctrl>X",
     NULL, G_CALLBACK(action_playlist_cut)},

    {"playlist paste", GTK_STOCK_PASTE, N_("Paste"), "<Ctrl>V",
     NULL, G_CALLBACK(action_playlist_paste)},

    {"playlist select all", NULL, N_("Select All"), "<Ctrl>A",
     N_("Selects all of the playlist entries."),
     G_CALLBACK(action_playlist_select_all)},

    {"playlist select none", NULL, N_("Select None"), "<Shift><Ctrl>A",
     N_("Deselects all of the playlist entries."),
     G_CALLBACK(action_playlist_select_none)},
};

static GtkActionGroup * action_group_playback;
static GtkActionGroup * action_group_output;
static GtkActionGroup * action_group_view;
static GtkActionGroup * action_group_others;
static GtkActionGroup * action_group_playlist;

/* ***************************** */

GtkWidget *ui_manager_get_menus(void)
{
    GtkWidget *menu;

    menu = gtk_ui_manager_get_widget(ui_manager, "/mainwin-menus");

    return menu;
}

static GtkActionGroup *ui_manager_new_action_group(const gchar * group_name)
{
    GtkActionGroup *group = gtk_action_group_new(group_name);
    gtk_action_group_set_translation_domain(group, PACKAGE_NAME);
    return group;
}

void ui_manager_init(void)
{
    /* toggle actions */
    toggleaction_group_others = ui_manager_new_action_group("toggleaction_others");
    gtk_action_group_add_toggle_actions(toggleaction_group_others, toggleaction_entries_others, G_N_ELEMENTS(toggleaction_entries_others), NULL);

    /* normal actions */
    action_group_playback = ui_manager_new_action_group("action_playback");
    gtk_action_group_add_actions(action_group_playback, action_entries_playback, G_N_ELEMENTS(action_entries_playback), NULL);

    action_group_playlist = ui_manager_new_action_group("action_playlist");
    gtk_action_group_add_actions(action_group_playlist, action_entries_playlist, G_N_ELEMENTS(action_entries_playlist), NULL);

    action_group_output = ui_manager_new_action_group ("action_output");
    gtk_action_group_add_actions (action_group_output, action_entries_output,
     G_N_ELEMENTS (action_entries_output), NULL);

    action_group_view = ui_manager_new_action_group("action_view");
    gtk_action_group_add_actions(action_group_view, action_entries_view, G_N_ELEMENTS(action_entries_view), NULL);

    action_group_others = ui_manager_new_action_group("action_others");
    gtk_action_group_add_actions(action_group_others, action_entries_others, G_N_ELEMENTS(action_entries_others), NULL);

    /* ui */
    ui_manager = gtk_ui_manager_new();
    gtk_ui_manager_insert_action_group(ui_manager, toggleaction_group_others, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playback, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playlist, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_output, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_view, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_others, 0);
}

#ifdef GDK_WINDOWING_QUARTZ
static GtkWidget *carbon_menubar;
#endif


void ui_manager_create_menus(void)
{
    GError *gerr = NULL;

    /* attach xml menu definitions */
    gtk_ui_manager_add_ui_from_file(ui_manager, DATA_DIR "/ui/player.ui", &gerr);

    if (gerr != NULL)
    {
        g_critical("Error loading player.ui: %s", gerr->message);
        g_error_free(gerr);
        return;
    }

    gtk_menu_item_set_submenu ((GtkMenuItem *) gtk_ui_manager_get_widget
     (ui_manager, "/mainwin-menus/plugins-menu"), aud_get_plugin_menu
     (AUDACIOUS_MENU_MAIN));
    gtk_menu_item_set_submenu ((GtkMenuItem *) gtk_ui_manager_get_widget
     (ui_manager, "/mainwin-menus/output/effects menu"),
     audgui_create_effects_menu ());
    gtk_menu_item_set_submenu ((GtkMenuItem *) gtk_ui_manager_get_widget
     (ui_manager, "/mainwin-menus/view/iface menu"),
     audgui_create_iface_menu ());

    playlistwin_popup_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/playlist-rightclick-menu");

    gtk_menu_item_set_submenu ((GtkMenuItem *) gtk_ui_manager_get_widget
     (ui_manager, "/playlist-menus/playlist-rightclick-menu/plugins-menu"),
     aud_get_plugin_menu (AUDACIOUS_MENU_PLAYLIST_RCLICK));

    playlist_tab_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/playlist-tab-menu");
}


GtkAccelGroup *ui_manager_get_accel_group(void)
{
    return gtk_ui_manager_get_accel_group(ui_manager);
}


GtkWidget *ui_manager_get_popup_menu(GtkUIManager * self, const gchar * path)
{
    GtkWidget *menu_item = gtk_ui_manager_get_widget(self, path);

    if (GTK_IS_MENU_ITEM(menu_item))
        return gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));
    else
        return NULL;
}


static void menu_popup_pos_func(GtkMenu * menu, gint * x, gint * y, gboolean * push_in, gint * point)
{
    GtkRequisition requisition;
    gint screen_width;
    gint screen_height;

    gtk_widget_size_request(GTK_WIDGET(menu), &requisition);

    screen_width = gdk_screen_width();
    screen_height = gdk_screen_height();

    *x = CLAMP(point[0] - 2, 0, MAX(0, screen_width - requisition.width));
    *y = CLAMP(point[1] - 2, 0, MAX(0, screen_height - requisition.height));

    *push_in = FALSE;
}


void ui_manager_popup_menu_show(GtkMenu * menu, gint x, gint y, guint button, guint time)
{
    gint pos[2];
    pos[0] = x;
    pos[1] = y;

    gtk_menu_popup(menu, NULL, NULL, (GtkMenuPositionFunc) menu_popup_pos_func, pos, button, time);
}

void ui_manager_destroy(void)
{
    gtk_menu_detach ((GtkMenu *) aud_get_plugin_menu (AUDACIOUS_MENU_MAIN));
    gtk_menu_detach ((GtkMenu *) aud_get_plugin_menu
     (AUDACIOUS_MENU_PLAYLIST_RCLICK));

    g_object_unref((GObject *) toggleaction_group_others);
    g_object_unref((GObject *) action_group_playback);
    g_object_unref((GObject *) action_group_playlist);
    g_object_unref((GObject *) action_group_output);
    g_object_unref((GObject *) action_group_view);
    g_object_unref((GObject *) action_group_others);
    g_object_unref((GObject *) ui_manager);
}
