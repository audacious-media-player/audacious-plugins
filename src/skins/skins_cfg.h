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

typedef struct {
    gint player_x, player_y;
    gint equalizer_x, equalizer_y;
    gint playlist_x, playlist_y;
    gint playlist_width, playlist_height;
    gboolean autoscroll;
    gboolean always_on_top, sticky;
    gboolean equalizer_visible, playlist_visible;
    gboolean player_shaded, equalizer_shaded, playlist_shaded;
    gboolean analyzer_peaks;
    gboolean twoway_scroll;
    gint timer_mode;
    gint vis_type;
    gint analyzer_mode, analyzer_type;
    gint scope_mode;
    gint voiceprint_mode;
    gint vu_mode;
    gint analyzer_falloff, peaks_falloff;
    gboolean mainwin_use_bitmapfont;
} skins_cfg_t;

extern skins_cfg_t config;

void skins_cfg_load();
void skins_cfg_save();

void skins_configure (void);
void skins_configure_cleanup (void);

void on_skin_view_drag_data_received(GtkWidget * widget,
                                GdkDragContext * context,
                                gint x, gint y,
                                GtkSelectionData * selection_data,
                                guint info, guint time,
                                gpointer user_data);

#endif
