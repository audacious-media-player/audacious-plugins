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

#include "shoutcast-model.h"
#include "shoutcast-widget.h"

#include <QHeaderView>
#include <QScrollBar>

ShoutcastListingWidget::ShoutcastListingWidget (QWidget * parent) :
    audqt::TreeView (parent)
{
    m_model = new ShoutcastTunerModel (this);

    setModel (m_model);
    setRootIsDecorated (false);

    auto hdr = header ();
    hdr->setStretchLastSection (false);
    hdr->setSectionResizeMode (0, QHeaderView::Stretch);
}

void ShoutcastListingWidget::activate (const QModelIndex & index)
{
    if (index.row () < 0)
        return;

    Playlist::temporary_playlist ().activate ();
    auto entry = m_model->entry (index.row ());

    AUDINFO ("Play radio entry %s [%d].\n", (const char *) entry.title.toLocal8Bit (), entry.station_id);

    StringBuf playlist_uri = str_printf ("https://yp.shoutcast.com/sbin/tunein-station.m3u?id=%d", entry.station_id);

    Playlist::temporary_playlist ().insert_entry (-1, playlist_uri, Tuple (), true);
}

ShoutcastGenreWidget::ShoutcastGenreWidget (QWidget * parent) :
    QTreeView (parent)
{
    m_model = new ShoutcastGenreModel ();

    setModel (m_model);
    setRootIsDecorated (false);

    auto idx = m_model->index (0);
    auto selection = selectionModel ();
    selection->select (idx, QItemSelectionModel::Select);

    auto hdr = header ();
    hdr->setStretchLastSection (false);
    hdr->setSectionResizeMode (QHeaderView::ResizeToContents);

    int w = 0;
    for (int i=0; i < m_model->columnCount (); i++)
    {
        resizeColumnToContents (i);
        w += columnWidth (i);
    }

    setFixedWidth (w + verticalScrollBar ()->width () + 2);
}

ShoutcastTunerWidget::ShoutcastTunerWidget (QWidget * parent) :
    QWidget (parent)
{
    m_layout = new QHBoxLayout (this);

    m_genre = new ShoutcastGenreWidget ();
    m_layout->addWidget (m_genre);

    m_tuner = new ShoutcastListingWidget ();
    m_layout->addWidget (m_tuner);

    auto genre_selection_model = m_genre->selectionModel ();
    connect(genre_selection_model, &QItemSelectionModel::selectionChanged, [&] (const QItemSelection &selected, const QItemSelection &) {
        // this should never happen, but just to be sure...
        if (! selected.indexes ().length ())
            return;

        auto idx = selected.indexes ().first ();

        ShoutcastTunerModel *model = (ShoutcastTunerModel *) m_tuner->model ();
        model->fetch_stations (String (ShoutcastTunerModel::genres [idx.row ()]));
    });

    connect(m_tuner->model (), &QAbstractItemModel::modelReset, [&] () {
        auto model = m_tuner->model ();
        m_tuner->scrollTo (model->index (0,0));
        for (int i=1; i < model->columnCount (); i++)
            m_tuner->resizeColumnToContents (i);
    });
}
