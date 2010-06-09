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

#if 0
/* this header contains prototypes for plugin-available menu functions */
#include "ui_plugin_menu.h"
#endif

#include <audacious/plugin.h>
#include <audacious/ui_plugin_menu.h>
#include <libaudgui/libaudgui.h>

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


static GtkActionEntry action_entries_visualization[] = {
    {"visualization", NULL, N_("Visualization")},
    {"vismode", NULL, N_("Visualization Mode")},
    {"anamode", NULL, N_("Analyzer Mode")},
    {"scomode", NULL, N_("Scope Mode")},
    {"vprmode", NULL, N_("Voiceprint Mode")},
    {"wshmode", NULL, N_("WindowShade VU Mode")},
    {"anafoff", NULL, N_("Analyzer Falloff")},
    {"peafoff", NULL, N_("Peaks Falloff")}
};

static GtkActionEntry action_entries_playlist[] = {

    {"playlist", NULL, N_("Playlist")},

    {"playlist new", GTK_STOCK_NEW, N_("New Playlist"), "<Shift>N",
     N_("New Playlist"), G_CALLBACK(action_playlist_new)},

    {"playlist select next", GTK_STOCK_MEDIA_NEXT, N_("Select Next Playlist"), "<Shift>P",
     N_("Select Next Playlist"), G_CALLBACK(action_playlist_next)},

    {"playlist select previous", GTK_STOCK_MEDIA_PREVIOUS, N_("Select Previous Playlist"), "<Shift><Ctrl>P",
     N_("Select Previous Playlist"), G_CALLBACK(action_playlist_prev)},

    {"playlist delete", GTK_STOCK_DELETE, N_("Delete Playlist"), "<Shift>D",
     N_("Delete Playlist"), G_CALLBACK(action_playlist_delete)},

    {"playlist load", GTK_STOCK_OPEN, N_("Load List"), "O",
     N_("Loads a playlist file into the selected playlist."), G_CALLBACK(action_playlist_load_list)},

    {"playlist save", GTK_STOCK_SAVE, N_("Save List"), "<Shift>S",
     N_("Saves the selected playlist."), G_CALLBACK(action_playlist_save_list)},

    {"playlist save all", GTK_STOCK_SAVE, N_("Save All Playlists"), "<Alt>S",
     N_("Saves all the playlists that are open. Note that this "
        "is done automatically when Audacious quits."),
     G_CALLBACK(action_playlist_save_all_playlists)},

    {"playlist refresh", GTK_STOCK_REFRESH, N_("Refresh List"), "F5",
     N_("Refreshes metadata associated with a playlist entry."),
     G_CALLBACK(action_playlist_refresh_list)},

    {"playlist manager", AUD_STOCK_PLAYLIST, N_("List Manager"), "P",
     N_("Opens the playlist manager."),
     G_CALLBACK(action_open_list_manager)}
};

static GtkActionEntry action_entries_view[] = {

    {"view", NULL, N_("View")}
};

static GtkActionEntry action_entries_playlist_add[] = {
    {"playlist add url", GTK_STOCK_NETWORK, N_("Add Internet Address..."), "<Ctrl>H",
     N_("Adds a remote track to the playlist."),
     G_CALLBACK(action_playlist_add_url)},

    {"playlist add files", GTK_STOCK_ADD, N_("Add Files..."), "F",
     N_("Adds files to the playlist."),
     G_CALLBACK(action_playlist_add_files)},
};

static GtkActionEntry action_entries_playlist_select[] = {
    {"playlist search and select", GTK_STOCK_FIND, N_("Search and Select"), "<Ctrl>F",
     N_("Searches the playlist and selects playlist entries based on specific criteria."),
     G_CALLBACK(action_playlist_search_and_select)},

    {"playlist invert selection", NULL, N_("Invert Selection"), NULL,
     N_("Inverts the selected and unselected entries."),
     G_CALLBACK(action_playlist_invert_selection)},

    {"playlist select all", NULL, N_("Select All"), "<Ctrl>A",
     N_("Selects all of the playlist entries."),
     G_CALLBACK(action_playlist_select_all)},

    {"playlist select none", NULL, N_("Select None"), "<Shift><Ctrl>A",
     N_("Deselects all of the playlist entries."),
     G_CALLBACK(action_playlist_select_none)},
};

