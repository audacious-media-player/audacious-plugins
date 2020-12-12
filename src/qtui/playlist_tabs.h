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

#include <QTabBar>
#include <QTabWidget>

#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>

class PlaylistTabBar;
class PlaylistWidget;
class QLineEdit;
class QMenu;

class PlaylistTabs : public QTabWidget
{
public:
    PlaylistTabs(QWidget * parent = nullptr);

    PlaylistWidget * currentPlaylistWidget() const;
    PlaylistWidget * playlistWidget(int idx) const;

    void currentChangedTrigger(int idx);
    void tabEditedTrigger();

protected:
    bool eventFilter(QObject * obj, QEvent * e);

private:
    QMenu * m_pl_menu;
    PlaylistTabBar * m_tabbar;
    bool m_in_update = false;

    void activateSearch();
    void addRemovePlaylists();
    void renameCurrent();

    void playlist_activate_cb();
    void playlist_update_cb(Playlist::UpdateLevel global_level);
    void playlist_position_cb(Playlist list);

    const HookReceiver<PlaylistTabs> //
        hook1{"qtui find", this, &PlaylistTabs::activateSearch},
        hook2{"qtui rename playlist", this, &PlaylistTabs::renameCurrent};

    const HookReceiver<PlaylistTabs> activate_hook{
        "playlist activate", this, &PlaylistTabs::playlist_activate_cb};
    const HookReceiver<PlaylistTabs, Playlist::UpdateLevel> update_hook{
        "playlist update", this, &PlaylistTabs::playlist_update_cb};
    const HookReceiver<PlaylistTabs, Playlist> position_hook{
        "playlist position", this, &PlaylistTabs::playlist_position_cb};
};

class PlaylistTabBar : public QTabBar
{
public:
    PlaylistTabBar(QWidget * parent = nullptr);

    void updateTitles();
    void updateIcons();
    void startRename(Playlist playlist);
    bool cancelRename();

protected:
    void mousePressEvent(QMouseEvent * e) override;
    void mouseDoubleClickEvent(QMouseEvent * e) override;
    void contextMenuEvent(QContextMenuEvent * e) override;

private:
    QLineEdit * getTabEdit(int idx);
    void updateTabText(int idx);
    void setupTab(int idx, QWidget * button, QWidget ** oldp);
    void tabMoved(int from, int to);
    void updateSettings();

    const HookReceiver<PlaylistTabBar> //
        pause_hook{"playback pause", this, &PlaylistTabBar::updateIcons},
        unpause_hook{"playback unpause", this, &PlaylistTabBar::updateIcons},
        set_playing_hook{"playlist set playing", this,
                         &PlaylistTabBar::updateIcons},
        settings_hook{"qtui update playlist settings", this,
                      &PlaylistTabBar::updateSettings};

    QWidget * m_leftbtn = nullptr;
};

#endif
