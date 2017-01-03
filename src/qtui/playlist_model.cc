/*
 * playlist_model.cc
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

#include <QApplication>
#include <QIcon>
#include <QMimeData>
#include <QUrl>

#include <libaudcore/i18n.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/playlist.h>
#include <libaudcore/tuple.h>

#include "playlist_columns.h"
#include "playlist_model.h"

static inline QPixmap get_icon (const char * name)
{
    qreal r = qApp->devicePixelRatio ();
    QPixmap pm = QIcon::fromTheme (name).pixmap (16 * r);
    pm.setDevicePixelRatio (r);
    return pm;
}

PlaylistModel::PlaylistModel (QObject * parent, int uniqueID) :
    QAbstractListModel (parent),
    m_uniqueID (uniqueID)
{
    m_rows = aud_playlist_entry_count (playlist ());
}

PlaylistModel::~PlaylistModel () {}

int PlaylistModel::rowCount (const QModelIndex & parent) const
{
    return m_rows;
}

int PlaylistModel::columnCount (const QModelIndex & parent) const
{
    return PL_COLS;
}

QVariant PlaylistModel::data (const QModelIndex &index, int role) const
{
    String title, artist, album;
    Tuple tuple;
    int length;

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column ())
        {
        case PL_COL_ALBUM:
        case PL_COL_ALBUM_ARTIST:
        case PL_COL_ARTIST:
        case PL_COL_BITRATE:
        case PL_COL_COMMENT:
        case PL_COL_CUSTOM:
        case PL_COL_FILENAME:
        case PL_COL_GENRE:
        case PL_COL_LENGTH:
        case PL_COL_PATH:
        case PL_COL_TITLE:
        case PL_COL_TRACK:
        case PL_COL_YEAR:
            tuple = aud_playlist_entry_get_tuple (playlist (), index.row (), Playlist::NoWait);
            break;
        }

        switch (index.column ())
        {
        case PL_COL_TITLE:
            return QString (tuple.get_str (Tuple::Title));
        case PL_COL_ARTIST:
            return QString (tuple.get_str (Tuple::Artist));
        case PL_COL_ALBUM:
            return QString (tuple.get_str (Tuple::Album));
        case PL_COL_QUEUED:
            return getQueued (index.row ());
        case PL_COL_LENGTH:
            length = tuple.get_int (Tuple::Length);
            return QString (length >=0 ? str_format_time (length) : "" );
        case PL_COL_BITRATE:
            return QString ("%1 kbps").arg (tuple.get_int (Tuple::Bitrate));
        case PL_COL_NUMBER:
            return QString ("%1").arg (index.row () + 1);
        case PL_COL_YEAR:
            return QString ("%1").arg (tuple.get_int (Tuple::Year));
        case PL_COL_ALBUM_ARTIST:
            return QString (tuple.get_str (Tuple::AlbumArtist));
        case PL_COL_TRACK:
            return QString ("%1").arg (tuple.get_int (Tuple::Track));
        case PL_COL_GENRE:
            return QString (tuple.get_str (Tuple::Genre));
        case PL_COL_PATH:
            return QString (tuple.get_str (Tuple::Path));
        case PL_COL_FILENAME:
            return QString (tuple.get_str (Tuple::Basename));
        case PL_COL_CUSTOM:
            return QString (tuple.get_str (Tuple::FormattedTitle));
        case PL_COL_COMMENT:
            return QString (tuple.get_str (Tuple::Comment));
            break;
        }
        break;

    case Qt::TextAlignmentRole:
        switch (index.column ())
        {
        case PL_COL_NOW_PLAYING:
            return Qt::AlignCenter;
        case PL_COL_LENGTH:
            return Qt::AlignRight + Qt::AlignVCenter;
        default:
            return Qt::AlignLeft + Qt::AlignVCenter;
        }
        break;

    case Qt::DecorationRole:
        if (index.column () == 0 && index.row () == aud_playlist_get_position (playlist ()))
        {
            if (aud_playlist_get_playing () == playlist ())
                if (aud_drct_get_paused ())
                    return get_icon ("media-playback-pause");
                else
                    return get_icon ("media-playback-start");
            else
                return get_icon ("media-playback-stop");
        }
        break;
    }
    return QVariant ();
}

QVariant PlaylistModel::headerData (int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section)
            {
            case PL_COL_NOW_PLAYING:
            case PL_COL_QUEUED:
            case PL_COL_NUMBER:
                return QString ();
            }

            return QString (_(pl_col_names[section]));
        }
    }
    return QVariant ();
}

Qt::DropActions PlaylistModel::supportedDropActions () const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlaylistModel::flags (const QModelIndex & index) const
{
    if (index.isValid ())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
}

QStringList PlaylistModel::mimeTypes () const
{
    return QStringList ("text/uri-list");
}

QMimeData * PlaylistModel::mimeData (const QModelIndexList & indexes) const
{
    int list = playlist ();

    /* we assume that <indexes> contains the selected entries */
    aud_playlist_cache_selected (list);

    QList<QUrl> urls;
    int prev = -1;

    for (auto & index : indexes)
    {
        int row = index.row ();
        if (row != prev)  /* skip multiple cells in same row */
        {
            urls.append (QString (aud_playlist_entry_get_filename (list, row)));
            prev = row;
        }
    }

    auto data = new QMimeData;
    data->setUrls (urls);
    return data;
}