static GtkActionEntry action_entries_playlist_delete[] = {
    {"playlist remove all", GTK_STOCK_CLEAR, N_("Remove All"), NULL,
     N_("Removes all entries from the playlist."),
     G_CALLBACK(action_playlist_remove_all)},

    {"playlist clear queue", GTK_STOCK_CLEAR, N_("Clear Queue"), "<Shift>Q",
     N_("Clears the queue associated with this playlist."),
     G_CALLBACK(action_playlist_clear_queue)},

    {"playlist remove unavailable", GTK_STOCK_DIALOG_ERROR, N_("Remove Unavailable Files"), NULL,
     N_("Removes unavailable files from the playlist."),
     G_CALLBACK(action_playlist_remove_unavailable)},

    {"playlist remove dups menu", NULL, N_("Remove Duplicates")},

    {"playlist remove dups by title", NULL, N_("By Title"), NULL,
     N_("Removes duplicate entries from the playlist by title."),
     G_CALLBACK(action_playlist_remove_dupes_by_title)},

    {"playlist remove dups by filename", NULL, N_("By Filename"), NULL,
     N_("Removes duplicate entries from the playlist by filename."),
     G_CALLBACK(action_playlist_remove_dupes_by_filename)},

    {"playlist remove dups by full path", NULL, N_("By Path + Filename"), NULL,
     N_("Removes duplicate entries from the playlist by their full path."),
     G_CALLBACK(action_playlist_remove_dupes_by_full_path)},

    {"playlist remove unselected", GTK_STOCK_REMOVE, N_("Remove Unselected"), NULL,
     N_("Remove unselected entries from the playlist."),
     G_CALLBACK(action_playlist_remove_unselected)},

    {"playlist remove selected", GTK_STOCK_REMOVE, N_("Remove Selected"), "Delete",
     N_("Remove selected entries from the playlist."),
     G_CALLBACK(action_playlist_remove_selected)},
};

