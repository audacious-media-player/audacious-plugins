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
#include "search_bar.h"

#include <QBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>

#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

class LayoutWidget : public QWidget
{
public:
    LayoutWidget (QWidget * parent, Playlist playlist, QMenu * contextMenu);

    PlaylistWidget * playlistWidget () const
        { return m_playlistWidget; }

    void activateSearch ()
    {
        m_searchBar->show ();
        m_searchBar->setFocus ();
    }

private:
    PlaylistWidget * m_playlistWidget;
    SearchBar * m_searchBar;
};

LayoutWidget::LayoutWidget (QWidget * parent, Playlist playlist, QMenu * contextMenu) :
    QWidget (parent),
    m_playlistWidget (new PlaylistWidget (this, playlist)),
    m_searchBar (new SearchBar (this, m_playlistWidget))
{
    auto layout = audqt::make_vbox (this, 0);
    layout->addWidget (m_playlistWidget);
    layout->addWidget (m_searchBar);

    m_playlistWidget->setContextMenu (contextMenu);
    m_searchBar->hide ();
}

/* --------------------------------- */

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
    setCurrentIndex (Playlist::active_playlist ().index ());

    connect (this, & QTabWidget::currentChanged, this, & PlaylistTabs::currentChangedTrigger);
}

PlaylistWidget * PlaylistTabs::currentPlaylistWidget () const
{
    return ((LayoutWidget *) currentWidget ())->playlistWidget ();
}

PlaylistWidget * PlaylistTabs::playlistWidget (int idx) const
{
    auto w = (LayoutWidget *) widget (idx);
    return w ? w->playlistWidget () : nullptr;
}

void PlaylistTabs::activateSearch ()
{
    ((LayoutWidget *) currentWidget ())->activateSearch ();
}

void PlaylistTabs::addRemovePlaylists ()
{
    int tabs = count ();
    int playlists = Playlist::n_playlists ();

    for (int i = 0; i < tabs; i ++)
    {
        auto w = (LayoutWidget *) widget (i);
        int list_idx = w->playlistWidget ()->playlist ().index ();

        if (list_idx < 0)
        {
            removeTab (i);
            delete w;
            tabs --;
            i --;
        }
        else if (list_idx != i)
        {
            bool found = false;

            for (int j = i + 1; j < tabs; j ++)
            {
                w = (LayoutWidget *) widget (j);
                list_idx = w->playlistWidget ()->playlist ().index ();

                if (list_idx == i)
                {
                    removeTab (j);
                    insertTab (i, w, QString ());
                    found = true;
                    break;
                }
            }

            if (! found)
            {
                insertTab (i, new LayoutWidget (this, Playlist::by_index (i), m_pl_menu), QString ());
                tabs ++;
            }
        }
    }

    while (tabs < playlists)
    {
        addTab (new LayoutWidget (this, Playlist::by_index (tabs), m_pl_menu), QString ());
        tabs ++;
    }
}

void PlaylistTabs::updateTitles ()
{
    int tabs = count ();
    for (int i = 0; i < tabs; i ++)
        updateTabText (i);
}

void PlaylistTabs::currentChangedTrigger (int idx)
{
    Playlist::by_index (idx).activate ();
}

QLineEdit * PlaylistTabs::getTabEdit (int idx)
{
    return dynamic_cast<QLineEdit *> (m_tabbar->tabButton (idx, QTabBar::LeftSide));
}

void PlaylistTabs::updateTabText (int idx)
{
    QString title;

    if (! getTabEdit (idx))
    {
        auto playlist = Playlist::by_index (idx);

        // escape ampersands for setTabText ()
        title = QString (playlist.get_title ()).replace ("&", "&&");

        if (aud_get_bool ("qtui", "entry_count_visible"))
            title += QString (" (%1)").arg (playlist.n_entries ());
    }

    setTabText (idx, title);
}

void PlaylistTabs::setupTab (int idx, QWidget * button, QWidget * * oldp)
{
    QWidget * old = m_tabbar->tabButton (idx, QTabBar::LeftSide);
    m_tabbar->setTabButton (idx, QTabBar::LeftSide, button);

    if (oldp)
        * oldp = old;
    else
    {
        old->setParent (nullptr);
        old->deleteLater ();
    }

    updateTabText (idx);
}

void PlaylistTabs::editTab (int idx, Playlist playlist)
{
    QLineEdit * edit = getTabEdit (idx);

    if (! edit)
    {
        edit = new QLineEdit ((const char *) playlist.get_title ());

        connect (edit, & QLineEdit::returnPressed, [this, playlist, edit] ()
        {
            playlist.set_title (edit->text ().toUtf8 ());
            cancelRename ();
        });

        setupTab (idx, edit, & m_leftbtn);
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
            return cancelRename ();
    }

    return QTabWidget::eventFilter(obj, e);
}

void PlaylistTabs::renameCurrent ()
{
    int idx = currentIndex ();
    auto playlist = currentPlaylistWidget ()->playlist ();

    if (! m_tabbar->isVisible ())
        audqt::playlist_show_rename (playlist);
    else
        editTab (idx, playlist);
}

bool PlaylistTabs::cancelRename ()
{
    bool cancelled = false;

    for (int i = 0; i < count (); i ++)
    {
        QLineEdit * edit = getTabEdit (i);
        if (! edit)
            continue;

        setupTab (i, m_leftbtn, nullptr);
        m_leftbtn = nullptr;
        cancelled = true;
    }

    return cancelled;
}

void PlaylistTabs::playlist_activate_cb ()
{
    setCurrentIndex (Playlist::active_playlist ().index ());
    cancelRename ();
}

void PlaylistTabs::playlist_update_cb (Playlist::UpdateLevel global_level)
{
    if (global_level == Playlist::Structure)
        addRemovePlaylists ();
    if (global_level >= Playlist::Metadata)
        updateTitles ();

    for (int i = 0; i < count (); i ++)
        playlistWidget (i)->playlistUpdate ();

    setCurrentIndex (Playlist::active_playlist ().index ());
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
    connect (this, & QTabBar::tabCloseRequested, [] (int idx) {
        audqt::playlist_confirm_delete (Playlist::by_index (idx));
    });
}

void PlaylistTabBar::tabMoved (int from, int to)
{
    Playlist::reorder_playlists (from, to, 1);
}

void PlaylistTabBar::mousePressEvent (QMouseEvent * e)
{
    if (e->button () == Qt::MidButton)
    {
        int index = tabAt (e->pos ());
        if (index >= 0)
        {
            audqt::playlist_confirm_delete (Playlist::by_index (index));
            e->accept ();
        }
    }

    QTabBar::mousePressEvent (e);
}

void PlaylistTabBar::mouseDoubleClickEvent (QMouseEvent * e)
{
    int idx = tabAt (e->pos ());
    if (idx < 0 || e->button () != Qt::LeftButton)
        return;

    Playlist::by_index (idx).start_playback ();
}

void PlaylistTabBar::updateSettings ()
{
#if QT_VERSION >= 0x050400
    setAutoHide (! aud_get_bool ("qtui", "playlist_tabs_visible"));
#endif
    setTabsClosable (aud_get_bool ("qtui", "close_button_visible"));
}