bool PlaylistModel::dropMimeData (const QMimeData * data, Qt::DropAction action,
 int row, int column, const QModelIndex & parent)
{
    if (action != Qt::CopyAction || ! data->hasUrls ())
        return false;

    Index<PlaylistAddItem> items;
    for (auto & url : data->urls ())
        items.append (String (url.toEncoded ()));

    aud_playlist_entry_insert_batch (playlist (), row, std::move (items), false);
    return true;
}

int PlaylistModel::playlist () const
{
    return aud_playlist_by_unique_id (m_uniqueID);
}

int PlaylistModel::uniqueId () const
{
    return m_uniqueID;
}

void PlaylistModel::entriesAdded (int row, int count)
{
    if (count < 1)
        return;

    int last = row + count - 1;
    beginInsertRows (QModelIndex (), row, last);
    m_rows += count;
    endInsertRows ();
}

void PlaylistModel::entriesRemoved (int row, int count)
{
    if (count < 1)
        return;

    int last = row + count - 1;
    beginRemoveRows (QModelIndex (), row, last);
    m_rows -= count;
    endRemoveRows ();
}

void PlaylistModel::entriesChanged (int row, int count)
{
    if (count < 1)
        return;

    int bottom = row + count - 1;
    auto topLeft = createIndex (row, 0);
    auto bottomRight = createIndex (bottom, columnCount () - 1);
    emit dataChanged (topLeft, bottomRight);
}

QString PlaylistModel::getQueued (int row) const
{
    int at = aud_playlist_queue_find_entry (playlist (), row);
    if (at < 0)
        return QString ();
    else
        return QString ("#%1").arg (at + 1);
}

/* ---------------------------------- */

void PlaylistProxyModel::setFilter (const char * filter)
{
    m_searchTerms = str_list_to_index (filter, " ");
    invalidateFilter ();
}

bool PlaylistProxyModel::filterAcceptsRow (int source_row, const QModelIndex &) const
{
    if (! m_searchTerms.len ())
        return true;

    int list = aud_playlist_by_unique_id (m_uniqueID);
    Tuple tuple = aud_playlist_entry_get_tuple (list, source_row);

    String strings[] = {
        tuple.get_str (Tuple::Title),
        tuple.get_str (Tuple::Artist),
        tuple.get_str (Tuple::Album)
    };

    for (auto & term : m_searchTerms)
    {
        bool found = false;

        for (auto & s : strings)
        {
            if (s && strstr_nocase_utf8 (s, term))
            {
                found = true;
                break;
            }
        }

        if (! found)
            return false;
    }

    return true;
}
