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

#include "playlist.h"
#include "playlist_tabs.h"

#include <QKeyEvent>
#include <QLineEdit>

#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

PlaylistTabs::PlaylistTabs (QWidget * parent) :
    QTabWidget (parent),
    m_leftbtn (nullptr),
    m_tabbar (new PlaylistTabBar (this))
{
    installEventFilter (this);

    // set up tab bar
    m_tabbar->setFocusPolicy (Qt::NoFocus);
    setTabBar (m_tabbar);

    populatePlaylists ();
    setCurrentIndex (aud_playlist_get_active ());

    connect (this, &QTabWidget::currentChanged, this, &PlaylistTabs::currentChangedTrigger);
}

PlaylistTabs::~PlaylistTabs ()
{
    // TODO: cleanup playlists
}

void PlaylistTabs::maybeCreateTab (int count_, int uniq_id)
{
    int tabs = count ();

    for (int i = 0; i < tabs; i++)
    {
        PlaylistWidget * playlistWidget = (PlaylistWidget *) widget (i);
        if (uniq_id == playlistWidget->uniqueId())
            return;
    }

    auto playlistWidget = new PlaylistWidget (0, uniq_id);
    addTab ((QWidget *) playlistWidget, QString (aud_playlist_get_title (count_)));
}

void PlaylistTabs::cullPlaylists ()
{
    int tabs = count ();

    for (int i = 0; i < tabs; i++)
    {
         PlaylistWidget * playlistWidget = (PlaylistWidget *) widget (i);

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

PlaylistWidget * PlaylistTabs::playlistWidget (int num)
{
    return (PlaylistWidget *) widget (num);
}

void PlaylistTabs::filterTrigger (const QString &text)
{
    ((PlaylistWidget *) currentWidget ())->setFilter (text);
}

void PlaylistTabs::currentChangedTrigger (int idx)
{
    cancelRename ();
    aud_playlist_set_active (idx);
}

QLineEdit * PlaylistTabs::getTabEdit (int idx)
{
    return dynamic_cast<QLineEdit *> (m_tabbar->tabButton (idx, QTabBar::LeftSide));
}

void PlaylistTabs::setupTab (int idx, QWidget * button, const QString & text, QWidget * * oldp)
{
    QWidget * old = m_tabbar->tabButton (idx, QTabBar::LeftSide);
    m_tabbar->setTabButton (idx, QTabBar::LeftSide, button);
    setTabText (idx, text);

    if (oldp)
        * oldp = old;
    else
    {
        old->setParent (nullptr);
        old->deleteLater ();
    }
}

void PlaylistTabs::tabEditedTrigger ()
{
    int idx = currentIndex ();
    if (idx < 0)
        return;

    QLineEdit * edit = getTabEdit (idx);
    if (! edit)
        return;

    QString title = edit->text ();
    aud_playlist_set_title (idx, title.toUtf8 ());

    setupTab (idx, m_leftbtn, title, nullptr);
    m_leftbtn = nullptr;
}

void PlaylistTabs::editTab (int idx)
{
    QLineEdit * edit = new QLineEdit (tabText (idx));

    connect (edit, & QLineEdit::returnPressed, this, & PlaylistTabs::tabEditedTrigger);

    setupTab (idx, edit, QString (), & m_leftbtn);

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

void PlaylistTabs::renameCurrent ()
{
    int idx = currentIndex ();
    if (idx >= 0)
        editTab (idx);
}

void PlaylistTabs::cancelRename ()
{
    for (int i = 0; i < count (); i ++)
    {
        QLineEdit * edit = getTabEdit (i);
        if (! edit)
            continue;

        setupTab (i, m_leftbtn, (const char *) aud_playlist_get_title (i), nullptr);
        m_leftbtn = nullptr;
    }
}

void PlaylistTabs::playlist_update_cb (Playlist::UpdateLevel global_level)
{
    if (global_level == Playlist::Structure)
        populatePlaylists ();

    int lists = aud_playlist_count ();
    for (int list = 0; list < lists; list ++)
    {
        auto update = aud_playlist_update_detail (list);
        if (update.level)
            playlistWidget (list)->update (update);
    }
}

void PlaylistTabs::playlist_position_cb (int list)
{
    auto widget = playlistWidget (list);
    if (widget)
        widget->scrollToCurrent ();
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
    PlaylistWidget *pl = (PlaylistWidget *) p->widget (idx);

    if (! pl)
        return;

    audqt::playlist_confirm_delete (pl->playlist ());
}
