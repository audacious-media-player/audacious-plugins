/*
 * playlist_util.h
 * Copyright 2010-2011 Michał Lipski and John Lindgren
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

#ifndef __PLAYLISTUTIL_H__
#define __PLAYLISTUTIL_H__

GtkWidget * playlist_get_treeview (int playlist);

int playlist_count_selected_in_range (int list, int top, int length);
void playlist_song_info ();
void playlist_open_folder ();
void playlist_shift (int offset);

#endif
