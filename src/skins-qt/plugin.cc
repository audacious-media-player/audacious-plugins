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
#include <glib.h>
#include <QApplication>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>
#include <libaudqt/iface.h>
#include <libaudqt/libaudqt.h>

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

class QtSkins : public audqt::QtIfacePlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Winamp Classic Interface"),
        PACKAGE,
        nullptr,
        & skins_prefs,
        PluginQtOnly
    };

    constexpr QtSkins () : audqt::QtIfacePlugin (info) {}

    bool init ();
    void cleanup ();

    void run () { audqt::run (); }
    void quit () { audqt::quit (); }

    void show (bool show)
        { view_show_player (show); }
};

EXPORT QtSkins aud_plugin_instance;

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

    config.scale = aud_get_bool ("skins", "double_size") ? 2 : 1;

    if (restart && config.scale != old_scale)
        dock_change_scale (old_scale, config.scale);

    mainwin_create ();
    equalizerwin_create ();
    playlistwin_create ();

    menu_init (mainwin);

    /* copy menu shortcuts to the two other windows */
    for (QAction * action : mainwin->actions ())
    {
        equalizerwin->addAction (action);
        playlistwin->addAction (action);
    }

    view_apply_skin ();
    view_apply_on_top ();
    view_apply_sticky ();

    if (aud_drct_get_playing ())
        mainwin_playback_begin ();
    else
        mainwin_update_song_info ();

    timer_add (TimerRate::Hz4, (TimerFunc) mainwin_update_song_info);
}

bool QtSkins::init ()
{
    skins_cfg_load ();

    if (! load_initial_skin ())
        return false;

    audqt::init ();
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

    delete mainwin; mainwin = nullptr;
    delete playlistwin; playlistwin = nullptr;
    delete equalizerwin; equalizerwin = nullptr;
}

void QtSkins::cleanup ()
{
    skins_cfg_save ();

    destroy_plugin_windows ();

    skins_cleanup_main ();
    audqt::cleanup ();

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
