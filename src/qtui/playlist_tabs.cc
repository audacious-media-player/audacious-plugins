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

#include "playlist_tabs.h"
#include "menus.h"
#include "playlist-qt.h"
#include "search_bar.h"
#include "settings.h"

#include <QBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

class LayoutWidget : public QWidget
{
public:
    LayoutWidget(QWidget * parent, Playlist playlist, QMenu * contextMenu);

    PlaylistWidget * playlistWidget() const { return m_playlistWidget; }

    void activateSearch()
    {
        m_searchBar->show();
        // use ShortcutFocusReason to select text in the search entry
        // (the default OtherFocusReason does not select text)
        m_searchBar->setFocus(Qt::ShortcutFocusReason);
    }

private:
    PlaylistWidget * m_playlistWidget;
    SearchBar * m_searchBar;
};

LayoutWidget::LayoutWidget(QWidget * parent, Playlist playlist,
                           QMenu * contextMenu)
    : QWidget(parent), m_playlistWidget(new PlaylistWidget(this, playlist)),
      m_searchBar(new SearchBar(this, m_playlistWidget))
{
    auto layout = audqt::make_vbox(this, 0);
    layout->addWidget(m_playlistWidget);
    layout->addWidget(m_searchBar);

    m_playlistWidget->setContextMenu(contextMenu);
    m_searchBar->hide();
}

/* --------------------------------- */

PlaylistTabs::PlaylistTabs(QWidget * parent)
    : QTabWidget(parent), m_pl_menu(qtui_build_pl_menu(this)),
      m_tabbar(new PlaylistTabBar(this))
{
    installEventFilter(this);

    // set up tab bar
    m_tabbar->setFocusPolicy(Qt::NoFocus);
    setTabBar(m_tabbar);

    addRemovePlaylists();
    m_tabbar->updateTitles();
    m_tabbar->updateIcons();
    setCurrentIndex(Playlist::active_playlist().index());

    connect(this, &QTabWidget::currentChanged, this,
            &PlaylistTabs::currentChangedTrigger);
}

PlaylistWidget * PlaylistTabs::currentPlaylistWidget() const
{
    return ((LayoutWidget *)currentWidget())->playlistWidget();
}

PlaylistWidget * PlaylistTabs::playlistWidget(int idx) const
{
    auto w = (LayoutWidget *)widget(idx);
    return w ? w->playlistWidget() : nullptr;
}

void PlaylistTabs::activateSearch()
{
    ((LayoutWidget *)currentWidget())->activateSearch();
}

