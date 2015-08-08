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

    addRemovePlaylists ();
    updateTitles ();
    setCurrentIndex (aud_playlist_get_active ());

    connect (this, & QTabWidget::currentChanged, this, & PlaylistTabs::currentChangedTrigger);
}

void PlaylistTabs::addRemovePlaylists ()
{
    int tabs = count ();
    int playlists = aud_playlist_count ();

    for (int i = 0; i < tabs; i ++)
    {
        auto widget = playlistWidget (i);
        int playlist = widget->playlist ();

        if (playlist < 0)
        {
            removeTab (i);
            delete widget;
            tabs --;
            i --;
        }
        else if (playlist != i)
        {
            bool found = false;

            for (int j = i + 1; j < tabs; j ++)
            {
                widget = playlistWidget (j);
                playlist = widget->playlist ();

                if (playlist == i)
                {
                    removeTab (j);
                    insertTab (i, widget, QString ());
                    found = true;
                    break;
                }
            }

            if (! found)
            {
                int id = aud_playlist_get_unique_id (i);
                insertTab (i, new PlaylistWidget (0, id), QString ());
                tabs ++;
            }
        }
    }

    while (tabs < playlists)
    {
        int id = aud_playlist_get_unique_id (tabs);
        addTab (new PlaylistWidget (0, id), QString ());
        tabs ++;
    }
}

void PlaylistTabs::updateTitles ()
{
    int tabs = count ();
    for (int i = 0; i < tabs; i ++)
        setTabText (i, (const char *) aud_playlist_get_title (i));
}

void PlaylistTabs::filterTrigger (const QString & text)
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
        QKeyEvent * ke = (QKeyEvent *) e;

        if (ke->key () == Qt::Key_Escape)
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

void PlaylistTabs::playlist_activate_cb ()
{
    if (! aud_playlist_update_pending ())
        setCurrentIndex (aud_playlist_get_active ());
}

void PlaylistTabs::playlist_update_cb (Playlist::UpdateLevel global_level)
{
    if (global_level == Playlist::Structure)
        addRemovePlaylists ();
    if (global_level >= Playlist::Metadata)
        updateTitles ();

    int lists = aud_playlist_count ();
    for (int list = 0; list < lists; list ++)
    {
        auto update = aud_playlist_update_detail (list);
        if (update.level)
            playlistWidget (list)->update (update);
    }

    setCurrentIndex (aud_playlist_get_active ());
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

    connect (this, & QTabBar::tabCloseRequested, this, & PlaylistTabBar::handleCloseRequest);
}

void PlaylistTabBar::mousePressEvent (QMouseEvent * e)
{
    if (e->button () == Qt::MidButton)
    {
        int index = tabAt (e->pos ());
        handleCloseRequest (index);
        e->accept ();
    }

    QTabBar::mousePressEvent (e);
}

void PlaylistTabBar::mouseDoubleClickEvent (QMouseEvent * e)
{
    int idx = tabAt (e->pos ());
    if (idx < 0 || e->button () != Qt::LeftButton)
        return;

    PlaylistTabs * p = (PlaylistTabs *) parent ();
    p->editTab (idx);
}

void PlaylistTabBar::handleCloseRequest (int idx)
{
    PlaylistTabs * p = (PlaylistTabs *) parent ();
    PlaylistWidget * pl = (PlaylistWidget *) p->widget (idx);

    if (! pl)
        return;

    audqt::playlist_confirm_delete (pl->playlist ());
}
