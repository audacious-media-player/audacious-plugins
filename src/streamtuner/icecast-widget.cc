// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#include "icecast-widget.h"

IcecastListingWidget::IcecastListingWidget (QWidget * parent) :
    audqt::TreeView (parent)
{
    m_model = new IcecastTunerModel ();

    setModel (m_model);
    setRootIsDecorated (false);
}

void IcecastListingWidget::activate (const QModelIndex & index)
{
    if (index.row () < 0)
        return;

    Playlist::temporary_playlist ().activate ();
    auto entry = m_model->entry (index.row ());

    AUDINFO ("Play radio entry %s [%s].\n", (const char *) entry.title, (const char *) entry.stream_uri);

    Playlist::temporary_playlist ().insert_entry (-1, entry.stream_uri, Tuple (), true);
}
