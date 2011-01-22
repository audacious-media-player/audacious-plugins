/*
 * ui-playlist-widget.h
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#ifndef UI_PLAYLIST_WIDGET_H
#define UI_PLAYLIST_WIDGET_H

#include <gtk/gtk.h>

GtkWidget * ui_playlist_widget_new (gint playlist);
gint ui_playlist_widget_get_playlist (GtkWidget * widget);
void ui_playlist_widget_set_playlist (GtkWidget * widget, gint playlist);
void ui_playlist_widget_update (GtkWidget * widget, gint type, gint at,
 gint count);

enum {PW_COL_NUMBER, PW_COL_TITLE, PW_COL_ARTIST, PW_COL_YEAR, PW_COL_ALBUM,
 PW_COL_TRACK, PW_COL_QUEUED, PW_COL_LENGTH, PW_COL_PATH, PW_COL_FILENAME,
 PW_COL_CUSTOM, PW_COLS};

extern const gchar * const pw_col_names[PW_COLS];

extern gint pw_num_cols;
extern gint pw_cols[PW_COLS];

void pw_col_init (void);
void pw_col_choose (void);
void pw_col_save (void);
void pw_col_cleanup (void);

#endif
