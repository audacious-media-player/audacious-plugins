/*
 * menu-ops.h
 * Copyright 2016 John Lindgren
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

#ifndef UI_COMMON_MENU_OPS_H
#define UI_COMMON_MENU_OPS_H

// remove duplicate playlist entries
void rm_dupes_title ();
void rm_dupes_filename ();
void rm_dupes_path ();

// sort entire playlist
void sort_track ();
void sort_title ();
void sort_artist ();
void sort_album ();
void sort_album_artist ();
void sort_date ();
void sort_genre ();
void sort_length ();
void sort_path ();
void sort_filename ();
void sort_custom_title ();
void sort_comment ();
void sort_reverse ();
void sort_random ();

// sort selected playlist entries
void sort_sel_track ();
void sort_sel_title ();
void sort_sel_artist ();
void sort_sel_album ();
void sort_sel_album_artist ();
void sort_sel_date ();
void sort_sel_genre ();
void sort_sel_length ();
void sort_sel_path ();
void sort_sel_filename ();
void sort_sel_custom_title ();
void sort_sel_comment ();
void sort_sel_reverse ();
void sort_sel_random ();

// playlist switching
void pl_prev ();
void pl_next ();
void pl_new ();
void pl_play ();

// playlist editing
void pl_queue_clear ();
void pl_queue_toggle ();
void pl_select_all ();
void pl_select_none ();
void pl_select_invert ();
void pl_refresh ();
void pl_refresh_sel ();
void pl_remove_all ();
void pl_remove_failed ();
void pl_remove_selected ();
void pl_remove_unselected ();

// clipboard
void pl_copy ();
void pl_cut ();
void pl_paste ();
void pl_paste_end ();

// misc playlist
void pl_song_info ();
void pl_open_folder ();

// volume control
void volume_up ();
void volume_down ();

#endif // UI_COMMON_MENU_OPS_H
