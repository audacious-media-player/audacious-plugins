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

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "menus.h"
#include "plugin.h"
#include "plugin-window.h"
#include "skins_cfg.h"
#include "equalizer.h"
#include "main.h"
#include "vis-callbacks.h"
#include "playlist.h"
#include "skin.h"
#include "window.h"
#include "view.h"

class SkinnedUI : public IfacePlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Winamp Classic Interface"),
        PACKAGE,
        nullptr,
        & skins_prefs,
        PluginGLibOnly
    };

    constexpr SkinnedUI () : IfacePlugin (info) {}

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
    void plugin_menu_add (AudMenuID id, void func (), const char * name, const char * icon)
        { audgui_plugin_menu_add (id, func, name, icon); }
    void plugin_menu_remove (AudMenuID id, void func ())
        { audgui_plugin_menu_remove (id, func); }

    void startup_notify (const char * id)
        { gdk_notify_startup_complete_with_id (id); }
};

EXPORT SkinnedUI aud_plugin_instance;

static String user_skin_dir;
static String skin_thumb_dir;

const char * skins_get_user_skin_dir ()
{
    if (! user_skin_dir)
        user_skin_dir = String (filename_build ({g_get_user_data_dir (), "audacious", "Skins"}));

    return user_skin_dir;
}

const char * skins_get_skin_thumb_dir ()
{
    if (! skin_thumb_dir)
        skin_thumb_dir = String (filename_build ({g_get_user_cache_dir (), "audacious", "thumbs-unscaled"}));

    return skin_thumb_dir;
}

static bool load_initial_skin ()
{
    String path = aud_get_str ("skins", "skin");
    if (path[0] && skin_load (path))
        return true;

    StringBuf def = filename_build ({aud_get_path (AudPath::DataDir), "Skins", "Default"});
    if (skin_load (def))
        return true;

    AUDERR ("Unable to load any skin; giving up!\n");
    return false;
}

static void skins_init_main (bool restart)
{
    int old_scale = config.scale;

    // The current scaling implementation allows any integer scale factor, but
    // the legacy "double size" config option is limited to two choices.  For
    // now, the two options are floor(DPI/96) and floor(DPI/96)+1.  On screens
    // up to 191 DPI, this gives the traditional 1x and 2x scale factors.
    config.scale = aud::max (1, audgui_get_dpi () / 96) +
     aud_get_bool ("skins", "double_size");

    if (restart && config.scale != old_scale)
        dock_change_scale (old_scale, config.scale);

    mainwin_create ();
    equalizerwin_create ();
    playlistwin_create ();

    view_apply_skin ();
    view_apply_on_top ();
    view_apply_sticky ();

    if (aud_drct_get_playing ())
        mainwin_playback_begin ();
    else
        mainwin_update_song_info ();

    timer_add (TimerRate::Hz4, (TimerFunc) mainwin_update_song_info);
}

bool SkinnedUI::init ()
{
    skins_cfg_load ();

    if (! load_initial_skin ())
        return false;

    audgui_init ();
    menu_init ();
    skins_init_main (false);

    create_plugin_windows ();

    return true;
}

static void skins_cleanup_main ()
{
    mainwin_unhook ();
    equalizerwin_unhook ();
    playlistwin_unhook ();

    timer_remove (TimerRate::Hz4, (TimerFunc) mainwin_update_song_info);

    gtk_widget_destroy (mainwin->gtk ()); mainwin = nullptr;
    gtk_widget_destroy (playlistwin->gtk ()); playlistwin = nullptr;
    gtk_widget_destroy (equalizerwin->gtk ()); equalizerwin = nullptr;
}

void SkinnedUI::cleanup ()
{
    skins_cfg_save ();

    destroy_plugin_windows ();

    skins_cleanup_main ();
    menu_cleanup ();
    audgui_cleanup ();

    skin = Skin ();

    user_skin_dir = String ();
    skin_thumb_dir = String ();
}

void skins_restart ()
{
    skins_cleanup_main ();
    skins_init_main (true);

    if (aud_ui_is_shown ())
        view_show_player (true);
}

void skins_close ()
{
    bool handled = false;
    hook_call ("window close", & handled);

    if (! handled)
        aud_quit ();
}
