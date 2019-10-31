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

#include "ihr-model.h"
#include "ihr-widget.h"

IHRListingWidget::IHRListingWidget (QWidget * parent) :
    audqt::TreeView (parent)
{
    m_model = new IHRTunerModel ();

    setModel (m_model);
    setRootIsDecorated (false);
}

void IHRListingWidget::activate (const QModelIndex & index)
{
    if (index.row () < 0)
        return;

    Playlist::temporary_playlist ().activate ();
    auto entry = m_model->station_for_idx (index);

    AUDINFO ("Play radio entry %s [%s].\n", (const char *) entry.title, (const char *) entry.stream_uri);

    Playlist::temporary_playlist ().insert_entry (-1, entry.stream_uri, Tuple (), true);
}

IHRMarketWidget::IHRMarketWidget (QWidget * parent) :
    QTreeView (parent)
{
    m_model = new IHRMarketModel ();

    setModel (m_model);
    setRootIsDecorated (false);
}

IHRTunerWidget::IHRTunerWidget (QWidget * parent) :
    QWidget (parent)
{
    m_layout = new QVBoxLayout (this);

    m_splitter = new QSplitter ();

    m_markets = new IHRMarketWidget ();
    m_splitter->addWidget (m_markets);

    m_tuner = new IHRListingWidget ();
    m_splitter->addWidget (m_tuner);
    m_splitter->setStretchFactor (1, 2);

    m_layout->addWidget (m_splitter);

    auto market_selection_model = m_markets->selectionModel ();
    connect(market_selection_model, &QItemSelectionModel::selectionChanged, [&] (const QItemSelection &selected, const QItemSelection &) {
        // this should never happen, but just to be sure...
        if (! selected.indexes ().length ())
            return;

        auto idx = selected.indexes ().first ();
        IHRMarketModel *market = (IHRMarketModel *) m_markets->model ();

        IHRTunerModel *model = (IHRTunerModel *) m_tuner->model ();
        model->fetch_stations (market->id_for_idx (idx));
    });
}
