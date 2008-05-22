/*
 * Audacious
 * Copyright (c) 2006-2007 Audacious development team.
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
#if 0
#include "ui_playlist_evlisteners.h"
#endif
#include <glib.h>
#include <math.h>
#if 0
#include "hook.h"
#include "playback.h"
#include "playlist.h"
#include "playlist_evmessages.h"
#include "visualization.h"
#endif
#include <audacious/plugin.h>
#if 0
#include "ui_credits.h"
#include "ui_equalizer.h"
#include "ui_jumptotrack.h"
#endif
#include "ui_fileopener.h"
#include "ui_main.h"
#if 0
#include "ui_playlist.h"
#include "ui_preferences.h"
#endif
#include "ui_skinned_playstatus.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_window.h"
#include "skins_cfg.h"

static gint song_info_timeout_source = 0;
static gint update_vis_timeout_source = 0;

typedef struct {
    gint bitrate;
    gint samplerate;
    gint channels;
} PlaylistEventInfoChange;

/* XXX: there has to be a better way than polling here! */
/* also: where should this function go? should it stay here? --mf0102 */
static gboolean
update_vis_func(gpointer unused)
{
    if (!audacious_drct_get_playing())
        return FALSE;
#if 0
    input_update_vis(audacious_drct_get_time());
#endif
    return TRUE;
}

static void
ui_main_evlistener_title_change(gpointer hook_data, gpointer user_data)
{
    gchar *text = (gchar *) hook_data;

    ui_skinned_textbox_set_text(mainwin_info, text);
    playlistwin_update_list(aud_playlist_get_active());
}

static void
ui_main_evlistener_hide_seekbar(gpointer hook_data, gpointer user_data)
{
    mainwin_disable_seekbar();
}

static void
ui_main_evlistener_volume_change(gpointer hook_data, gpointer user_data)
{
    gint *h_vol = (gint *) hook_data;
    gint vl, vr, b, v;

    vl = CLAMP(h_vol[0], 0, 100);
    vr = CLAMP(h_vol[1], 0, 100);
    v = MAX(vl, vr);
    if (vl > vr)
        b = (gint) rint(((gdouble) vr / vl) * 100) - 100;
    else if (vl < vr)
        b = 100 - (gint) rint(((gdouble) vl / vr) * 100);
    else
        b = 0;

    mainwin_set_volume_slider(v);
    equalizerwin_set_volume_slider(v);
    mainwin_set_balance_slider(b);
    equalizerwin_set_balance_slider(b);
}

static void
ui_main_evlistener_playback_begin(gpointer hook_data, gpointer user_data)
{

    PlaylistEntry *entry = (PlaylistEntry*)hook_data;
    g_return_if_fail(entry != NULL);
#if 0
    equalizerwin_load_auto_preset(entry->filename);
    input_set_eq(cfg.equalizer_active, cfg.equalizer_preamp,
                 cfg.equalizer_bands);
    output_set_eq(cfg.equalizer_active, cfg.equalizer_preamp,
                  cfg.equalizer_bands);
#endif
    ui_vis_clear_data(mainwin_vis);
    ui_svis_clear_data(mainwin_svis);
    mainwin_disable_seekbar();
    mainwin_update_song_info();

    if (config.player_shaded) {
        gtk_widget_show(mainwin_stime_min);
        gtk_widget_show(mainwin_stime_sec);
        gtk_widget_show(mainwin_sposition);
    } else {
        gtk_widget_show(mainwin_minus_num);
        gtk_widget_show(mainwin_10min_num);
        gtk_widget_show(mainwin_min_num);
        gtk_widget_show(mainwin_10sec_num);
        gtk_widget_show(mainwin_sec_num);
        gtk_widget_show(mainwin_position);
    }

    song_info_timeout_source = 
        g_timeout_add_seconds(1, (GSourceFunc) mainwin_update_song_info, NULL);

    update_vis_timeout_source =
        g_timeout_add(10, (GSourceFunc) update_vis_func, NULL);
#if 0
    vis_playback_start();
#endif
    ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_PLAY);
}

static void
ui_main_evlistener_playback_stop(gpointer hook_data, gpointer user_data)
{
    if (song_info_timeout_source)
        g_source_remove(song_info_timeout_source);
#if 0
    vis_playback_stop();
    free_vis_data();
#endif
    ui_skinned_playstatus_set_buffering(mainwin_playstatus, FALSE);
}

static void
ui_main_evlistener_playback_pause(gpointer hook_data, gpointer user_data)
{
    ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_PAUSE);
}

static void
ui_main_evlistener_playback_unpause(gpointer hook_data, gpointer user_data)
{
    ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_PLAY);
}

static void
ui_main_evlistener_playback_seek(gpointer hook_data, gpointer user_data)
{
#if 0
    free_vis_data();
#endif
}

static void
ui_main_evlistener_playback_play_file(gpointer hook_data, gpointer user_data)
{
#if 0
    if (cfg.random_skin_on_play)
        skin_set_random_skin();
#endif
}

static void
ui_main_evlistener_playlist_end_reached(gpointer hook_data, gpointer user_data)
{
    mainwin_clear_song_info();
#if 0
    if (cfg.stopaftersong)
        mainwin_set_stopaftersong(FALSE);
#endif
}

