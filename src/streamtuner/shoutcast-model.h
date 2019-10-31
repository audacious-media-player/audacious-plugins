// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#ifndef STREAMTUNER_SHOUTCAST_MODEL_H
#define STREAMTUNER_SHOUTCAST_MODEL_H

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

struct ShoutcastEntry {
    String title;
    String genre;
    int listeners;
    String type;
    int bitrate;
    int station_id;
};

class ShoutcastTunerModel : public QAbstractListModel {
public:
    static const char *genres[];

    enum {
        Title,
        Genre,
        Listeners,
        Type,
        Bitrate,
        NColumns
    };

    ShoutcastTunerModel (QObject * parent = nullptr);

    int columnCount (const QModelIndex &parent = QModelIndex()) const;
    int rowCount (const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data (const QModelIndex &index, int role = Qt::DisplayRole) const;

    void fetch_stations (String genre = String ());

    void process_station (QJsonObject object);
    void process_stations (QJsonArray & stations);

    const ShoutcastEntry & entry (int idx) const;

private:
    Index<ShoutcastEntry> m_results;
    QNetworkAccessManager *m_qnam;
};

#endif
