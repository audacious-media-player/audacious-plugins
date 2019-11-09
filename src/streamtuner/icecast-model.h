// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#ifndef STREAMTUNER_ICECAST_MODEL_H
#define STREAMTUNER_ICECAST_MODEL_H

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
#include <QVBoxLayout>
#include <QSplitter>
#include <QAbstractListModel>

struct IcecastEntry {
    QString title;
    QString genre;
    QString current_song;
    QString stream_uri;

    enum {
        MP3,
        AAC,
        Vorbis,
        Other
    } type;

    int bitrate;
};

class IcecastTunerModel : public QAbstractListModel {
public:
    enum {
        Title,
        Genre,
        Type,
        Bitrate,
        CurrentSong,
        NColumns
    };

    IcecastTunerModel (QObject * parent = nullptr);
    ~IcecastTunerModel ();

    int columnCount (const QModelIndex &parent = QModelIndex()) const;
    int rowCount (const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data (const QModelIndex &index, int role = Qt::DisplayRole) const;

    void fetch_stations ();

    const IcecastEntry & entry (int idx) const;

private:
    Index<IcecastEntry> m_results;
};

#endif
