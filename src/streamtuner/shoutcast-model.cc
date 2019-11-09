// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
#include <libaudcore/index.h>
#include <libaudcore/playlist.h>

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "shoutcast-model.h"

ShoutcastTunerModel::ShoutcastTunerModel (QObject * parent) :
    QAbstractListModel (parent)
{
    m_qnam = new QNetworkAccessManager (this);

    fetch_stations ();
}

ShoutcastTunerModel::~ShoutcastTunerModel ()
{
    m_results.clear ();
}

void ShoutcastTunerModel::fetch_stations (String genre)
{
    auto base = "https://directory.shoutcast.com";
    StringBuf uri;
    StringBuf post_data;

    // undefined genre: fetch top 500
    if (! genre || ! strcmp (genre, "Top 500 Stations"))
        uri = str_concat ({base, "/Home/Top"});
    else
    {
        uri = str_concat ({base, "/Home/BrowseByGenre"});
        post_data = str_concat ({"genrename=", genre});
    }

    // build the request for the fetch
    QUrl url = QUrl (QString (uri));
    QNetworkRequest request = QNetworkRequest (url);

    QNetworkReply * reply = m_qnam->post (request, (const char *) post_data);
    QObject::connect (reply, &QNetworkReply::finished, [reply, this] () {
        if (200 != reply->attribute (QNetworkRequest::HttpStatusCodeAttribute))
            return;

       auto data = reply->readAll();
       auto doc = QJsonDocument::fromJson(data);

       if (! doc.isArray ())
           return;

       auto stations = doc.array ();
       process_stations (stations);
    });
}

void ShoutcastTunerModel::process_station (QJsonObject object)
{
    ShoutcastEntry entry;

    entry.listeners = object["Listeners"].toInt ();
    entry.bitrate = object["Bitrate"].toInt ();
    entry.station_id = object["ID"].toInt ();
    entry.genre = object["Genre"].toString ();
    entry.title = object["Name"].toString ();
    entry.type = object["Format"].toString () == QString ("audio/mpeg") ? ShoutcastEntry::MP3 : ShoutcastEntry::AAC;

    m_results.append (entry);
}

void ShoutcastTunerModel::process_stations (QJsonArray & stations)
{
    AUDINFO ("Retrieved %d stations.\n", stations.size ());

    beginResetModel ();

    m_results.clear ();

    for (auto st : stations)
    {
        if (! st.isObject ())
            continue;

        process_station (st.toObject ());
    }

    endResetModel ();
}

QVariant ShoutcastTunerModel::headerData (int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    switch (section)
    {
    case Title:
        return QString (_("Title"));

    case Genre:
        return QString (_("Genre"));

    case Listeners:
        return QString (_("Listeners"));

    case Type:
        return QString (_("Type"));

    case Bitrate:
        return QString (_("Bitrate"));
    }

    return QVariant ();
}

QVariant ShoutcastTunerModel::data (const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    int row = index.row ();
    if (row > m_results.len ())
        return QVariant ();

    auto entry = m_results[row];

    switch (index.column ())
    {
    case Title:
        return QString (entry.title);

    case Genre:
        return QString (entry.genre);

    case Listeners:
        return QString::number (entry.listeners);

    case Type:
        return entry.type == ShoutcastEntry::MP3 ? QString ("MP3") : QString ("AAC");

    case Bitrate:
        return QString::number (entry.bitrate);
    }

    return QVariant ();
}

int ShoutcastTunerModel::columnCount (const QModelIndex &) const
{
    return NColumns;
}

int ShoutcastTunerModel::rowCount (const QModelIndex &) const
{
    return m_results.len ();
}

const ShoutcastEntry & ShoutcastTunerModel::entry (int idx) const
{
    return m_results[idx];
}

const char *ShoutcastTunerModel::genres[] = {
    N_("Top 500 Stations"),
    N_("Alternative"),
    N_("Blues"),
    N_("Classical"),
    N_("Country"),
    N_("Decades"),
    N_("Easy Listening"),
    N_("Electronic"),
    N_("Folk"),
    N_("Inspirational"),
    N_("International"),
    N_("Jazz"),
    N_("Latin"),
    N_("Metal"),
    N_("Misc"),
    N_("New Age"),
    N_("Pop"),
    N_("Public Radio"),
    N_("R&B and Urban"),
    N_("Rap"),
    N_("Reggae"),
    N_("Rock"),
    N_("Seasonal and Holiday"),
    N_("Soundtracks"),
    N_("Talk"),
    N_("Themes")
};

ShoutcastGenreModel::ShoutcastGenreModel (QObject * parent) :
    QAbstractListModel (parent)
{
}

int ShoutcastGenreModel::columnCount (const QModelIndex &) const
{
    return 1;
}

int ShoutcastGenreModel::rowCount (const QModelIndex &) const
{
    return sizeof (ShoutcastTunerModel::genres) / sizeof (ShoutcastTunerModel::genres[0]);
}

QVariant ShoutcastGenreModel::headerData (int, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    return QString (_("Genre"));
}

QVariant ShoutcastGenreModel::data (const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    return QString (ShoutcastTunerModel::genres[index.row ()]);
}
