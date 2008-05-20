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

#ifndef SKINS_CFG_H
#define SKINS_CFG_H

#include <glib.h>


typedef struct {
    gboolean scaled;
    gboolean always_on_top;
    gfloat scale_factor;
    gboolean always_show_cb;
    gchar *skin;
    gboolean player_shaded, equalizer_shaded, playlist_shaded;
    gboolean dim_titlebar;
    gboolean show_wm_decorations;
    gboolean easy_move;
} skins_cfg_t;

extern skins_cfg_t config;

skins_cfg_t * skins_cfg_new(void);
void skins_cfg_free();
void skins_cfg_load();
void skins_cfg_save();

#endif
