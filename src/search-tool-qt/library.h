/*
 * library.h
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

#ifndef LIBRARY_H
#define LIBRARY_H

#include <libaudcore/hook.h>
#include <libaudcore/multihash.h>
#include <libaudcore/playlist.h>

class Library
{
public:
    Library () { find_playlist (); }
    ~Library () { set_adding (false); }

    Playlist playlist () const { return m_playlist; }
    bool is_ready () const { return m_is_ready; }

    void begin_add (const char * uri);
    void check_ready_and_update (bool force);

    void connect_update (void (* func) (void *), void * data) {
        update_func = func;
        update_data = data;
    };

private:
    void find_playlist ();
    void create_playlist ();
    bool check_playlist (bool require_added, bool require_scanned);
    void set_adding (bool adding);

    static bool filter_cb (const char * filename, void *);

    void add_complete (void);
    void scan_complete (void);
    void playlist_update (void);

    Playlist m_playlist;
    bool m_is_ready = false;
    SimpleHash<String, bool> m_added_table;

    /* to allow safe callback access from playlist add thread */
    static aud::spinlock s_adding_lock;
    static Library * s_adding_library;

    void (* update_func) (void *) = nullptr;
    void * update_data = nullptr;

    HookReceiver<Library>
     hook1 {"playlist add complete", this, & Library::add_complete},
     hook2 {"playlist scan complete", this, & Library::scan_complete},
     hook3 {"playlist update", this, & Library::playlist_update};
};

#endif // LIBRARY_H
