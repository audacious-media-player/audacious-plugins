/*
 * playlist_tabs.cc
 * Copyright 2014 Michał Lipski
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

#include <QtGui>

#include <libaudcore/playlist.h>

#include "playlist.h"
#include "playlist_tabs.h"
#include "playlist_tabs.moc"

PlaylistTabs::PlaylistTabs (QTabWidget * parent) : QTabWidget (parent)
{
    setupUi (this);
    populatePlaylists ();
}

PlaylistTabs::~PlaylistTabs ()
{
    // TODO: cleanup playlists
}

void PlaylistTabs::populatePlaylists ()
{
    int playlists = aud_playlist_count ();

    for (int count = 0; count < playlists; count++)
    {
        auto playlistWidget = new Playlist (0, aud_playlist_get_unique_id (count));
        addTab ((QWidget *) playlistWidget, QString (aud_playlist_get_title (count)));
    }
}
