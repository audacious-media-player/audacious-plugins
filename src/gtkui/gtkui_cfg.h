/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2009  Audacious development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifndef UI_CFG_H
#define UI_CFG_H

#include <glib.h>
#include "util.h"

#define MAINWIN_DEFAULT_WIDTH     600
#define MAINWIN_DEFAULT_HEIGHT    400
#define MAINWIN_DEFAULT_POS_X     100
#define MAINWIN_DEFAULT_POS_Y     100

typedef struct
{
    gint player_x, player_y;
    gint player_width, player_height;
    gboolean save_window_position;
    gboolean player_visible;
    gboolean infoarea_visible;
    gboolean menu_visible;
    gboolean statusbar_visible;
    gboolean show_song_titles;
    gchar * playlist_columns;
    gboolean playlist_headers;
    gboolean custom_playlist_colors;
    gchar * playlist_bg;
    gchar * playlist_fg;
    gchar * playlist_font;
    gboolean always_on_top;
    gboolean autoscroll;
} gtkui_cfg_t;

extern gtkui_cfg_t config;

void gtkui_cfg_free();
void gtkui_cfg_load();
void gtkui_cfg_save();

#endif
