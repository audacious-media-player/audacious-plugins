/*
 * menu-ops.cc
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

#include <libaudcore/drct.h>
#include <libaudcore/playlist.h>

#define ACTIVE aud_playlist_get_active ()

void rm_dupes_title () { aud_playlist_remove_duplicates_by_scheme (ACTIVE, Playlist::Title); }
void rm_dupes_filename () { aud_playlist_remove_duplicates_by_scheme (ACTIVE, Playlist::Filename); }
void rm_dupes_path () { aud_playlist_remove_duplicates_by_scheme (ACTIVE, Playlist::Path); }

void sort_track () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Track); }
void sort_title () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Title); }
void sort_artist () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Artist); }
void sort_album () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Album); }
void sort_album_artist () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::AlbumArtist); }
void sort_date () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Date); }
void sort_genre () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Genre); }
void sort_length () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Length); }
void sort_path () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Path); }
void sort_filename () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::Filename); }
void sort_custom_title () { aud_playlist_sort_by_scheme (ACTIVE, Playlist::FormattedTitle); }
void sort_reverse () { aud_playlist_reverse (ACTIVE); }
void sort_random () { aud_playlist_randomize (ACTIVE); }

void sort_sel_track () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Track); }
void sort_sel_title () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Title); }
void sort_sel_artist () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Artist); }
void sort_sel_album () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Album); }
void sort_sel_album_artist () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::AlbumArtist); }
void sort_sel_date () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Date); }
void sort_sel_genre () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Genre); }
void sort_sel_length () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Length); }
void sort_sel_path () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Path); }
void sort_sel_filename () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::Filename); }
void sort_sel_custom_title () { aud_playlist_sort_selected_by_scheme (ACTIVE, Playlist::FormattedTitle); }
void sort_sel_reverse () { aud_playlist_reverse_selected (ACTIVE); }
void sort_sel_random () { aud_playlist_randomize_selected (ACTIVE); }

void pl_prev ()
{
    int list = aud_playlist_get_active ();
    aud_playlist_set_active (list > 0 ? list - 1 : aud_playlist_count () - 1);
}

void pl_next ()
{
    aud_playlist_set_active ((aud_playlist_get_active () + 1) % aud_playlist_count ());
}

void pl_play () { aud_playlist_play (ACTIVE); }

void pl_select_all () { aud_playlist_select_all (ACTIVE, true); }
void pl_select_none () { aud_playlist_select_all (ACTIVE, false); }
void pl_refresh () { aud_playlist_rescan (ACTIVE); }
void pl_refresh_sel () { aud_playlist_rescan_selected (ACTIVE); }
void pl_remove_failed () { aud_playlist_remove_failed (ACTIVE); }
void pl_remove_selected () { aud_playlist_delete_selected (ACTIVE); }

void pl_queue_clear ()
{
    int list = aud_playlist_get_active ();
    aud_playlist_queue_delete (list, 0, aud_playlist_queue_count (list));
}

void pl_queue_toggle ()
{
    int list = aud_playlist_get_active ();
    int focus = aud_playlist_get_focus (list);
    if (focus < 0)
        return;

    /* make sure focused row is selected */
    if (! aud_playlist_entry_get_selected (list, focus))
    {
        aud_playlist_select_all (list, false);
        aud_playlist_entry_set_selected (list, focus, true);
    }

    int at = aud_playlist_queue_find_entry (list, focus);
    if (at < 0)
        aud_playlist_queue_insert_selected (list, -1);
    else
        aud_playlist_queue_delete_selected (list);
}

void pl_select_invert ()
{
    int list = aud_playlist_get_active ();
    int entries = aud_playlist_entry_count (list);

    for (int entry = 0; entry < entries; entry ++)
        aud_playlist_entry_set_selected (list, entry,
         ! aud_playlist_entry_get_selected (list, entry));
}

void pl_remove_all ()
{
    int list = aud_playlist_get_active ();
    aud_playlist_entry_delete (list, 0, aud_playlist_entry_count (list));
}

void pl_remove_unselected ()
{
    pl_select_invert ();
    pl_remove_selected ();
    pl_select_all ();
}

void volume_up () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }
void volume_down () { aud_drct_set_volume_main (aud_drct_get_volume_main () - 5); }
