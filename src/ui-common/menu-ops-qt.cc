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
    int list = aud_playlist_get_active ();
    int entries = aud_playlist_entry_count (list);

    if (! aud_playlist_selected_count (list))
        return;

    aud_playlist_cache_selected (list);

    QList<QUrl> urls;
    for (int i = 0; i < entries; i ++)
    {
        if (aud_playlist_entry_get_selected (list, i))
            urls.append (QString (aud_playlist_entry_get_filename (list, i)));
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

static void paste_to (int list, int pos)
{
    auto data = QApplication::clipboard ()->mimeData ();
    if (! data->hasUrls ())
        return;

    Index<PlaylistAddItem> items;
    for (auto & url : data->urls ())
        items.append (String (url.toEncoded ()));

    aud_playlist_entry_insert_batch (list, pos, std::move (items), false);
}

void pl_paste ()
{
    int list = aud_playlist_get_active ();
    paste_to (list, aud_playlist_get_focus (list));
}

void pl_paste_end ()
{
    paste_to (aud_playlist_get_active (), -1);
}

void pl_song_info ()
{
    int list = aud_playlist_get_active ();
    int focus = aud_playlist_get_focus (list);
    if (focus >= 0)
        audqt::infowin_show (list, focus);
}

void pl_open_folder ()
{
    int list = aud_playlist_get_active ();
    int focus = aud_playlist_get_focus (list);

    String filename = aud_playlist_entry_get_filename (list, focus);
    if (! filename)
        return;

    const char * slash = strrchr (filename, '/');
    if (! slash)
        return;

    /* don't trim trailing slash, it may be important */
    StringBuf folder = str_copy (filename, slash + 1 - filename);

    QDesktopServices::openUrl (QUrl (QString (folder)));
}
