/*
 * playlist_selection.h
 * Copyright 2015 John Lindgren
 * Copyright 2020 Ariadne Conill
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

#ifndef PLAYLIST_SELECTION_H

#include <QAbstractListModel>
#include <QBoxLayout>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QMouseEvent>
#include <QToolButton>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/treeview.h>

namespace Moonstone {

class PlaylistsModel : public QAbstractListModel
{
public:
    enum {
        ColumnTitle,
        ColumnEntries,
        NColumns
    };

    PlaylistsModel () :
        m_rows (Playlist::n_playlists ()),
        m_playing (Playlist::playing_playlist ().index ())
    {
    }

    void setFont (const QFont & font)
    {
        m_bold = font;
        m_bold.setBold (true);

        if (m_playing >= 0)
            update_rows (m_playing, 1);
    }

    void update (Playlist::UpdateLevel level);

protected:
    int rowCount (const QModelIndex & parent) const { return m_rows; }
    int columnCount (const QModelIndex & parent) const { return NColumns; }

    Qt::DropActions supportedDropActions () const { return Qt::MoveAction; }

    Qt::ItemFlags flags (const QModelIndex & index) const
    {
        if (index.isValid ())
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        else
            return Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
    }

    QVariant data (const QModelIndex & index, int role) const;
    QVariant headerData (int section, Qt::Orientation orientation, int role) const;

private:
    void update_rows (int row, int count);
    void update_playing ();

    const HookReceiver<PlaylistsModel>
     activate_hook {"playlist set playing", this, & PlaylistsModel::update_playing};

    int m_rows, m_playing;
    QFont m_bold;
};

class PlaylistsView : public audqt::TreeView
{
public:
    PlaylistsView ();

protected:
    void changeEvent (QEvent * event) override
    {
        if (event->type () == QEvent::FontChange)
            m_model.setFont (font ());

        audqt::TreeView::changeEvent (event);
    }

    void currentChanged (const QModelIndex & current, const QModelIndex & previous) override;
    void dropEvent (QDropEvent * event) override;

private:
    PlaylistsModel m_model;

    void update (Playlist::UpdateLevel level);
    void update_sel ();

    void activate (const QModelIndex & index) override
    {
        if (index.isValid ())
            Playlist::by_index (index.row ()).start_playback ();
    }

    const HookReceiver<PlaylistsView, Playlist::UpdateLevel>
     update_hook {"playlist update", this, & PlaylistsView::update};
    const HookReceiver<PlaylistsView>
     activate_hook {"playlist activate", this, & PlaylistsView::update_sel};

    int m_in_update = 0;
};

}

#endif
