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
#include <libaudcore/playlist.h>
#include <libaudcore/tuple.h>

#include "playlist_model.h"
#include "playlist_model.moc"

PlaylistModel::PlaylistModel (QObject * parent) : QAbstractListModel (parent)
{

}

PlaylistModel::~PlaylistModel ()
{

}

int PlaylistModel::rowCount (const QModelIndex & parent) const
{
   return aud_playlist_entry_count (playlist ());
}

int PlaylistModel::columnCount (const QModelIndex & parent) const
{
    return 5;
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
        case 1:
        case 2:
        case 3:
            aud_playlist_entry_describe (playlist (), index.row (), title, artist, album, true);
            break;
        case 4:
            tuple = aud_playlist_entry_get_tuple (playlist (), index.row (), false);
            break;
        }

        switch (index.column ())
        {
        case 1:
            return QString (title);
        case 2:
            return QString (artist);
        case 3:
            return QString (album);
        case 4:
            return QString (str_format_time (tuple.get_int (FIELD_LENGTH)));
        }

    case Qt::TextAlignmentRole:
        switch (index.column ())
        {
        case 4:
            return Qt::AlignRight;
        }

    case Qt::DecorationRole:

        if (index.column () == 0 && index.row () == aud_playlist_get_position (playlist ()))
        {
            if (aud_playlist_get_playing () == playlist ())
                return QIcon::fromTheme ("media-playback-start");
            else
                return QIcon::fromTheme ("media-playback-pause");
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
            case 0:
                return QString ("Playing");
            case 1:
                return QString ("Title");
            case 2:
                return QString ("Artist");
            case 3:
                return QString ("Album");
            }
         }
     }
    return QVariant ();
}

void PlaylistModel::setUniqueId (int id)
{
    uniqueId = id;
}

int PlaylistModel::playlist () const
{
    return aud_playlist_by_unique_id (uniqueId);
}
