/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

#ifndef AUDACIOUS_UI_PLAYLIST_H
#define AUDACIOUS_UI_PLAYLIST_H

#include <gtk/gtk.h>

#include "ui_main.h"

#define PLAYLISTWIN_FRAME_TOP_HEIGHT    20
#define PLAYLISTWIN_FRAME_BOTTOM_HEIGHT 38
#define PLAYLISTWIN_FRAME_LEFT_WIDTH    12
#define PLAYLISTWIN_FRAME_RIGHT_WIDTH   19

#define PLAYLISTWIN_MIN_WIDTH           MAINWIN_WIDTH
#define PLAYLISTWIN_MIN_HEIGHT          MAINWIN_HEIGHT
#define PLAYLISTWIN_WIDTH_SNAP          25
#define PLAYLISTWIN_HEIGHT_SNAP         29
#define PLAYLISTWIN_SHADED_HEIGHT       MAINWIN_SHADED_HEIGHT
#define PLAYLISTWIN_WIDTH               config.playlist_width
#define PLAYLISTWIN_HEIGHT \
    (config.playlist_shaded ? PLAYLISTWIN_SHADED_HEIGHT : config.playlist_height)

#define PLAYLISTWIN_DEFAULT_WIDTH       275
#define PLAYLISTWIN_DEFAULT_HEIGHT      232
#define PLAYLISTWIN_DEFAULT_POS_X       295
#define PLAYLISTWIN_DEFAULT_POS_Y       20

#define PLAYLISTWIN_DEFAULT_FONT        "Sans Bold 8"

void playlistwin_update (void);
void playlistwin_set_toprow(gint top);
void playlistwin_shade_toggle(void);
void playlistwin_create(void);
void playlistwin_unhook (void);
void playlistwin_hide_timer(void);
void playlistwin_set_time (const gchar * minutes, const gchar * seconds);
void playlistwin_show (char show);
void playlistwin_set_sinfo_font(gchar *font);
void playlistwin_set_sinfo_scroll(gboolean scroll);
gint playlistwin_list_get_visible_count(void);
gint playlistwin_list_get_first(void);

extern gint active_playlist;
extern gchar * active_title;
extern glong active_length;
extern GtkWidget * playlistwin, * playlistwin_list;

#endif /* AUDACIOUS_UI_PLAYLIST_H */
