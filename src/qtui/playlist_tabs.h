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
public:
    PlaylistTabs (QWidget * parent = nullptr);

    PlaylistWidget * playlistWidget (int num)
        { return (PlaylistWidget *) widget (num); }

    void editTab (int idx);
    void filterTrigger (const QString & text);
    void currentChangedTrigger (int idx);
    void tabEditedTrigger ();

protected:
    bool eventFilter (QObject * obj, QEvent * e);

private:
    QWidget * m_leftbtn;
    PlaylistTabBar * m_tabbar;

    QLineEdit * getTabEdit (int idx);
    void setTabTitle (int idx, const char * text);
    void setupTab (int idx, QWidget * button, const char * text, QWidget * * oldp);

    void addRemovePlaylists ();
    void updateTitles ();
    void renameCurrent ();
    void cancelRename ();

    void playlist_activate_cb ();
    void playlist_update_cb (Playlist::UpdateLevel global_level);
    void playlist_position_cb (int list);

    const HookReceiver<PlaylistTabs>
     activate_hook {"playlist activate", this, & PlaylistTabs::playlist_activate_cb};
    const HookReceiver<PlaylistTabs, Playlist::UpdateLevel>
     update_hook {"playlist update", this, & PlaylistTabs::playlist_update_cb};
    const HookReceiver<PlaylistTabs, int>
     position_hook {"playlist position", this, & PlaylistTabs::playlist_position_cb};
    const HookReceiver<PlaylistTabs>
     rename_hook {"qtui rename playlist", this, & PlaylistTabs::renameCurrent};
};

class PlaylistTabBar : public QTabBar
{
public:
    PlaylistTabBar (QWidget * parent = nullptr);
    void handleCloseRequest (int idx);

protected:
    void mousePressEvent (QMouseEvent * e);
    void mouseDoubleClickEvent (QMouseEvent * e);
};

#endif
