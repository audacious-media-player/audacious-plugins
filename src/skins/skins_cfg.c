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


#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_vis.h"
#include <glib.h>
#include <stdlib.h>
#include <audacious/plugin.h>

skins_cfg_t config;

skins_cfg_t skins_default_config = {
    .scaled = FALSE,
    .autoscroll = TRUE,
    .always_on_top = FALSE,
    .sticky = FALSE,
    .scale_factor = 2.0,
    .always_show_cb = TRUE,
    .close_dialog_open = TRUE,
    .close_dialog_add = TRUE,
    .skin = NULL,
    .filesel_path = NULL,
    .playlist_visible = FALSE,
    .equalizer_visible = FALSE,
    .player_visible = TRUE,
    .player_shaded = FALSE,
    .equalizer_shaded = FALSE,
    .playlist_shaded = FALSE,
    .dim_titlebar = TRUE,
    .show_wm_decorations = FALSE,
    .easy_move = TRUE,
    .allow_broken_skins = FALSE,
    .warn_about_broken_gtk_engines = TRUE,
    .warn_about_win_visibility = TRUE,
    .disable_inline_gtk = FALSE,
    .timer_mode = 0,
    .vis_type = VIS_ANALYZER,
    .analyzer_mode = ANALYZER_NORMAL,
    .analyzer_type = ANALYZER_BARS,
    .scope_mode = SCOPE_DOT,
    .voiceprint_mode = VOICEPRINT_NORMAL,
    .vu_mode = VU_SMOOTH,
    .vis_refresh = REFRESH_FULL,
    .analyzer_falloff = FALLOFF_FAST,
    .peaks_falloff = FALLOFF_SLOW,
    .player_x = MAINWIN_DEFAULT_POS_X,
    .player_y = MAINWIN_DEFAULT_POS_Y,
    .equalizer_x = EQUALIZER_DEFAULT_POS_X,
    .equalizer_y = EQUALIZER_DEFAULT_POS_Y,
    .playlist_x = PLAYLISTWIN_DEFAULT_POS_X,
    .playlist_y = PLAYLISTWIN_DEFAULT_POS_Y,
    .playlist_width = PLAYLISTWIN_DEFAULT_WIDTH,
    .playlist_height = PLAYLISTWIN_DEFAULT_HEIGHT,
    .playlist_position = 0,
    .mouse_change = 8,                 /* mouse wheel scroll step */
    .scroll_pl_by = 3,
    .colorize_r = 255, .colorize_g = 255, .colorize_b = 255,
    .snap_distance = 10,
    .snap_windows = TRUE,
    .save_window_position = TRUE,
    .analyzer_peaks = TRUE,
    .twoway_scroll = TRUE,             /* use back and forth scroll */
    .mainwin_use_bitmapfont = TRUE,
    .eq_scaled_linked = TRUE,
    .use_xmms_style_fileselector = FALSE,
    .show_numbers_in_pl = TRUE,
    .show_separator_in_pl = TRUE,
};

typedef struct skins_cfg_boolent_t {
    char const *be_vname;
    gboolean *be_vloc;
    gboolean be_wrt;
} skins_cfg_boolent;

static skins_cfg_boolent skins_boolents[] = {
    {"always_show_cb", &config.always_show_cb, TRUE},
    {"always_on_top", &config.always_on_top, TRUE},
    {"sticky", &config.sticky, TRUE},
    {"always_show_cb", &config.always_show_cb, TRUE},
    {"scaled", &config.scaled, TRUE},
    {"autoscroll_songname", &config.autoscroll, TRUE},
    {"equalizer_visible", &config.equalizer_visible, TRUE},
    {"playlist_visible", &config.playlist_visible, TRUE},
    {"player_visible", &config.player_visible, TRUE},
    {"player_shaded", &config.player_shaded, TRUE},
    {"equalizer_shaded", &config.equalizer_shaded, TRUE},
    {"playlist_shaded", &config.playlist_shaded, TRUE},
    {"dim_titlebar", &config.dim_titlebar, TRUE},
    {"show_wm_decorations", &config.show_wm_decorations, TRUE},
    {"easy_move", &config.easy_move, TRUE},
    {"allow_broken_skins", &config.allow_broken_skins, TRUE},
    {"disable_inline_gtk", &config.disable_inline_gtk, TRUE},
    {"snap_windows", &config.snap_windows, TRUE},
    {"save_window_positions", &config.save_window_position, TRUE},
    {"analyzer_peaks", &config.analyzer_peaks, TRUE},
    {"twoway_scroll", &config.twoway_scroll, TRUE},
    {"warn_about_win_visibility", &config.warn_about_win_visibility, TRUE},
    {"warn_about_broken_gtk_engines", &config.warn_about_broken_gtk_engines, TRUE},
    {"mainwin_use_bitmapfont", &config.mainwin_use_bitmapfont, TRUE},
    {"eq_scaled_linked", &config.eq_scaled_linked, TRUE},
    {"show_numbers_in_pl", &config.show_numbers_in_pl, TRUE},
    {"show_separator_in_pl", &config.show_separator_in_pl, TRUE},
};

