/*
 * playlist.cc
 * Copyright 2014 Michał Lipski
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

#include <QtGui>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>

#include "playlist.h"
#include "playlist.moc"
#include "playlist_model.h"

Playlist::Playlist (QFrame * parent, int uniqueId) : QFrame (parent)
{
    setupUi (this);

    model = new PlaylistModel (0, uniqueId);

    treeView->setAlternatingRowColors (true);
    treeView->setAttribute (Qt::WA_MacShowFocusRect, false);
    treeView->setIndentation (0);
    treeView->setModel (model);
    treeView->setUniformRowHeights (true);
    treeView->setColumnWidth (0, 25);
    treeView->setColumnWidth (1, 300);
    treeView->setColumnWidth (2, 150);
    treeView->setColumnWidth (3, 200);

    connect(treeView, &QTreeView::doubleClicked, this, &Playlist::doubleClicked);

    positionUpdate ();
}

Playlist::~Playlist ()
{
    delete model;
}

void Playlist::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Enter or e->key() == Qt::Key_Return)
    {
        aud_playlist_set_position (playlist (), treeView->currentIndex().row());
        aud_drct_play_playlist (playlist ());
    }
    if (e->key() == Qt::Key_Right)
    {
        aud_drct_seek (aud_drct_get_time() + 5000);
    }
    if (e->key() == Qt::Key_Left)
    {
        aud_drct_seek (aud_drct_get_time() - 5000);
    }
}

void Playlist::doubleClicked (const QModelIndex &index)
{
    aud_playlist_set_position (playlist (), index.row ());
    aud_drct_play_playlist (playlist ());
}

int Playlist::playlist ()
{
    return model->playlist ();
}

void Playlist::scrollToCurrent ()
{
    int row = aud_playlist_get_position (playlist ());
    auto index = model->index (row);
    treeView->setCurrentIndex (index);
    treeView->scrollTo (index);
}

void Playlist::update (int type, int at, int count)
{
    if (type == PLAYLIST_UPDATE_STRUCTURE)
    {
        int old_entries = model->rowCount ();
        int entries = aud_playlist_entry_count (playlist ());

        model->removeRows (at, old_entries - (entries - count));
        model->insertRows (at, count);
    }
    else if (type == PLAYLIST_UPDATE_METADATA)
        model->updateRows (at, count);
}

void Playlist::positionUpdate ()
{
    int row = aud_playlist_get_position (playlist ());
    if (! aud_playlist_update_pending ())
    {
        model->updateRow (row);
        scrollToCurrent ();
    }
}
