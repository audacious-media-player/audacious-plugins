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

#include "playlist_model.h"

const char * const PlaylistModel::labels[] = {
    N_("Now Playing"),
    N_("Entry number"),
    N_("Title"),
    N_("Artist"),
    N_("Year"),
    N_("Album"),
    N_("Album artist"),
    N_("Track"),
    N_("Genre"),
    N_("Queue position"),
    N_("Length"),
    N_("File path"),
    N_("File name"),
    N_("Custom title"),
    N_("Bitrate"),
    N_("Comment")
};

static_assert (aud::n_elems (PlaylistModel::labels) == PlaylistModel::n_cols, "update PlaylistModel::labels");

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
    return 1 + n_cols;
}

QVariant PlaylistModel::alignment (int col) const
{
    switch (col)
    {
    case NowPlaying:
        return Qt::AlignCenter;
    case Length:
        return Qt::AlignRight + Qt::AlignVCenter;
    default:
        return Qt::AlignLeft + Qt::AlignVCenter;
    }
}

QVariant PlaylistModel::data (const QModelIndex &index, int role) const
{
    int col = index.column () - 1;
    if (col < 0 || col > n_cols)
        return QVariant ();

    Tuple tuple;
    int length;

    switch (role)
    {
    case Qt::DisplayRole:
        switch (col)
        {
        case Album:
        case AlbumArtist:
        case Artist:
        case Bitrate:
        case Comment:
        case CustomTitle:
        case Filename:
        case Genre:
        case Length:
        case Path:
        case Title:
        case Track:
        case Year:
            tuple = aud_playlist_entry_get_tuple (playlist (), index.row (), Playlist::NoWait);
            break;
        }

        switch (col)
        {
        case Title:
            return QString (tuple.get_str (Tuple::Title));
        case Artist:
            return QString (tuple.get_str (Tuple::Artist));
        case Album:
            return QString (tuple.get_str (Tuple::Album));
        case QueuePos:
            return queuePos (index.row ());
        case Length:
            length = tuple.get_int (Tuple::Length);
            return QString (length >= 0 ? (const char *) str_format_time (length) : "");
        case Bitrate:
            return QString ("%1 kbps").arg (tuple.get_int (Tuple::Bitrate));
        case EntryNumber:
            return QString ("%1").arg (index.row () + 1);
        case Year:
            return QString ("%1").arg (tuple.get_int (Tuple::Year));
        case AlbumArtist:
            return QString (tuple.get_str (Tuple::AlbumArtist));
        case Track:
            return QString ("%1").arg (tuple.get_int (Tuple::Track));
        case Genre:
            return QString (tuple.get_str (Tuple::Genre));
        case Path:
            return QString (tuple.get_str (Tuple::Path));
        case Filename:
            return QString (tuple.get_str (Tuple::Basename));
        case CustomTitle:
            return QString (tuple.get_str (Tuple::FormattedTitle));
        case Comment:
            return QString (tuple.get_str (Tuple::Comment));
        }
        break;

    case Qt::TextAlignmentRole:
        return alignment (col);

    case Qt::DecorationRole:
        if (col == NowPlaying && index.row () == aud_playlist_get_position (playlist ()))
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
    if (orientation != Qt::Horizontal)
        return QVariant ();

    int col = section - 1;
    if (col < 0 || col > n_cols)
        return QVariant ();

    switch (role)
    {
    case Qt::DisplayRole:
        switch (col)
        {
        case NowPlaying:
        case EntryNumber:
        case QueuePos:
            return QVariant ();
        }

        return QString (_(labels[col]));

    case Qt::TextAlignmentRole:
        return alignment (col);

    default:
        return QVariant ();
    }
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

QString PlaylistModel::queuePos (int row) const
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
