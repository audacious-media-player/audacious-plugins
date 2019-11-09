// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#include <QXmlStreamReader>

#include "icecast-model.h"

static const char *ICECAST_YP = "http://dir.xiph.org/yp.xml";

IcecastTunerModel::IcecastTunerModel (QObject * parent) :
    QAbstractListModel (parent)
{
    fetch_stations ();
}

IcecastTunerModel::~IcecastTunerModel ()
{
    m_results.clear ();
}

void IcecastTunerModel::fetch_stations ()
{
    vfs_async_file_get_contents(ICECAST_YP, [&] (const char *, const Index<char> & buf) {
        if (! buf.len ())
            return;

        beginResetModel ();

        AUDINFO("icecast: got results from YP server\n");

        // oh, this is going to be fun... parse the XML as fast as possible
        QXmlStreamReader reader (QByteArray (buf.begin (), buf.len ()));
        IcecastEntry entry;

        // lets prefab some atoms for fast comparisons
        QString entry_atom = QString ("entry");
        QString server_name_atom = QString ("server_name");
        QString listen_url_atom = QString ("listen_url");
        QString server_type_atom = QString ("server_type");
        QString bitrate_atom = QString ("bitrate");
        QString genre_atom = QString ("genre");
        QString current_song_atom = QString ("current_song");
        QString mp3_atom = QString ("audio/mpeg");
        QString aac_atom = QString ("audio/aacp");
        QString vorbis_atom = QString ("application/ogg");

        while (! reader.atEnd ()) {
            auto token_type = reader.readNext ();

            switch (token_type) {
            case QXmlStreamReader::StartElement:
                if (! reader.name ().compare (server_name_atom))
                    entry.title = reader.readElementText ();
                else if (! reader.name ().compare (listen_url_atom))
                    entry.stream_uri = reader.readElementText ();
                else if (! reader.name ().compare (current_song_atom))
                    entry.current_song = reader.readElementText ();
                else if (! reader.name ().compare (genre_atom))
                    entry.genre = reader.readElementText ();
                else if (! reader.name ().compare (server_type_atom))
                {
                    auto server_type = reader.readElementText ();

                    if (! server_type.compare (mp3_atom))
                        entry.type = IcecastEntry::MP3;
                    else if (! server_type.compare (aac_atom))
                        entry.type = IcecastEntry::AAC;
                    else if (! server_type.compare (vorbis_atom))
                        entry.type = IcecastEntry::Vorbis;
                    else
                        entry.type = IcecastEntry::Other;
                }
                else if (! reader.name ().compare (bitrate_atom))
                    entry.bitrate = reader.readElementText ().toInt ();

                break;
            case QXmlStreamReader::EndElement:
                if (! reader.name ().compare (entry_atom))
                    m_results.append (entry);

                break;
            default:
                break;
            }
        }

        endResetModel ();
    });
}

const IcecastEntry & IcecastTunerModel::entry (int idx) const
{
    return m_results[idx];
}

int IcecastTunerModel::columnCount (const QModelIndex &) const
{
    return NColumns;
}

int IcecastTunerModel::rowCount (const QModelIndex &) const
{
    return m_results.len ();
}

QVariant IcecastTunerModel::headerData (int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    switch (section) {
    case Title:
        return QString (_("Title"));

    case Genre:
        return QString (_("Genre"));

    case Type:
        return QString (_("Type"));

    case Bitrate:
        return QString (_("Bitrate"));

    case CurrentSong:
        return QString (_("Current Song"));
    }

    return QVariant ();
}

QVariant IcecastTunerModel::data (const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    int row = index.row ();
    auto station = entry (row);

    switch (index.column ())
    {
    case Title:
        return QString (station.title);

    case Genre:
        return QString (station.genre);

    case Type:
        switch (station.type)
        {
        case IcecastEntry::MP3:
            return QString ("MP3");
        case IcecastEntry::AAC:
            return QString ("AAC");
        case IcecastEntry::Vorbis:
            return QString ("OGG");
        default:
            return QString (_("Other"));
        }
        break;

    case Bitrate:
        return QString::number (station.bitrate);

    case CurrentSong:
        return QString (station.current_song);
    }

    return QVariant ();
}
