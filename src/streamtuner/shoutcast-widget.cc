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

ShoutcastTunerWidget::ShoutcastTunerWidget (QWidget * parent) :
    audqt::TreeView (parent)
{
    m_model = new ShoutcastTunerModel ();

    setModel (m_model);
    setRootIsDecorated (false);
}

void ShoutcastTunerWidget::activate (const QModelIndex & index)
{
    if (index.row () < 0)
        return;

    Playlist::temporary_playlist ().activate ();
    auto entry = m_model->entry (index.row ());

    AUDINFO ("Play radio entry %s [%d].\n", (const char *) entry.title, entry.station_id);

    StringBuf playlist_uri = str_printf ("https://yp.shoutcast.com/sbin/tunein-station.m3u?id=%d", entry.station_id);

    Playlist::temporary_playlist ().insert_entry (-1, playlist_uri, Tuple (), true);
}
