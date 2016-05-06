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
#include "menus.h"

#include <QKeyEvent>
#include <QLineEdit>

#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

PlaylistTabs::PlaylistTabs (QWidget * parent) :
    QTabWidget (parent),
    m_pl_menu (qtui_build_pl_menu (this)),
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

PlaylistWidget * PlaylistTabs::createWidget (int list)
{
    int id = aud_playlist_get_unique_id (list);
    auto widget = new PlaylistWidget (this, id);
    widget->setContextMenu (m_pl_menu);
    return widget;
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
                insertTab (i, createWidget (i), QString ());
                tabs ++;
            }
        }
    }

    while (tabs < playlists)
    {
        addTab (createWidget (tabs), QString ());
        tabs ++;
    }
}

void PlaylistTabs::updateTitles ()
{
    int tabs = count ();
    for (int i = 0; i < tabs; i ++)
        setTabTitle (i, aud_playlist_get_title (i));
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

void PlaylistTabs::setTabTitle (int idx, const char * text)
{
    QString title = QString ();

    if (aud_get_bool ("qtui", "entry_count_visible"))
    {
        title.setNum (aud_playlist_entry_count (idx));
        title.prepend (" (").append (")");
    }

    // escape ampersands for setTabText ()
    setTabText (idx, title.prepend (QString (text).replace ("&", "&&")));
}

void PlaylistTabs::setupTab (int idx, QWidget * button, const char * text, QWidget * * oldp)
{
    QWidget * old = m_tabbar->tabButton (idx, QTabBar::LeftSide);
    m_tabbar->setTabButton (idx, QTabBar::LeftSide, button);
    setTabTitle (idx, text);

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

    QByteArray title = edit->text ().toUtf8 ();
    aud_playlist_set_title (idx, title);

    setupTab (idx, m_leftbtn, title, nullptr);
    m_leftbtn = nullptr;
}

void PlaylistTabs::editTab (int idx)
{
    QLineEdit * edit = getTabEdit (idx);

    if (! edit)
    {
        edit = new QLineEdit ((const char *) aud_playlist_get_title (idx));

        connect (edit, & QLineEdit::returnPressed, this, & PlaylistTabs::tabEditedTrigger);

        setupTab (idx, edit, nullptr, & m_leftbtn);
        setTabText (idx, nullptr);
    }

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

    if (! m_tabbar->isVisible ())
        audqt::playlist_show_rename (idx);
    else
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
    setMovable (true);
    setDocumentMode (true);
    updateSettings ();

    connect (this, & QTabBar::tabMoved, this, & PlaylistTabBar::tabMoved);
    connect (this, & QTabBar::tabCloseRequested, this, & PlaylistTabBar::handleCloseRequest);
}

void PlaylistTabBar::tabMoved (int from, int to)
{
    aud_playlist_reorder (from, to, 1);
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
    PlaylistWidget * pl = p->playlistWidget (idx);

    aud_playlist_play (pl->playlist ());
}

void PlaylistTabBar::handleCloseRequest (int idx)
{
    PlaylistTabs * p = (PlaylistTabs *) parent ();
    PlaylistWidget * pl = p->playlistWidget (idx);

    if (! pl)
        return;

    audqt::playlist_confirm_delete (pl->playlist ());
}

void PlaylistTabBar::updateSettings ()
{
#if QT_VERSION >= 0x050400
    setAutoHide (! aud_get_bool ("qtui", "playlist_tabs_visible"));
#endif
    setTabsClosable (aud_get_bool ("qtui", "close_button_visible"));
}