static GtkActionEntry action_entries_playlist_sort[] = {
    {"playlist randomize list", AUD_STOCK_RANDOMIZEPL, N_("Randomize List"), "<Ctrl><Shift>R",
     N_("Randomizes the playlist."),
     G_CALLBACK(action_playlist_randomize_list)},

    {"playlist reverse list", GTK_STOCK_GO_UP, N_("Reverse List"), NULL,
     N_("Reverses the playlist."),
     G_CALLBACK(action_playlist_reverse_list)},

    {"playlist sort menu", GTK_STOCK_GO_DOWN, N_("Sort List")},

    {"playlist sort by title", NULL, N_("By Title"), NULL,
     N_("Sorts the list by title."),
     G_CALLBACK(action_playlist_sort_by_title)},

    {"playlist sort by album", NULL, N_("By Album"), NULL,
     N_("Sorts the list by album."),
     G_CALLBACK(action_playlist_sort_by_album)},

    {"playlist sort by artist", NULL, N_("By Artist"), NULL,
     N_("Sorts the list by artist."),
     G_CALLBACK(action_playlist_sort_by_artist)},

    {"playlist sort by filename", NULL, N_("By Filename"), NULL,
     N_("Sorts the list by filename."),
     G_CALLBACK(action_playlist_sort_by_filename)},

    {"playlist sort by full path", NULL, N_("By Path + Filename"), NULL,
     N_("Sorts the list by full pathname."),
     G_CALLBACK(action_playlist_sort_by_full_path)},

    {"playlist sort by date", NULL, N_("By Date"), NULL,
     N_("Sorts the list by modification time."),
     G_CALLBACK(action_playlist_sort_by_date)},

    {"playlist sort by track number", NULL, N_("By Track Number"), NULL,
     N_("Sorts the list by track number."),
     G_CALLBACK(action_playlist_sort_by_track_number)},

    {"playlist sort selected menu", GTK_STOCK_GO_DOWN, N_("Sort Selected")},

    {"playlist sort selected by title", NULL, N_("By Title"), NULL,
     N_("Sorts the list by title."),
     G_CALLBACK(action_playlist_sort_selected_by_title)},

    {"playlist sort selected by album", NULL, N_("By Album"), NULL,
     N_("Sorts the list by album."),
     G_CALLBACK(action_playlist_sort_selected_by_album)},

    {"playlist sort selected by artist", NULL, N_("By Artist"), NULL,
     N_("Sorts the list by artist."),
     G_CALLBACK(action_playlist_sort_selected_by_artist)},

    {"playlist sort selected by filename", NULL, N_("By Filename"), NULL,
     N_("Sorts the list by filename."),
     G_CALLBACK(action_playlist_sort_selected_by_filename)},

    {"playlist sort selected by full path", NULL, N_("By Path + Filename"), NULL,
     N_("Sorts the list by full pathname."),
     G_CALLBACK(action_playlist_sort_selected_by_full_path)},

    {"playlist sort selected by date", NULL, N_("By Date"), NULL,
     N_("Sorts the list by modification time."),
     G_CALLBACK(action_playlist_sort_selected_by_date)},

    {"playlist sort selected by track number", NULL, N_("By Track Number"), NULL,
     N_("Sorts the list by track number."),
     G_CALLBACK(action_playlist_sort_selected_by_track_number)},
};

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

    {"play file", GTK_STOCK_OPEN, N_("Play File"), "L",
     N_("Load and play a file"), G_CALLBACK(action_play_file)},

    {"play location", GTK_STOCK_NETWORK, N_("Play Location"), "<Ctrl>L",
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
};


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

    action_group_visualization = ui_manager_new_action_group("action_visualization");
    gtk_action_group_add_actions(action_group_visualization, action_entries_visualization, G_N_ELEMENTS(action_entries_visualization), NULL);

    action_group_view = ui_manager_new_action_group("action_view");
    gtk_action_group_add_actions(action_group_view, action_entries_view, G_N_ELEMENTS(action_entries_view), NULL);

    action_group_others = ui_manager_new_action_group("action_others");
    gtk_action_group_add_actions(action_group_others, action_entries_others, G_N_ELEMENTS(action_entries_others), NULL);

    action_group_playlist_add = ui_manager_new_action_group("action_playlist_add");
    gtk_action_group_add_actions(action_group_playlist_add, action_entries_playlist_add, G_N_ELEMENTS(action_entries_playlist_add), NULL);

    action_group_playlist_select = ui_manager_new_action_group("action_playlist_select");
    gtk_action_group_add_actions(action_group_playlist_select, action_entries_playlist_select, G_N_ELEMENTS(action_entries_playlist_select), NULL);

    action_group_playlist_delete = ui_manager_new_action_group("action_playlist_delete");
    gtk_action_group_add_actions(action_group_playlist_delete, action_entries_playlist_delete, G_N_ELEMENTS(action_entries_playlist_delete), NULL);

    action_group_playlist_sort = ui_manager_new_action_group("action_playlist_sort");
    gtk_action_group_add_actions(action_group_playlist_sort, action_entries_playlist_sort, G_N_ELEMENTS(action_entries_playlist_sort), NULL);

    /* ui */
    ui_manager = gtk_ui_manager_new();
    gtk_ui_manager_insert_action_group(ui_manager, toggleaction_group_others, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playback, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playlist, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_visualization, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_view, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_others, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playlist_add, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playlist_select, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playlist_delete, 0);
    gtk_ui_manager_insert_action_group(ui_manager, action_group_playlist_sort, 0);

    return;
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
        g_critical("Error creating UI<ui/mainwin.ui>: %s", gerr->message);
        g_error_free(gerr);
        return;
    }

    /* create GtkMenu widgets using path from xml definitions */
    mainwin_songname_menu = ui_manager_get_popup_menu(ui_manager, "/mainwin-menus/songname-menu");
    mainwin_visualization_menu = ui_manager_get_popup_menu(ui_manager, "/mainwin-menus/main-menu/visualization");
    mainwin_playback_menu = ui_manager_get_popup_menu(ui_manager, "/mainwin-menus/main-menu/playback");
    mainwin_playlist_menu = ui_manager_get_popup_menu(ui_manager, "/mainwin-menus/main-menu/playlist");
    mainwin_view_menu = ui_manager_get_popup_menu(ui_manager, "/mainwin-menus/main-menu/view");
    mainwin_general_menu = ui_manager_get_popup_menu(ui_manager, "/mainwin-menus/main-menu");

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/mainwin-menus/plugins-menu")), aud_get_plugin_menu(AUDACIOUS_MENU_MAIN));

