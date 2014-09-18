/*
 * playlist_tabs.h
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

#ifndef PLAYLIST_TABS_H
#define PLAYLIST_TABS_H

#include <libaudcore/playlist.h>

#include "playlist.h"
#include "filter_input.h"

class PlaylistTabBar;

class PlaylistTabs : public QTabWidget
{
    Q_OBJECT

public:
    PlaylistTabs (QWidget * parent = 0);
    ~PlaylistTabs ();
    Playlist * playlistWidget (int num);
    Playlist * activePlaylistWidget ();

    void editTab (int idx);

public slots:
    void filterTrigger (const QString &text);
    void currentChangedTrigger (int idx);
    void tabEditedTrigger ();

protected:
    bool eventFilter (QObject * obj, QEvent *e);

private:
    PlaylistTabBar *m_tabbar;

    void populatePlaylists ();
    void maybeCreateTab (int count_, int uniq_id);
    void cullPlaylists ();
    void cancelRename ();

    static void playlist_update_cb (void * data, PlaylistTabs * tabWidget)
    {
        if (data == PLAYLIST_UPDATE_STRUCTURE)
            tabWidget->populatePlaylists();

        int lists = aud_playlist_count ();

        for (int list = 0; list < lists; list ++)
        {
            int at, count;
            PlaylistUpdateLevel level = aud_playlist_updated_range (list, & at, & count);

            if (level)
                tabWidget->playlistWidget (list)->update (level, at, count);
        }
    }

    static void playlist_activate_cb (void * data, PlaylistTabs * tabWidget)
    {

    }

    static void playlist_set_playing_cb (void * data, PlaylistTabs * tabWidget)
    {

    }

    static void playlist_position_cb (void * data, PlaylistTabs * tabWidget)
    {
        int num = (int) (long) data;
        auto playlistWidget = tabWidget->playlistWidget (num);
        if (playlistWidget)
            playlistWidget->positionUpdate ();
    }
};

class PlaylistTabBar : public QTabBar
{
    Q_OBJECT

public:
    PlaylistTabBar (QWidget * parent = 0);

public slots:
    void handleCloseRequest (int idx);

protected:
    void mouseDoubleClickEvent (QMouseEvent *e);
};

#endif
