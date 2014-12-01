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

#include <QTabWidget>

#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>

#include "playlist.h"
#include "filter_input.h"

class PlaylistTabBar;
class PlaylistWidget;
class QLineEdit;

class PlaylistTabs : public QTabWidget
{
    Q_OBJECT

public:
    PlaylistTabs (QWidget * parent = 0);
    ~PlaylistTabs ();
    PlaylistWidget * playlistWidget (int num);
    PlaylistWidget * activePlaylistWidget ();

    void editTab (int idx);

public slots:
    void filterTrigger (const QString &text);
    void currentChangedTrigger (int idx);
    void tabEditedTrigger ();

protected:
    bool eventFilter (QObject * obj, QEvent *e);

private:
    QWidget *m_leftbtn;
    PlaylistTabBar *m_tabbar;

    QLineEdit * getTabEdit (int idx);
    void setupTab (int idx, QWidget * button, const QString & text, QWidget * * oldp);

    void populatePlaylists ();
    void maybeCreateTab (int count_, int uniq_id);
    void cullPlaylists ();
    void cancelRename ();

    void playlist_update_cb (Playlist::Update global_level);
    void playlist_position_cb (int list);

    const HookReceiver<PlaylistTabs, Playlist::Update>
     update_hook {"playlist update", this, & PlaylistTabs::playlist_update_cb};
    const HookReceiver<PlaylistTabs, int>
     position_hook {"playlist position", this, & PlaylistTabs::playlist_position_cb};
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