#ifdef GDK_WINDOWING_QUARTZ
    gtk_ui_manager_add_ui_from_file(ui_manager, DATA_DIR "/ui/carbon-menubar.ui", &gerr);

    if (gerr != NULL)
    {
        g_critical("Error creating UI<ui/carbon-menubar.ui>: %s", gerr->message);
        g_error_free(gerr);
        return;
    }

    carbon_menubar = ui_manager_get_popup_menu(ui_manager, "/carbon-menubar/main-menu");
    sync_menu_takeover_menu(GTK_MENU_SHELL(carbon_menubar));
#endif

    gtk_ui_manager_add_ui_from_file(ui_manager, DATA_DIR "/ui/playlist.ui", &gerr);

    if (gerr != NULL)
    {
        g_critical("Error creating UI<ui/playlist.ui>: %s", gerr->message);
        g_error_free(gerr);
        return;
    }

    playlistwin_popup_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/playlist-rightclick-menu");

    playlistwin_pladd_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/add-menu");
    playlistwin_pldel_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/del-menu");
    playlistwin_plsel_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/select-menu");
    playlistwin_plsort_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/misc-menu");
    playlistwin_pllist_menu = ui_manager_get_popup_menu(ui_manager, "/playlist-menus/playlist-menu");

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/playlist-menus/playlist-menu/plugins-menu")), aud_get_plugin_menu(AUDACIOUS_MENU_PLAYLIST));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/playlist-menus/playlist-rightclick-menu/plugins-menu")), aud_get_plugin_menu(AUDACIOUS_MENU_PLAYLIST_RCLICK));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/playlist-menus/add-menu/plugins-menu")), aud_get_plugin_menu(AUDACIOUS_MENU_PLAYLIST_ADD));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/playlist-menus/del-menu/plugins-menu")), aud_get_plugin_menu(AUDACIOUS_MENU_PLAYLIST_REMOVE));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/playlist-menus/select-menu/plugins-menu")), aud_get_plugin_menu(AUDACIOUS_MENU_PLAYLIST_SELECT));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_ui_manager_get_widget(ui_manager, "/playlist-menus/misc-menu/plugins-menu")), aud_get_plugin_menu(AUDACIOUS_MENU_PLAYLIST_MISC));

    return;
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
    g_object_unref((GObject *) toggleaction_group_others);
    g_object_unref((GObject *) action_group_playback);
    g_object_unref((GObject *) action_group_playlist);
    g_object_unref((GObject *) action_group_visualization);
    g_object_unref((GObject *) action_group_view);
    g_object_unref((GObject *) action_group_others);
    g_object_unref((GObject *) action_group_playlist_add);
    g_object_unref((GObject *) action_group_playlist_select);
    g_object_unref((GObject *) action_group_playlist_delete);
    g_object_unref((GObject *) action_group_playlist_sort);
    g_object_unref((GObject *) ui_manager);
}
