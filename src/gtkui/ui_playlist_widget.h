/*
 * ui_playlist_widget.h
 * Copyright 2011-2012 John Lindgren, William Pitcock, and Michał Lipski
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#ifndef UI_PLAYLIST_WIDGET_H
#define UI_PLAYLIST_WIDGET_H

#include <gtk/gtk.h>
#include <libaudcore/playlist.h>

GtkWidget * ui_playlist_widget_new (Playlist playlist);
void ui_playlist_widget_update (GtkWidget * widget);
void ui_playlist_widget_scroll (GtkWidget * widget);

enum {
    PW_COL_NUMBER,
    PW_COL_TITLE,
    PW_COL_ARTIST,
    PW_COL_YEAR,
    PW_COL_ALBUM,
    PW_COL_ALBUM_ARTIST,
    PW_COL_TRACK,
    PW_COL_GENRE,
    PW_COL_QUEUED,
    PW_COL_LENGTH,
    PW_COL_PATH,
    PW_COL_FILENAME,
    PW_COL_CUSTOM,
    PW_COL_BITRATE,
    PW_COL_COMMENT,
    PW_COLS
};

extern const char * const pw_col_names[PW_COLS];

extern int pw_num_cols;
extern int pw_cols[PW_COLS];
extern int pw_col_widths[PW_COLS];

void pw_col_init ();
void * pw_col_create_chooser ();
void pw_col_save ();
void pw_col_cleanup ();

#endif