static gint ncfgbent = G_N_ELEMENTS(skins_boolents);

typedef struct skins_cfg_nument_t {
    char const *ie_vname;
    gint *ie_vloc;
    gboolean ie_wrt;
} skins_cfg_nument;

static skins_cfg_nument skins_numents[] = {
    {"player_x", &config.player_x, TRUE},
    {"player_y", &config.player_y, TRUE},
    {"timer_mode", &config.timer_mode, TRUE},
    {"vis_type", &config.vis_type, TRUE},
    {"analyzer_mode", &config.analyzer_mode, TRUE},
    {"analyzer_type", &config.analyzer_type, TRUE},
    {"scope_mode", &config.scope_mode, TRUE},
    {"vu_mode", &config.vu_mode, TRUE},
    {"voiceprint_mode", &config.voiceprint_mode, TRUE},
    {"vis_refresh_rate", &config.vis_refresh, TRUE},
    {"analyzer_falloff", &config.analyzer_falloff, TRUE},
    {"peaks_falloff", &config.peaks_falloff, TRUE},
    {"playlist_x", &config.playlist_x, TRUE},
    {"playlist_y", &config.playlist_y, TRUE},
    {"playlist_width", &config.playlist_width, TRUE},
    {"playlist_height", &config.playlist_height, TRUE},
    {"playlist_position", &config.playlist_position, TRUE},
    {"equalizer_x", &config.equalizer_x, TRUE},
    {"equalizer_y", &config.equalizer_y, TRUE},
    {"mouse_wheel_change", &config.mouse_change, TRUE},
    {"scroll_pl_by", &config.scroll_pl_by, TRUE},
    {"colorize_r", &config.colorize_r, TRUE},
    {"colorize_g", &config.colorize_g, TRUE},
    {"colorize_b", &config.colorize_b, TRUE},
    {"snap_distance", &config.snap_distance, TRUE},
};

static gint ncfgient = G_N_ELEMENTS(skins_numents);

void skins_cfg_free() {
    if (config.skin) { g_free(config.skin); config.skin = NULL; }
}

void skins_cfg_load() {
    mcs_handle_t *cfgfile = aud_cfg_db_open();

  /* if (!aud_cfg_db_get_int(cfgfile, "skins", "field_name", &(cfg->where)))
         cfg->where = default value
     if (!aud_cfg_db_get_string(cfgfile, "skins", "field_name", &(cfg->where)))
         cfg->where = g_strdup("defaul");
     if (!aud_cfg_db_get_bool(cfgfile, "skins", "field_name", &(cfg->where)))
         cfg->where = FALSE / TRUE;
  */
  
    memcpy(&config, &skins_default_config, sizeof(skins_cfg_t));
    int i;
    
    for (i = 0; i < ncfgbent; ++i) {
        aud_cfg_db_get_bool(cfgfile, "skins",
                            skins_boolents[i].be_vname,
                            skins_boolents[i].be_vloc);
    }
    
    for (i = 0; i < ncfgient; ++i) {
        aud_cfg_db_get_int(cfgfile, "skins",
                           skins_numents[i].ie_vname,
                           skins_numents[i].ie_vloc);
    }

    if (!aud_cfg_db_get_string(cfgfile, "skins", "skin", &(config.skin)))
        config.skin = g_strdup(BMP_DEFAULT_SKIN_PATH);

    if (!aud_cfg_db_get_float(cfgfile, "skins", "scale_factor", &(config.scale_factor)))
        config.scale_factor = 2.0;

    aud_cfg_db_close(cfgfile);
}


void skins_cfg_save(skins_cfg_t * cfg) {
    mcs_handle_t *cfgfile = aud_cfg_db_open();

/*
    aud_cfg_db_set_int(cfgfile, "skins", "field_name", cfg->where);
    aud_cfg_db_set_string(cfgfile, "skins", "field_name", cfg->where);
    aud_cfg_db_set_bool(cfgfile, "skins", "field_name", cfg->where);
*/
    aud_cfg_db_set_string(cfgfile, "skins", "skin", cfg->skin);

    int i;

    for (i = 0; i < ncfgbent; ++i)
        if (skins_boolents[i].be_wrt)
            aud_cfg_db_set_bool(cfgfile, "skins",
                                skins_boolents[i].be_vname,
                                *skins_boolents[i].be_vloc);

    for (i = 0; i < ncfgient; ++i)
        if (skins_numents[i].ie_wrt)
            aud_cfg_db_set_int(cfgfile, "skins",
                               skins_numents[i].ie_vname,
                               *skins_numents[i].ie_vloc);

    aud_cfg_db_close(cfgfile);
}
