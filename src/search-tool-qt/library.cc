/*
 * library.cc
 * Copyright 2011-2019 John Lindgren
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

#include "library.h"

#include <string.h>
#include <libaudcore/i18n.h>

aud::spinlock Library::s_adding_lock;
Library * Library::s_adding_library = nullptr;

void Library::find_playlist ()
{
    m_playlist = Playlist ();

    for (int p = 0; p < Playlist::n_playlists (); p ++)
    {
        auto playlist = Playlist::by_index (p);
        if (! strcmp (playlist.get_title (), _("Library")))
        {
            m_playlist = playlist;
            break;
        }
    }
}

void Library::create_playlist ()
{
    m_playlist = Playlist::blank_playlist ();
    m_playlist.set_title (_("Library"));
    m_playlist.active_playlist ();
}

bool Library::check_playlist (bool require_added, bool require_scanned)
{
    if (! m_playlist.exists ())
    {
        m_playlist = Playlist ();
        return false;
    }

    if (require_added && m_playlist.add_in_progress ())
        return false;
    if (require_scanned && m_playlist.scan_in_progress ())
        return false;

    return true;
}

void Library::set_adding (bool adding)
{
    auto lh = s_adding_lock.take ();
    s_adding_library = adding ? this : nullptr;
}

bool Library::filter_cb (const char * filename, void *)
{
    bool add = false;
    auto lh = s_adding_lock.take ();

    if (s_adding_library)
    {
        bool * added = s_adding_library->m_added_table.lookup (String (filename));

        if ((add = ! added))
            s_adding_library->m_added_table.add (String (filename), true);
        else
            (* added) = true;
    }

    return add;
}

void Library::begin_add (const char * uri)
{
    if (s_adding_library)
        return;

    if (! check_playlist (false, false))
        create_playlist ();

    m_added_table.clear ();

    int entries = m_playlist.n_entries ();

    for (int entry = 0; entry < entries; entry ++)
    {
        String filename = m_playlist.entry_filename (entry);

        if (! m_added_table.lookup (filename))
        {
            m_playlist.select_entry (entry, false);
            m_added_table.add (filename, false);
        }
        else
            m_playlist.select_entry (entry, true);
    }

    m_playlist.remove_selected ();

    set_adding (true);

    Index<PlaylistAddItem> add;
    add.append (String (uri));
    m_playlist.insert_filtered (-1, std::move (add), filter_cb, nullptr, false);
}

void Library::check_ready_and_update (bool force)
{
    bool now_ready = check_playlist (true, true);
    if (now_ready != m_is_ready || force)
    {
        m_is_ready = now_ready;
        signal_update ();
    }
}

void Library::add_complete ()
{
    if (! check_playlist (true, false))
        return;

    if (s_adding_library)
    {
        set_adding (false);

        int entries = m_playlist.n_entries ();

        for (int entry = 0; entry < entries; entry ++)
        {
            String filename = m_playlist.entry_filename (entry);
            bool * added = m_added_table.lookup (filename);

            m_playlist.select_entry (entry, ! added || ! (* added));
        }

        m_added_table.clear ();

        /* don't clear the playlist if nothing was added */
        if (m_playlist.n_selected () < entries)
            m_playlist.remove_selected ();
        else
            m_playlist.select_all (false);

        m_playlist.sort_entries (Playlist::Path);
    }

    if (! m_playlist.update_pending ())
        check_ready_and_update (false);
}

void Library::scan_complete ()
{
    if (! m_playlist.update_pending ())
        check_ready_and_update (false);
}

void Library::playlist_update ()
{
    check_ready_and_update (m_playlist.update_detail ().level >= Playlist::Metadata);
}