static void
ui_main_evlistener_playlist_info_change(gpointer hook_data, gpointer user_data)
{
    PlaylistEventInfoChange *msg = (PlaylistEventInfoChange *) hook_data;

    mainwin_set_song_info(msg->bitrate, msg->samplerate, msg->channels);
}

static void
ui_main_evlistener_mainwin_set_always_on_top(gpointer hook_data, gpointer user_data)
{
    gboolean *ontop = (gboolean*)hook_data;
    mainwin_set_always_on_top(*ontop);
}

static void
ui_main_evlistener_mainwin_show(gpointer hook_data, gpointer user_data)
{
    gboolean *show = (gboolean*)hook_data;
    mainwin_show(*show);
}

static void
ui_main_evlistener_equalizerwin_show(gpointer hook_data, gpointer user_data)
{
    gboolean *show = (gboolean*)hook_data;
    equalizerwin_show(*show);
}

static void
ui_main_evlistener_prefswin_show(gpointer hook_data, gpointer user_data)
{
#if 0
    gboolean *show = (gboolean*)hook_data;
    if (*show == TRUE)
        show_prefs_window();
    else
        hide_prefs_window();
#endif
}

static void
ui_main_evlistener_aboutwin_show(gpointer hook_data, gpointer user_data)
{
#if 0
    gboolean *show = (gboolean*)hook_data;
    if (*show == TRUE)
        show_about_window();
    else
        hide_about_window();
#endif
}


static void
ui_main_evlistener_ui_jump_to_track_show(gpointer hook_data, gpointer user_data)
{
#if 0
    gboolean *show = (gboolean*)hook_data;
    if (*show == TRUE)
        ui_jump_to_track();
    else
        ui_jump_to_track_hide();
#endif
}

static void
ui_main_evlistener_filebrowser_show(gpointer hook_data, gpointer user_data)
{
#if 0
    gboolean *play_button = (gboolean*)hook_data;
    run_filebrowser(*play_button);
#endif
}

static void
ui_main_evlistener_filebrowser_hide(gpointer hook_data, gpointer user_data)
{
#if 0
    hide_filebrowser();
#endif
}

static void
ui_main_evlistener_visualization_timeout(gpointer hook_data, gpointer user_data)
{
    if (config.player_shaded && config.player_visible)
        ui_svis_timeout_func(mainwin_svis, hook_data);
    else
        ui_vis_timeout_func(mainwin_vis, hook_data);
}

static void
ui_main_evlistener_config_save(gpointer hook_data, gpointer user_data)
{
    ConfigDb *db = (ConfigDb *) hook_data;

    if (SKINNED_WINDOW(mainwin)->x != -1 &&
        SKINNED_WINDOW(mainwin)->y != -1 )
    {
        aud_cfg_db_set_int(db, "skins", "player_x", SKINNED_WINDOW(mainwin)->x);
        aud_cfg_db_set_int(db, "skins", "player_y", SKINNED_WINDOW(mainwin)->y);
    }

    aud_cfg_db_set_bool(db, "skins", "mainwin_use_bitmapfont",
                    config.mainwin_use_bitmapfont);
}

void
ui_main_evlistener_init(void)
{
    aud_hook_associate("title change", ui_main_evlistener_title_change, NULL);
    aud_hook_associate("hide seekbar", ui_main_evlistener_hide_seekbar, NULL);
    aud_hook_associate("volume set", ui_main_evlistener_volume_change, NULL);
    aud_hook_associate("playback begin", ui_main_evlistener_playback_begin, NULL);
    aud_hook_associate("playback stop", ui_main_evlistener_playback_stop, NULL);
    aud_hook_associate("playback pause", ui_main_evlistener_playback_pause, NULL);
    aud_hook_associate("playback unpause", ui_main_evlistener_playback_unpause, NULL);
    aud_hook_associate("playback seek", ui_main_evlistener_playback_seek, NULL);
    aud_hook_associate("playback play file", ui_main_evlistener_playback_play_file, NULL);
    aud_hook_associate("playlist end reached", ui_main_evlistener_playlist_end_reached, NULL);
    aud_hook_associate("playlist info change", ui_main_evlistener_playlist_info_change, NULL);
    aud_hook_associate("mainwin set always on top", ui_main_evlistener_mainwin_set_always_on_top, NULL);
    aud_hook_associate("mainwin show", ui_main_evlistener_mainwin_show, NULL);
    aud_hook_associate("equalizerwin show", ui_main_evlistener_equalizerwin_show, NULL);
#if 0
    aud_hook_associate("prefswin show", ui_main_evlistener_prefswin_show, NULL);
    aud_hook_associate("aboutwin show", ui_main_evlistener_aboutwin_show, NULL);
    aud_hook_associate("ui jump to track show", ui_main_evlistener_ui_jump_to_track_show, NULL);
    aud_hook_associate("filebrowser show", ui_main_evlistener_filebrowser_show, NULL);
    aud_hook_associate("filebrowser hide", ui_main_evlistener_filebrowser_hide, NULL);
#endif
    aud_hook_associate("visualization timeout", ui_main_evlistener_visualization_timeout, NULL);
    aud_hook_associate("config save", ui_main_evlistener_config_save, NULL);
}

