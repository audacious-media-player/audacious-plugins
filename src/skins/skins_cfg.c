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
#include <glib.h>
#include <stdlib.h>
#include <audacious/plugin.h>

skins_cfg_t config;

skins_cfg_t skins_default_config = {
    .scaled = FALSE,
    .always_on_top = FALSE,
    .scale_factor = 2.0,
    .always_show_cb = TRUE,
    .skin = NULL,
    .player_shaded = FALSE,
    .equalizer_shaded = FALSE,
    .playlist_shaded = FALSE,
    .dim_titlebar = TRUE,
    .show_wm_decorations = FALSE,
    .easy_move = TRUE,
};

typedef struct skins_cfg_boolent_t {
    char const *be_vname;
    gboolean *be_vloc;
    gboolean be_wrt;
} skins_cfg_boolent;

static skins_cfg_boolent skins_boolents[] = {
    {"always_show_cb", &config.always_show_cb, TRUE},
    {"always_on_top", &config.always_on_top, TRUE},
    {"always_show_cb", &config.always_show_cb, TRUE},
    {"scaled", &config.scaled, TRUE},
    {"player_shaded", &config.player_shaded, TRUE},
    {"equalizer_shaded", &config.equalizer_shaded, TRUE},
    {"playlist_shaded", &config.playlist_shaded, TRUE},
    {"dim_titlebar", &config.dim_titlebar, TRUE},
    {"show_wm_decorations", &config.show_wm_decorations, TRUE},
    {"easy_move", &config.easy_move, TRUE},
};

static gint ncfgbent = G_N_ELEMENTS(skins_boolents);

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

    aud_cfg_db_close(cfgfile);
}
