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

#include <gtk/gtk.h>

struct PluginPreferences;

typedef struct {
    int player_x, player_y;
    int equalizer_x, equalizer_y;
    int playlist_x, playlist_y;
    int playlist_width, playlist_height;
    int scale;
    bool autoscroll;
    bool analyzer_peaks;
    bool twoway_scroll;
    int vis_type;
    int analyzer_mode, analyzer_type;
    int scope_mode;
    int voiceprint_mode;
    int vu_mode;
    int analyzer_falloff, peaks_falloff;
    bool mainwin_use_bitmapfont;
} skins_cfg_t;

extern skins_cfg_t config;

void skins_cfg_load ();
void skins_cfg_save ();

void on_skin_view_drag_data_received (GtkWidget * widget,
 GdkDragContext * context, int x, int y, GtkSelectionData * selection_data,
 unsigned info, unsigned time, void * data);

extern const PluginPreferences skins_prefs;

#endif
