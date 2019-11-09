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
#include <libaudcore/vfs_async.h>

#include <libaudqt/treeview.h>

#include <QWidget>
#include <QTabWidget>
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "ihr-model.h"

IHRMarketModel::IHRMarketModel (QObject * parent) :
    QAbstractListModel (parent)
{
    fetch_markets ();
}

int IHRMarketModel::columnCount (const QModelIndex &) const
{
    return 1;
}

int IHRMarketModel::rowCount (const QModelIndex &) const
{
    return m_results.len ();
}

QVariant IHRMarketModel::headerData (int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    return QString (_("Market"));
}

QVariant IHRMarketModel::data (const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    int row = index.row ();
    auto entry = m_results[row];

    return QString ("%1, %2, %3 (%4)")
           .arg (entry.city)
           .arg (entry.state)
           .arg (entry.country_code)
           .arg (entry.station_count);
}

static const char *URI_GET_MARKETS = "https://api.iheart.com/api/v2/content/markets?limit=10000&cache=true";

void IHRMarketModel::fetch_markets ()
{
    vfs_async_file_get_contents (URI_GET_MARKETS, [&] (const char *, const Index<char> & buf) {
        if (! buf.len ())
            return;

       auto doc = QJsonDocument::fromJson(QByteArray (buf.begin (), buf.len ()));
       if (! doc.isObject ())
           return;

       auto root = doc.object ();
       auto market_count = root["total"].toInt ();

       AUDINFO ("Fetched %d markets.\n", market_count);

       beginResetModel ();

       auto markets = root["hits"].toArray ();

       for (auto market_ref : markets)
       {
           auto market = market_ref.toObject ();
           IHRMarketEntry entry;

           entry.market_id = market["marketId"].toInt ();
           entry.station_count = market["stationCount"].toInt ();
           entry.city = market["city"].toString ();
           entry.state = market["stateAbbreviation"].toString ();
           entry.country_code = market["countryAbbreviation"].toString ();

           m_results.append (entry);
       }

       endResetModel ();
    });
}

int IHRMarketModel::id_for_idx (const QModelIndex &index) const
{
    if (index.row () < 0)
        return -1;

    auto entry = m_results[index.row ()];
    return entry.market_id;
}

IHRTunerModel::IHRTunerModel (QObject * parent) :
    QAbstractListModel (parent)
{
}

int IHRTunerModel::columnCount (const QModelIndex &) const
{
    return NColumns;
}

int IHRTunerModel::rowCount (const QModelIndex &) const
{
    return m_results.len ();
}

QVariant IHRTunerModel::headerData (int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    switch (section) {
    case CallLetters:
        return QString (_("Call Letters"));

    case Title:
        return QString (_("Title"));

    case Description:
        return QString (_("Description"));
    }

    return QVariant ();
}

QVariant IHRTunerModel::data (const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    int row = index.row ();
    int col = index.column ();
    auto entry = m_results[row];

    switch (col) {
    case CallLetters:
        return QString (entry.call_letters);

    case Title:
        return QString (entry.title);

    case Description:
        return QString (entry.description);
    }

    return QVariant ();
}

void IHRTunerModel::fetch_stations (int market_id)
{
    StringBuf uri = str_printf("https://api.iheart.com/api/v2/content/liveStations?limit=100&marketId=%d", market_id);

    vfs_async_file_get_contents(uri, [&] (const char *, const Index<char> & buf) {
        if (! buf.len ())
            return;

       auto doc = QJsonDocument::fromJson(QByteArray (buf.begin (), buf.len ()));
       if (! doc.isObject ())
           return;

       auto root = doc.object ();
       auto station_count = root["total"].toInt ();

       AUDINFO ("Fetched %d stations for market %d.\n", station_count, market_id);

       beginResetModel ();

       m_results.clear ();

       auto stations = root["hits"].toArray ();

       for (auto station_ref : stations)
       {
           auto station = station_ref.toObject ();
           IHRStationEntry entry;

           entry.title = station["name"].toString ();
           entry.description = station["description"].toString ();
           entry.call_letters = station["callLetters"].toString ();

           auto streams = station["streams"].toObject ();
           entry.stream_uri = streams["shoutcast_stream"].toString ();

           m_results.append (entry);
       }

       endResetModel ();
    });
}

const IHRStationEntry & IHRTunerModel::station_for_idx (const QModelIndex &index) const
{
    return m_results[index.row ()];
}
