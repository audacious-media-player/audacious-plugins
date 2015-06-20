/*
 * playlist-manager-qt.cc
 * Copyright 2015 John Lindgren
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

#include <QAbstractListModel>
#include <QHeaderView>
#include <QTreeView>

#define AUD_PLUGIN_QT_ONLY
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>

class PlaylistManagerQt : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Playlist Manager"),
        PACKAGE
    };

    constexpr PlaylistManagerQt () : GeneralPlugin (info, false) {}

    void * get_qt_widget ();
};

EXPORT PlaylistManagerQt aud_plugin_instance;

class PlaylistsModel : public QAbstractListModel
{
public:
    enum {
        ColumnTitle,
        ColumnEntries,
        NColumns
    };

    PlaylistsModel () :
        m_rows (aud_playlist_count ()) {}

    void update (Playlist::UpdateLevel level);

protected:
    int rowCount (const QModelIndex & parent) const { return m_rows; }
    int columnCount (const QModelIndex & parent) const { return NColumns; }

    QVariant data (const QModelIndex & index, int role) const;
    QVariant headerData (int section, Qt::Orientation orientation, int role) const;

private:
    int m_rows;
};

class PlaylistsView : public QTreeView
{
public:
    PlaylistsView ();

protected:
    void currentChanged (const QModelIndex & current, const QModelIndex & previous);

private:
    PlaylistsModel m_model;

    void update (Playlist::UpdateLevel level);
    void update_sel ();

    const HookReceiver<PlaylistsView, Playlist::UpdateLevel>
     update_hook {"playlist update", this, & PlaylistsView::update};
    const HookReceiver<PlaylistsView>
     activate_hook {"playlist activate", this, & PlaylistsView::update_sel};

    int m_in_update = 0;
};

QVariant PlaylistsModel::data (const QModelIndex & index, int role) const
{
    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column ())
        {
        case ColumnTitle:
            return QString (aud_playlist_get_title (index.row ()));
        case ColumnEntries:
            return aud_playlist_entry_count (index.row ());
        }

    case Qt::TextAlignmentRole:
        if (index.column () == ColumnEntries)
            return Qt::AlignRight;
    }

    return QVariant ();
}

QVariant PlaylistsModel::headerData (int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case ColumnTitle:
            return QString (_("Title"));
        case ColumnEntries:
            return QString (_("Entries"));
        }
    }

    return QVariant ();
}

void PlaylistsModel::update (const Playlist::UpdateLevel level)
{
    int rows = aud_playlist_count ();

    if (level == Playlist::Structure)
    {
        if (rows < m_rows)
        {
            beginRemoveRows (QModelIndex (), rows, m_rows - 1);
            m_rows = rows;
            endRemoveRows ();
        }
        else if (rows > m_rows)
        {
            beginInsertRows (QModelIndex (), m_rows, rows - 1);
            m_rows = rows;
            endInsertRows ();
        }
    }

    if (level >= Playlist::Metadata)
    {
        auto topLeft = createIndex (0, 0);
        auto bottomRight = createIndex (m_rows - 1, NColumns - 1);
        emit dataChanged (topLeft, bottomRight);
    }
}

PlaylistsView::PlaylistsView ()
{
    m_in_update ++;
    setModel (& m_model);
    update_sel ();
    m_in_update --;

    auto hdr = header ();
    hdr->setStretchLastSection (false);
    hdr->setSectionResizeMode (PlaylistsModel::ColumnTitle, QHeaderView::Stretch);
    hdr->setSectionResizeMode (PlaylistsModel::ColumnEntries, QHeaderView::Interactive);
    hdr->resizeSection (PlaylistsModel::ColumnEntries, 64);

    setIndentation (0);
}

void PlaylistsView::currentChanged (const QModelIndex & current, const QModelIndex & previous)
{
    QTreeView::currentChanged (current, previous);
    if (! m_in_update)
        aud_playlist_set_active (current.row ());
}

void PlaylistsView::update (Playlist::UpdateLevel level)
{
    m_in_update ++;
    m_model.update (level);
    update_sel ();
    m_in_update --;
}

void PlaylistsView::update_sel ()
{
    if (aud_playlist_update_pending ())
        return;

    m_in_update ++;
    auto sel = selectionModel ();
    auto current = m_model.index (aud_playlist_get_active (), 0);
    sel->setCurrentIndex (current, sel->SelectCurrent | sel->Rows);
    m_in_update --;
}

void * PlaylistManagerQt::get_qt_widget ()
{
    return new PlaylistsView;
}
