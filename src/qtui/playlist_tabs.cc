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
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

#include "playlist.h"
#include "playlist_tabs.h"
#include "playlist_tabs.moc"

PlaylistTabs::PlaylistTabs (QWidget * parent) : QTabWidget (parent)
{
    installEventFilter (this);

    // set up tab bar
    m_tabbar = new PlaylistTabBar (this);
    m_tabbar->setFocusPolicy (Qt::NoFocus);
    setTabBar (m_tabbar);

    populatePlaylists ();

    hook_associate ("playlist update",      (HookFunction) playlist_update_cb, this);
    hook_associate ("playlist activate",    (HookFunction) playlist_activate_cb, this);
    hook_associate ("playlist set playing", (HookFunction) playlist_set_playing_cb, this);
    hook_associate ("playlist position",    (HookFunction) playlist_position_cb, this);

    connect (this, &QTabWidget::currentChanged, this, &PlaylistTabs::currentChangedTrigger);
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

void PlaylistTabs::cullPlaylists ()
{
    int tabs = count ();

    for (int i = 0; i < tabs; i++)
    {
         Playlist * playlistWidget = (Playlist *) widget (i);

         if (playlistWidget == nullptr || playlistWidget->playlist() < 0)
         {
             removeTab(i);
             delete playlistWidget;
         }
         else
             setTabText(i, QString (aud_playlist_get_title (playlistWidget->playlist())));
    }
}

void PlaylistTabs::populatePlaylists ()
{
    int playlists = aud_playlist_count ();

    for (int count = 0; count < playlists; count++)
        maybeCreateTab(count, aud_playlist_get_unique_id (count));

    cullPlaylists();
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

void PlaylistTabs::currentChangedTrigger (int idx)
{
    cancelRename ();
    aud_playlist_set_active (idx);
}

void PlaylistTabs::tabEditedTrigger ()
{
    int idx = currentIndex ();
    if (idx < 0)
        return;

    auto edit = dynamic_cast<QLineEdit *> (m_tabbar->tabButton (idx, QTabBar::LeftSide));
    if (! edit)
        return;

    QString title = edit->text ();
    m_tabbar->setTabButton (idx, QTabBar::LeftSide, m_leftbtn);
    setTabText (idx, title);

    aud_playlist_set_title (idx, title.toUtf8 ());
}

void PlaylistTabs::editTab (int idx)
{
    QLineEdit * edit = new QLineEdit (tabText (idx));

    connect (edit, & QLineEdit::returnPressed, this, & PlaylistTabs::tabEditedTrigger);

    m_leftbtn = m_tabbar->tabButton (idx, QTabBar::LeftSide);

    setTabText (idx, QString ());
    m_tabbar->setTabButton (idx, QTabBar::LeftSide, edit);

    edit->selectAll ();
    edit->setFocus ();
}

bool PlaylistTabs::eventFilter (QObject * obj, QEvent * e)
{
    if (e->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = (QKeyEvent *) e;

        if (ke->key() == Qt::Key_Escape)
        {
            cancelRename ();
            return true;
        }
    }

    return QTabWidget::eventFilter(obj, e);
}

void PlaylistTabs::cancelRename ()
{
    for (int i = 0; i < count (); i ++)
    {
        auto edit = dynamic_cast<QLineEdit *> (m_tabbar->tabButton (i, QTabBar::LeftSide));
        if (! edit)
            continue;

        m_tabbar->setTabButton (i, QTabBar::LeftSide, nullptr);
        setTabText (i, (const char *) aud_playlist_get_title (i));
    }
}

PlaylistTabBar::PlaylistTabBar (QWidget * parent) : QTabBar (parent)
{
    setDocumentMode (true);
    setTabsClosable (true);

    connect (this, &QTabBar::tabCloseRequested, this, &PlaylistTabBar::handleCloseRequest);
}

void PlaylistTabBar::mouseDoubleClickEvent (QMouseEvent *e)
{
    PlaylistTabs *p = (PlaylistTabs *) parent();

    int idx = tabAt (e->pos());
    if (idx < 0)
        return;

    p->editTab (idx);
}

void PlaylistTabBar::handleCloseRequest (int idx)
{
    PlaylistTabs *p = (PlaylistTabs *) parent ();
    Playlist *pl = (Playlist *) p->widget (idx);

    if (! pl)
        return;

    audqt::playlist_confirm_delete (pl->playlist ());
}
