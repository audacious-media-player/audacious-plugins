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

#include <QtGui>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/playlist.h>
#include <libaudcore/tuple.h>

#include "playlist_model.h"
#include "playlist_model.moc"

PlaylistModel::PlaylistModel (QObject * parent, int id) : QAbstractListModel (parent),
    m_uniqueId (id)
{
    m_rows = aud_playlist_entry_count (playlist ());
}

PlaylistModel::~PlaylistModel ()
{

}

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

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column ())
        {
        case PL_COL_TITLE:
        case PL_COL_ARTIST:
        case PL_COL_ALBUM:
            aud_playlist_entry_describe (playlist (), index.row (), title, artist, album, true);
            break;
        case PL_COL_LENGTH:
            tuple = aud_playlist_entry_get_tuple (playlist (), index.row (), false);
            break;
        }

        switch (index.column ())
        {
        case PL_COL_TITLE:
            return QString (title);
        case PL_COL_ARTIST:
            return QString (artist);
        case PL_COL_ALBUM:
            return QString (album);
        case PL_COL_QUEUED:
            return getQueued (index.row ());
        case PL_COL_LENGTH:
            return QString (str_format_time (tuple.get_int (FIELD_LENGTH)));
        }

    case Qt::TextAlignmentRole:
        switch (index.column ())
        {
        case PL_COL_LENGTH:
            return Qt::AlignRight;
        }

    case Qt::DecorationRole:

        if (index.column () == 0 && index.row () == aud_playlist_get_position (playlist ()))
        {
            if (aud_playlist_get_playing () == playlist ())
                if (aud_drct_get_paused ())
                    return QIcon::fromTheme ("media-playback-pause");
                else
                    return QIcon::fromTheme ("media-playback-start");
            else
                return QIcon::fromTheme ("media-playback-stop");
        }
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
            case PL_COL_TITLE:
                return QString ("Title");
            case PL_COL_ARTIST:
                return QString ("Artist");
            case PL_COL_ALBUM:
                return QString ("Album");
            case PL_COL_QUEUED:
                return QString ("Queued");
            case PL_COL_LENGTH:
                return QString ("Time");
            }
         }
     }
    return QVariant ();
}

int PlaylistModel::playlist () const
{
    return aud_playlist_by_unique_id (m_uniqueId);
}

int PlaylistModel::uniqueId () const
{
    return m_uniqueId;
}

bool PlaylistModel::insertRows (int row, int count, const QModelIndex & parent)
{
    int last = row + count - 1;
    beginInsertRows (parent, row, last);
    m_rows = aud_playlist_entry_count (playlist ());
    endInsertRows ();
    return true;
}

bool PlaylistModel::removeRows (int row, int count, const QModelIndex & parent)
{
    int last = row + count - 1;
    beginRemoveRows (parent, row, last);
    m_rows = aud_playlist_entry_count (playlist ());
    endRemoveRows ();
    return true;
}

void PlaylistModel::updateRows (int row, int count)
{
    int bottom = row + count - 1;
    auto topLeft = createIndex (row, 0);
    auto bottomRight = createIndex (bottom, columnCount () - 1);
    emit dataChanged (topLeft, bottomRight);
}

void PlaylistModel::updateRow (int row)
{
    updateRows (row, 1);
}

QString PlaylistModel::getQueued (int row) const
{
    int at = aud_playlist_queue_find_entry (playlist (), row);
    if (at < 0)
        return QString ("");
    else
        return QString ("#%1").arg (at + 1);
}
