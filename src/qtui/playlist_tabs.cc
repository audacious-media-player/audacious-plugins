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

#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>

#include "playlist.h"
#include "playlist_tabs.h"
#include "playlist_tabs.moc"

PlaylistTabs::PlaylistTabs (QTabWidget * parent) : QTabWidget (parent)
{
    setupUi (this);

    populatePlaylists ();

    hook_associate ("playlist update",      (HookFunction) playlist_update_cb, this);
    hook_associate ("playlist activate",    (HookFunction) playlist_activate_cb, this);
    hook_associate ("playlist set playing", (HookFunction) playlist_set_playing_cb, this);
    hook_associate ("playlist position",    (HookFunction) playlist_position_cb, this);
}

PlaylistTabs::~PlaylistTabs ()
{
    hook_dissociate ("playlist update",      (HookFunction) playlist_update_cb);
    hook_dissociate ("playlist activate",    (HookFunction) playlist_activate_cb);
    hook_dissociate ("playlist set playing", (HookFunction) playlist_set_playing_cb);
    hook_dissociate ("playlist position",    (HookFunction) playlist_position_cb);

    // TODO: cleanup playlists
}

void PlaylistTabs::maybeCreateTab (int count_, int uniq_id)
{
    int tabs = count ();

    for (int i = 0; i < tabs; i++)
    {
        Playlist * playlistWidget = (Playlist *) widget (i);
        if (uniq_id == playlistWidget->uniqueId())
            return;
    }

    auto playlistWidget = new Playlist (0, uniq_id);
    addTab ((QWidget *) playlistWidget, QString (aud_playlist_get_title (count_)));
}

void PlaylistTabs::cullDestroyedPlaylists ()
{
    int tabs = count ();

    for (int i = 0; i < tabs; i++)
    {
         Playlist * playlistWidget = (Playlist *) widget (i);

         if (playlistWidget->playlist() < 0)
         {
             removeTab(i);
             delete playlistWidget;
         }
    }
}

void PlaylistTabs::populatePlaylists ()
{
    int playlists = aud_playlist_count ();

    for (int count = 0; count < playlists; count++)
        maybeCreateTab(count, aud_playlist_get_unique_id (count));

    cullDestroyedPlaylists();
}

Playlist * PlaylistTabs::playlistWidget (int num)
{
    return (Playlist *) widget (num);
}

Playlist * PlaylistTabs::activePlaylistWidget ()
{
    int num = aud_playlist_get_active ();
    return (Playlist *) widget (num);
}

void PlaylistTabs::filterTrigger (const QString &text)
{
    activePlaylistWidget ()->setFilter (text);
}
