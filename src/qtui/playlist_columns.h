/*
 * playlist_columns.h
 * Copyright 2017 Eugene Paskevich
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

#ifndef PLAYLIST_COLUMNS_H
#define PLAYLIST_COLUMNS_H

enum {
    PL_COL_NOW_PLAYING,
    PL_COL_NUMBER,
    PL_COL_TITLE,
    PL_COL_ARTIST,
    PL_COL_YEAR,
    PL_COL_ALBUM,
    PL_COL_ALBUM_ARTIST,
    PL_COL_TRACK,
    PL_COL_GENRE,
    PL_COL_QUEUED,
    PL_COL_LENGTH,
    PL_COL_PATH,
    PL_COL_FILENAME,
    PL_COL_CUSTOM,
    PL_COL_BITRATE,
    PL_COL_COMMENT,
    PL_COLS
};

extern const char * const pl_col_names[PL_COLS];

extern int pl_num_cols;
extern int pl_cols[PL_COLS];
extern int pl_col_widths[PL_COLS];

void pl_col_init ();
void pl_col_save ();

void * pl_col_create_chooser ();

#endif
