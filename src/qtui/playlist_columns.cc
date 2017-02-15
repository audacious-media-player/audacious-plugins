/*
 * playlist_columns.cc
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

#include "playlist_columns.h"

#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/index.h>
#include <libaudcore/runtime.h>

const char * const pl_col_names[PL_COLS] = {
    N_("Now Playing"),
    N_("Entry number"),
    N_("Title"),
    N_("Artist"),
    N_("Year"),
    N_("Album"),
    N_("Album artist"),
    N_("Track"),
    N_("Genre"),
    N_("Queue position"),
    N_("Length"),
    N_("File path"),
    N_("File name"),
    N_("Custom title"),
    N_("Bitrate"),
    N_("Comment")
};

int pl_num_cols;
int pl_cols[PL_COLS];
int pl_col_widths[PL_COLS];

const char * const pl_col_keys[PL_COLS] = {
    "playing",
    "number",
    "title",
    "artist",
    "year",
    "album",
    "album-artist",
    "track",
    "genre",
    "queued",
    "length",
    "path",
    "filename",
    "custom",
    "bitrate",
    "comment"
};

const int pl_default_widths[PL_COLS] = {
    25,   // playing
    25,   // entry number
    275,  // title
    175,  // artist
    50,   // year
    175,  // album
    175,  // album artist
    75,   // track
    100,  // genre
    25,   // queue position
    75,   // length
    275,  // path
    275,  // filename
    275,  // custom title
    75,   // bitrate
    275   // comment
};

void pl_col_init ()
{
    pl_num_cols = 0;

    String columns = aud_get_str ("qtui", "playlist_columns");
    Index<String> index = str_list_to_index (columns, " ");

    int count = aud::min (index.len (), (int) PL_COLS);

    for (int c = 0; c < count; c ++)
    {
        const String & column = index[c];

        int i = 0;
        while (i < PL_COLS && strcmp (column, pl_col_keys[i]))
            i ++;

        if (i == PL_COLS)
            break;

        pl_cols[pl_num_cols ++] = i;
    }

    auto widths = str_list_to_index (aud_get_str ("qtui", "column_widths"), ", ");
    int nwidths = aud::min (widths.len (), (int) PL_COLS);

    for (int i = 0; i < nwidths; i ++)
        pl_col_widths[i] = str_to_int (widths[i]);
    for (int i = nwidths; i < PL_COLS; i ++)
        pl_col_widths[i] = pl_default_widths[i];
}

void pl_col_save ()
{
    Index<String> index;
    for (int i = 0; i < pl_num_cols; i ++)
        index.append (String (pl_col_keys[pl_cols[i]]));

    aud_set_str ("qtui", "playlist_columns", index_to_str_list (index, " "));
    aud_set_str ("qtui", "column_widths", int_array_to_str (pl_col_widths, PL_COLS));
}
