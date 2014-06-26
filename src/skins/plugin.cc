/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2008 Tomasz Moń
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include <stdlib.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>

#include "menus.h"
#include "plugin.h"
#include "plugin-window.h"
#include "preset-browser.h"
#include "preset-list.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "view.h"

char * skins_paths[SKINS_PATH_COUNT];

static bool skins_init (void);
static void skins_cleanup (void);

#define AUD_PLUGIN_NAME     N_("Winamp Classic Interface")
#define AUD_PLUGIN_PREFS    & skins_prefs
#define AUD_PLUGIN_INIT     skins_init
#define AUD_PLUGIN_CLEANUP  skins_cleanup

#define AUD_IFACE_SHOW  view_show_player
#define AUD_IFACE_RUN   gtk_main
#define AUD_IFACE_QUIT  gtk_main_quit

#define AUD_IFACE_SHOW_ABOUT         audgui_show_about_window
#define AUD_IFACE_HIDE_ABOUT         audgui_hide_about_window
#define AUD_IFACE_SHOW_FILEBROWSER   audgui_run_filebrowser
#define AUD_IFACE_HIDE_FILEBROWSER   audgui_hide_filebrowser
#define AUD_IFACE_SHOW_JUMP_TO_SONG  audgui_jump_to_track
#define AUD_IFACE_HIDE_JUMP_TO_SONG  audgui_jump_to_track_hide
#define AUD_IFACE_SHOW_SETTINGS      audgui_show_prefs_window
#define AUD_IFACE_HIDE_SETTINGS      audgui_hide_prefs_window
#define AUD_IFACE_MENU_ADD           audgui_plugin_menu_add
#define AUD_IFACE_MENU_REMOVE        audgui_plugin_menu_remove

#define AUD_DECLARE_IFACE
#include <libaudcore/plugin-declare.h>

static int update_source;

static void skins_free_paths(void) {
    int i;

    for (i = 0; i < SKINS_PATH_COUNT; i++)  {
        g_free(skins_paths[i]);
        skins_paths[i] = nullptr;
    }
}

static void skins_init_paths() {
    char *xdg_data_home;
    char *xdg_cache_home;

    xdg_data_home = (getenv("XDG_DATA_HOME") == nullptr
        ? g_build_filename(g_get_home_dir(), ".local", "share", nullptr)
        : g_strdup(getenv("XDG_DATA_HOME")));
    xdg_cache_home = (getenv("XDG_CACHE_HOME") == nullptr
        ? g_build_filename(g_get_home_dir(), ".cache", nullptr)
        : g_strdup(getenv("XDG_CACHE_HOME")));

    skins_paths[SKINS_PATH_USER_SKIN_DIR] =
        g_build_filename(xdg_data_home, "audacious", "Skins", nullptr);
    skins_paths[SKINS_PATH_SKIN_THUMB_DIR] =
        g_build_filename(xdg_cache_home, "audacious", "thumbs", nullptr);

    g_free(xdg_data_home);
    g_free(xdg_cache_home);
}

static gboolean update_cb (void * unused)
{
    mainwin_update_song_info ();
    return TRUE;
}

static bool skins_init (void)
{
    audgui_init ();

    skins_init_paths();
    skins_cfg_load();

    menu_init ();

    init_skins (aud_get_str ("skins", "skin"));

    view_apply_on_top ();
    view_apply_sticky ();

    if (aud_drct_get_playing ())
    {
        ui_main_evlistener_playback_begin (nullptr, nullptr);
        if (aud_drct_get_paused ())
            ui_main_evlistener_playback_pause (nullptr, nullptr);
    }
    else
        mainwin_update_song_info ();

    update_source = g_timeout_add (250, update_cb, nullptr);

    create_plugin_windows ();

    return TRUE;
}

static void skins_cleanup (void)
{
    destroy_plugin_windows ();

    mainwin_unhook ();
    playlistwin_unhook ();
    g_source_remove (update_source);

    skins_cfg_save();

    cleanup_skins();
    skins_free_paths();

    eq_preset_browser_cleanup ();
    eq_preset_list_cleanup ();

    menu_cleanup ();

    audgui_cleanup ();
}

gboolean handle_window_close (void)
{
    gboolean handled = FALSE;
    hook_call ("window close", & handled);

    if (! handled)
        aud_quit ();

    return TRUE;
}
