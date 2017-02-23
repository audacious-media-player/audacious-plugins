/*
 * menu-ops-qt.cc
 * Copyright 2016 John Lindgren
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

#include "menu-ops.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QMimeData>
#include <QUrl>

#include <libaudcore/audstrings.h>
#include <libaudcore/playlist.h>
#include <libaudqt/libaudqt.h>

void pl_copy ()
{
    auto list = Playlist::active_playlist ();
    int entries = list.n_entries ();

    if (! list.n_selected ())
        return;

    list.cache_selected ();

    QList<QUrl> urls;
    for (int i = 0; i < entries; i ++)
    {
        if (list.entry_selected (i))
            urls.append (QString (list.entry_filename (i)));
    }

    auto data = new QMimeData;
    data->setUrls (urls);
    QApplication::clipboard ()->setMimeData (data);
}

void pl_cut ()
{
    pl_copy ();
    pl_remove_selected ();
}

static void paste_to (Playlist list, int pos)
{
    auto data = QApplication::clipboard ()->mimeData ();
    if (! data->hasUrls ())
        return;

    Index<PlaylistAddItem> items;
    for (auto & url : data->urls ())
        items.append (String (url.toEncoded ()));

    list.insert_items (pos, std::move (items), false);
}

void pl_paste ()
{
    auto list = Playlist::active_playlist ();
    paste_to (list, list.get_focus ());
}

void pl_paste_end ()
{
    paste_to (Playlist::active_playlist (), -1);
}

void pl_song_info ()
{
    auto list = Playlist::active_playlist ();
    int focus = list.get_focus ();
    if (focus >= 0)
        audqt::infowin_show (list, focus);
}

void pl_open_folder ()
{
    auto list = Playlist::active_playlist ();
    int focus = list.get_focus ();

    String filename = list.entry_filename (focus);
    if (! filename)
        return;

    const char * slash = strrchr (filename, '/');
    if (! slash)
        return;

    /* don't trim trailing slash, it may be important */
    QDesktopServices::openUrl (QString::fromUtf8 (filename, slash + 1 - filename));
}
