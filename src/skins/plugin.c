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

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "plugin.h"
#include "skins_cfg.h"
#include "ui_dock.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_manager.h"
#include "ui_playlist.h"
#include "ui_skin.h"

gchar * skins_paths[SKINS_PATH_COUNT];

static gboolean skins_init (void);
static void skins_cleanup (void);
static gboolean ui_is_shown (void);
static void show_error_message (const gchar * markup);

AUD_IFACE_PLUGIN
(
    .name = "Winamp Classic Interface",
    .init = skins_init,
    .cleanup = skins_cleanup,
    .about = skins_about,
    .configure = skins_configure,
    .show = mainwin_show,
    .is_shown = ui_is_shown,
    .show_error = show_error_message,
    .show_filebrowser = audgui_run_filebrowser,
    .show_jump_to_track = audgui_jump_to_track,
)

gboolean plugin_is_active = FALSE;

static gint update_source;

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

static gboolean update_cb (void * unused)
{
    mainwin_update_song_info ();
    return TRUE;
}

static gboolean skins_init (void)
{
    plugin_is_active = TRUE;
    g_log_set_handler(NULL, G_LOG_LEVEL_WARNING, g_log_default_handler, NULL);

    skins_init_paths();
    skins_cfg_load();

    audgui_set_default_icon();
    audgui_register_stock_icons();

    ui_manager_init();
    ui_manager_create_menus();

    init_skins(config.skin);
    mainwin_setup_menus();

    if (aud_drct_get_playing ())
    {
        ui_main_evlistener_playback_begin (NULL, NULL);
        info_change ();

        if (aud_drct_get_paused ())
            ui_main_evlistener_playback_pause (NULL, NULL);
    }
    else
        mainwin_update_song_info ();

    mainwin_show (config.player_visible);

    eq_init_hooks ();
    update_source = g_timeout_add (250, update_cb, NULL);

    return TRUE;
}

static void skins_cleanup (void)
{
    if (plugin_is_active)
    {
        skins_configure_cleanup ();

        mainwin_unhook ();
        playlistwin_unhook ();
        eq_end_hooks ();
        g_source_remove (update_source);

        skins_cfg_save();

        cleanup_skins();
        clear_dock_window_list ();
        skins_free_paths();
        skins_cfg_free();
        ui_manager_destroy();
        plugin_is_active = FALSE;
    }
}

void skins_about (void)
{
    static GtkWidget * about_window = NULL;

    audgui_simple_message (& about_window, GTK_MESSAGE_INFO,
     _("About Skinned GUI"),
     _("Copyright (c) 2008, by Tomasz Moń <desowin@gmail.com>\n\n"));
}

static gboolean ui_is_shown (void)
{
    return config.player_visible;
}

static void show_error_message(const gchar * markup)
{
    GtkWidget *dialog =
        gtk_message_dialog_new_with_markup(GTK_WINDOW(mainwin),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           "%s",_(markup));

    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show(GTK_WIDGET(dialog));

    g_signal_connect_swapped(dialog, "response",
                             G_CALLBACK(gtk_widget_destroy),
                             dialog);
}