void PlaylistTabs::addRemovePlaylists()
{
    int tabs = count();
    int playlists = Playlist::n_playlists();

    for (int i = 0; i < tabs; i++)
    {
        auto w = (LayoutWidget *)widget(i);
        int list_idx = w->playlistWidget()->playlist().index();

        if (list_idx < 0)
        {
            removeTab(i);
            delete w;
            tabs--;
            i--;
        }
        else if (list_idx != i)
        {
            bool found = false;

            for (int j = i + 1; j < tabs; j++)
            {
                w = (LayoutWidget *)widget(j);
                list_idx = w->playlistWidget()->playlist().index();

                if (list_idx == i)
                {
                    removeTab(j);
                    insertTab(i, w, QString());
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                insertTab(
                    i, new LayoutWidget(this, Playlist::by_index(i), m_pl_menu),
                    QString());
                tabs++;
            }
        }
    }

    while (tabs < playlists)
    {
        addTab(new LayoutWidget(this, Playlist::by_index(tabs), m_pl_menu),
               QString());
        tabs++;
    }
}

void PlaylistTabs::currentChangedTrigger(int idx)
{
    if (!m_in_update)
        Playlist::by_index(idx).activate();
}

bool PlaylistTabs::eventFilter(QObject * obj, QEvent * e)
{
    if (e->type() == QEvent::KeyPress)
    {
        QKeyEvent * ke = (QKeyEvent *)e;

        if (ke->key() == Qt::Key_Escape)
            return m_tabbar->cancelRename();
    }

    return QTabWidget::eventFilter(obj, e);
}

void PlaylistTabs::renameCurrent()
{
    auto playlist = currentPlaylistWidget()->playlist();

    if (m_tabbar->isVisible())
        m_tabbar->startRename(playlist);
    else
        audqt::playlist_show_rename(playlist);
}

void PlaylistTabs::playlist_activate_cb()
{
    m_in_update = true;
    setCurrentIndex(Playlist::active_playlist().index());
    m_tabbar->cancelRename();
    m_in_update = false;
}

void PlaylistTabs::playlist_update_cb(Playlist::UpdateLevel global_level)
{
    m_in_update = true;
    if (global_level == Playlist::Structure)
        addRemovePlaylists();
    if (global_level >= Playlist::Metadata)
        m_tabbar->updateTitles();

    for (int i = 0; i < count(); i++)
        playlistWidget(i)->playlistUpdate();

    setCurrentIndex(Playlist::active_playlist().index());
    m_in_update = false;
}

void PlaylistTabs::playlist_position_cb(Playlist list)
{
    auto widget = playlistWidget(list.index());
    if (widget)
        widget->scrollToCurrent();
}

PlaylistTabBar::PlaylistTabBar(QWidget * parent) : QTabBar(parent)
{
    setMovable(true);
    setDocumentMode(true);
    updateSettings();

    connect(this, &QTabBar::tabMoved, this, &PlaylistTabBar::tabMoved);
    connect(this, &QTabBar::tabCloseRequested, [](int idx) {
        audqt::playlist_confirm_delete(Playlist::by_index(idx));
    });
}

void PlaylistTabBar::updateTitles()
{
    int tabs = count();
    for (int i = 0; i < tabs; i++)
        updateTabText(i);
}

void PlaylistTabBar::updateIcons()
{
    QIcon icon;
    int playing = Playlist::playing_playlist().index();
    if (playing >= 0)
        icon = QIcon::fromTheme(aud_drct_get_paused() ? "media-playback-pause"
                                                     : "media-playback-start");

    int tabs = count();
    for (int i = 0; i < tabs; i++)
    {
        /* hide icon when editing so it doesn't get shown on the wrong side */
        setTabIcon(i, (i == playing && !getTabEdit(i)) ? icon : QIcon());
    }
}

void PlaylistTabBar::startRename(Playlist playlist)
{
    int idx = playlist.index();
    QLineEdit * edit = getTabEdit(idx);

    if (!edit)
    {
        edit = new QLineEdit((const char *)playlist.get_title());

        connect(edit, &QLineEdit::returnPressed, [this, playlist, edit]() {
            playlist.set_title(edit->text().toUtf8());
            cancelRename();
        });

        setupTab(idx, edit, &m_leftbtn);
        updateIcons();
    }

    edit->selectAll();
    edit->setFocus();
}

bool PlaylistTabBar::cancelRename()
{
    bool cancelled = false;

    for (int i = 0; i < count(); i++)
    {
        QLineEdit * edit = getTabEdit(i);
        if (!edit)
            continue;

        setupTab(i, m_leftbtn, nullptr);
        m_leftbtn = nullptr;
        cancelled = true;
        updateIcons();
    }

    return cancelled;
}

void PlaylistTabBar::mousePressEvent(QMouseEvent * e)
{
    if (e->button() == Qt::MiddleButton)
    {
        int index = tabAt(e->pos());
        if (index >= 0)
        {
            audqt::playlist_confirm_delete(Playlist::by_index(index));
            e->accept();
        }
    }

    QTabBar::mousePressEvent(e);
}

void PlaylistTabBar::mouseDoubleClickEvent(QMouseEvent * e)
{
    int idx = tabAt(e->pos());
    if (idx < 0 || e->button() != Qt::LeftButton)
        return;

    Playlist::by_index(idx).start_playback();
}

void PlaylistTabBar::contextMenuEvent(QContextMenuEvent * e)
{
    int idx = tabAt(e->pos());
    if (idx < 0)
        return;

    auto menu = new QMenu(this);
    auto playlist = Playlist::by_index(idx);

    auto play_act = new QAction(QIcon::fromTheme("media-playback-start"),
                                audqt::translate_str(N_("_Play")), menu);
    auto rename_act =
        new QAction(QIcon::fromTheme("insert-text"),
                    audqt::translate_str(N_("_Rename ...")), menu);
    auto remove_act = new QAction(QIcon::fromTheme("edit-delete"),
                                  audqt::translate_str(N_("Remo_ve")), menu);

    QObject::connect(play_act, &QAction::triggered,
                     [playlist]() { playlist.start_playback(); });
    QObject::connect(rename_act, &QAction::triggered, [this, playlist]() {
        if (playlist.exists())
            startRename(playlist);
    });
    QObject::connect(remove_act, &QAction::triggered, [playlist]() {
        if (playlist.exists())
            audqt::playlist_confirm_delete(playlist);
    });

    menu->addAction(play_act);
    menu->addAction(rename_act);
    menu->addAction(remove_act);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(e->globalPos());
}

QLineEdit * PlaylistTabBar::getTabEdit(int idx)
{
    return dynamic_cast<QLineEdit *>(tabButton(idx, QTabBar::LeftSide));
}

void PlaylistTabBar::updateTabText(int idx)
{
    QString title;

    if (!getTabEdit(idx))
    {
        auto playlist = Playlist::by_index(idx);

        // escape ampersands for setTabText ()
        title = QString(playlist.get_title()).replace("&", "&&");

        if (aud_get_bool("qtui", "entry_count_visible"))
            title += QString(" (%1)").arg(playlist.n_entries());
    }

    setTabText(idx, title);
}

void PlaylistTabBar::setupTab(int idx, QWidget * button, QWidget ** oldp)
{
    QWidget * old = tabButton(idx, QTabBar::LeftSide);
    setTabButton(idx, QTabBar::LeftSide, button);

    if (oldp)
        *oldp = old;
    else
    {
        old->setParent(nullptr);
        old->deleteLater();
    }

    updateTabText(idx);
}

void PlaylistTabBar::tabMoved(int from, int to)
{
    Playlist::reorder_playlists(from, to, 1);
}

void PlaylistTabBar::updateSettings()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    setAutoHide(false);
#endif

    switch (aud_get_int("qtui", "playlist_tabs_visible"))
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    case PlaylistTabVisibility::AutoHide:
        setAutoHide(true);
        break;
#endif
    case PlaylistTabVisibility::Always:
        show();
        break;

    case PlaylistTabVisibility::Never:
        hide();
        break;
    }

    setTabsClosable(aud_get_bool("qtui", "close_button_visible"));
    updateTitles();
}
