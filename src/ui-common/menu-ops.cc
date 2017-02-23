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

#define ACTIVE Playlist::active_playlist ()

void rm_dupes_title () { ACTIVE.remove_duplicates (Playlist::Title); }
void rm_dupes_filename () { ACTIVE.remove_duplicates (Playlist::Filename); }
void rm_dupes_path () { ACTIVE.remove_duplicates (Playlist::Path); }

void sort_track () { ACTIVE.sort_entries (Playlist::Track); }
void sort_title () { ACTIVE.sort_entries (Playlist::Title); }
void sort_artist () { ACTIVE.sort_entries (Playlist::Artist); }
void sort_album () { ACTIVE.sort_entries (Playlist::Album); }
void sort_album_artist () { ACTIVE.sort_entries (Playlist::AlbumArtist); }
void sort_date () { ACTIVE.sort_entries (Playlist::Date); }
void sort_genre () { ACTIVE.sort_entries (Playlist::Genre); }
void sort_length () { ACTIVE.sort_entries (Playlist::Length); }
void sort_path () { ACTIVE.sort_entries (Playlist::Path); }
void sort_filename () { ACTIVE.sort_entries (Playlist::Filename); }
void sort_custom_title () { ACTIVE.sort_entries (Playlist::FormattedTitle); }
void sort_comment () { ACTIVE.sort_entries (Playlist::Comment); }
void sort_reverse () { ACTIVE.reverse_order (); }
void sort_random () { ACTIVE.randomize_order (); }

void sort_sel_track () { ACTIVE.sort_selected (Playlist::Track); }
void sort_sel_title () { ACTIVE.sort_selected (Playlist::Title); }
void sort_sel_artist () { ACTIVE.sort_selected (Playlist::Artist); }
void sort_sel_album () { ACTIVE.sort_selected (Playlist::Album); }
void sort_sel_album_artist () { ACTIVE.sort_selected (Playlist::AlbumArtist); }
void sort_sel_date () { ACTIVE.sort_selected (Playlist::Date); }
void sort_sel_genre () { ACTIVE.sort_selected (Playlist::Genre); }
void sort_sel_length () { ACTIVE.sort_selected (Playlist::Length); }
void sort_sel_path () { ACTIVE.sort_selected (Playlist::Path); }
void sort_sel_filename () { ACTIVE.sort_selected (Playlist::Filename); }
void sort_sel_custom_title () { ACTIVE.sort_selected (Playlist::FormattedTitle); }
void sort_sel_comment () { ACTIVE.sort_selected (Playlist::Comment); }
void sort_sel_reverse () { ACTIVE.reverse_selected (); }
void sort_sel_random () { ACTIVE.randomize_selected (); }

void pl_prev ()
{
    int idx = ACTIVE.index ();
    Playlist::by_index (idx > 0 ? idx - 1 : Playlist::n_playlists () - 1).activate ();
}

void pl_next ()
{
    int idx = ACTIVE.index ();
    Playlist::by_index ((idx + 1) % Playlist::n_playlists ()).activate ();
}

void pl_new () { Playlist::new_playlist (); }
void pl_play () { ACTIVE.start_playback (); }

void pl_queue_clear () { ACTIVE.queue_remove_all (); }
void pl_select_all () { ACTIVE.select_all (true); }
void pl_select_none () { ACTIVE.select_all (false); }
void pl_refresh () { ACTIVE.rescan_all (); }
void pl_refresh_sel () { ACTIVE.rescan_selected (); }
void pl_remove_all () { ACTIVE.remove_all_entries (); }
void pl_remove_failed () { ACTIVE.remove_unavailable (); }
void pl_remove_selected () { ACTIVE.remove_selected (); }

void pl_queue_toggle ()
{
    auto list = ACTIVE;
    int focus = list.get_focus ();
    if (focus < 0)
        return;

    /* make sure focused row is selected */
    if (! list.entry_selected (focus))
    {
        list.select_all (false);
        list.select_entry (focus, true);
    }

    int at = list.queue_find_entry (focus);
    if (at < 0)
        list.queue_insert_selected (-1);
    else
        list.queue_remove_selected ();
}

void pl_select_invert ()
{
    auto list = ACTIVE;
    int entries = list.n_entries ();

    for (int entry = 0; entry < entries; entry ++)
        list.select_entry (entry, ! list.entry_selected (entry));
}

void pl_remove_unselected ()
{
    pl_select_invert ();
    pl_remove_selected ();
    pl_select_all ();
}

void volume_up () { aud_drct_set_volume_main (aud_drct_get_volume_main () + 5); }
void volume_down () { aud_drct_set_volume_main (aud_drct_get_volume_main () - 5); }
