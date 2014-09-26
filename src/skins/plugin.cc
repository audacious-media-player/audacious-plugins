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
#include <libaudcore/interface.h>
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

class SkinnedUI : public IfacePlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Winamp Classic Interface"),
        PACKAGE,
        nullptr,
        & skins_prefs
    };

    SkinnedUI () : IfacePlugin (info) {}

    bool init ();
    void cleanup ();

    void run ()
        { gtk_main (); }
    void quit ()
        { gtk_main_quit (); }

    void show (bool show)
        { view_show_player (show); }

    void show_about_window ()
        { audgui_show_about_window (); }
    void hide_about_window ()
        { audgui_hide_about_window (); }
    void show_filebrowser (bool open)
        { audgui_run_filebrowser (open); }
    void hide_filebrowser ()
        { audgui_hide_filebrowser (); }
    void show_jump_to_song ()
        { audgui_jump_to_track (); }
    void hide_jump_to_song ()
        { audgui_jump_to_track_hide (); }
    void show_prefs_window ()
        { audgui_show_prefs_window (); }
    void hide_prefs_window ()
        { audgui_hide_prefs_window (); }
    void plugin_menu_add (int id, void func (), const char * name, const char * icon)
        { audgui_plugin_menu_add (id, func, name, icon); }
    void plugin_menu_remove (int id, void func ())
        { audgui_plugin_menu_remove (id, func); }
};

SkinnedUI aud_plugin_instance;

char * skins_paths[SKINS_PATH_COUNT];

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

static void skins_init_main (void)
{
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
}

bool SkinnedUI::init ()
{
    if (aud_get_mainloop_type () != MainloopType::GLib)
        return false;

    audgui_init ();

    skins_cfg_load ();
    skins_init_paths ();

    menu_init ();
    skins_init_main ();

    create_plugin_windows ();

    return true;
}

static void skins_cleanup_main (void)
{
    mainwin_unhook ();
    playlistwin_unhook ();
    g_source_remove (update_source);

    cleanup_skins ();

    eq_preset_browser_cleanup ();
    eq_preset_list_cleanup ();
}

void SkinnedUI::cleanup ()
{
    skins_cfg_save ();

    destroy_plugin_windows ();

    skins_cleanup_main ();
    menu_cleanup ();

    skins_free_paths ();

    audgui_cleanup ();
}

void skins_restart (void)
{
    skins_cleanup_main ();
    skins_init_main ();

    if (aud_ui_is_shown ())
        view_show_player (true);
}

gboolean handle_window_close (void)
{
    gboolean handled = FALSE;
    hook_call ("window close", & handled);

    if (! handled)
        aud_quit ();

    return TRUE;
}
