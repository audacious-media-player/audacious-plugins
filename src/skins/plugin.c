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


#include "plugin.h"
#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_manager.h"
#include "icons-stock.h"
#include "ui_main_evlisteners.h"
#include "ui_playlist_evlisteners.h"
#include <audacious/i18n.h>
#include <libintl.h>

gchar *skins_paths[SKINS_PATH_COUNT] = {};

Interface skins_interface =
{
    .id = "skinned",
    .desc = "Audacious Skinned GUI",
    .init = skins_init,
    .fini = skins_cleanup
};

SIMPLE_INTERFACE_PLUGIN("skinned", &skins_interface);
gboolean plugin_is_active = FALSE;
static GtkWidget *cfgdlg;

static void skins_free_paths(void) {
    int i;

    for (i = 0; i < SKINS_PATH_COUNT; i++)  {
        g_free(skins_paths[i]);
        skins_paths[i] = NULL;
    }
}

static void skins_init_paths() {
    char *xdg_data_home;
    char *xdg_cache_home;

    xdg_data_home = (getenv("XDG_DATA_HOME") == NULL
        ? g_build_filename(g_get_home_dir(), ".local", "share", NULL)
        : g_strdup(getenv("XDG_DATA_HOME")));
    xdg_cache_home = (getenv("XDG_CACHE_HOME") == NULL
        ? g_build_filename(g_get_home_dir(), ".cache", NULL)
        : g_strdup(getenv("XDG_CACHE_HOME")));

    skins_paths[SKINS_PATH_USER_SKIN_DIR] =
        g_build_filename(xdg_data_home, "audacious", "Skins", NULL);
    skins_paths[SKINS_PATH_SKIN_THUMB_DIR] =
        g_build_filename(xdg_cache_home, "audacious", "thumbs", NULL);

    g_free(xdg_data_home);
    g_free(xdg_cache_home);
}

gboolean skins_init(void) {
    plugin_is_active = TRUE;
    g_log_set_handler(NULL, G_LOG_LEVEL_WARNING, g_log_default_handler, NULL);

    skins_init_paths();
    skins_cfg_load();

    ui_main_check_theme_engine();

    register_aud_stock_icons();
    ui_manager_init();
    ui_manager_create_menus();

    init_skins(config.skin);
    mainwin_setup_menus();

    gint h_vol[2];
    aud_input_get_volume(&h_vol[0], &h_vol[1]);
    aud_hook_call("volume set", h_vol);

    skins_interface.ops->create_prefs_window();
    cfgdlg = skins_configure();
    aud_prefswin_page_new(cfgdlg, N_("Skinned Interface"), DATA_DIR "/images/appearance.png");

    aud_hook_call("create prefswin", NULL);

    if (config.player_visible)
       mainwin_show (1);
    if (config.equalizer_visible) equalizerwin_show(TRUE);
    if (config.playlist_visible)
       playlistwin_show (1);

    if (audacious_drct_get_playing ())
        ui_main_evlistener_playback_begin (0, 0);
    if (audacious_drct_get_paused ())
        ui_main_evlistener_playback_pause (0, 0);

    g_message("Entering Gtk+ main loop!");
    gtk_main();

    return TRUE;
}

gboolean skins_cleanup(void) {
    if (plugin_is_active == TRUE) {
        gtk_widget_hide(mainwin);
        gtk_widget_hide(equalizerwin);
        gtk_widget_hide(playlistwin);
        skins_cfg_save();
        cleanup_skins();
        skins_free_paths();
        ui_main_evlistener_dissociate();
        ui_playlist_evlistener_dissociate();
        skins_cfg_free();
        ui_manager_destroy();
        mainwin = NULL;
        equalizerwin = NULL;
        playlistwin = NULL;
        mainwin_info = NULL;
        plugin_is_active = FALSE;
    }

    gtk_main_quit();

    return TRUE;
}

void skins_about(void) {
    static GtkWidget* about_window = NULL;

    if (about_window) {
        gtk_window_present(GTK_WINDOW(about_window));
        return;
    }

    about_window = audacious_info_dialog(_("About Skinned GUI"),
                   _("Copyright (c) 2008, by Tomasz Moń <desowin@gmail.com>\n\n"),
                   _("OK"), FALSE, NULL, NULL);

    g_signal_connect(G_OBJECT(about_window), "destroy",	G_CALLBACK(gtk_widget_destroyed), &about_window);
}
