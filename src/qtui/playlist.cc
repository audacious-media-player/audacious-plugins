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

Playlist::Playlist (QTreeView * parent, int uniqueId) : QTreeView (parent)
{
    model = new PlaylistModel (0, uniqueId);

    /* setting up filtering model */
    proxyModel = new QSortFilterProxyModel (this);
    proxyModel->setSourceModel (model);
    proxyModel->setFilterKeyColumn (-1); /* filter by all columns */

    setModel (proxyModel);
    setAlternatingRowColors (true);
    setAttribute (Qt::WA_MacShowFocusRect, false);
    setIndentation (0);
    setUniformRowHeights (true);
    setFrameShape (QFrame::NoFrame);
    setColumnWidth (PL_COL_NOW_PLAYING, 25);
    setColumnWidth (PL_COL_TITLE, 300);
    setColumnWidth (PL_COL_ARTIST, 150);
    setColumnWidth (PL_COL_ALBUM, 200);
    resizeColumnToContents(PL_COL_QUEUED);
    resizeColumnToContents(PL_COL_LENGTH);
    positionUpdate ();
}

void Playlist::setFilter (const QString &text)
{
    proxyModel->setFilterRegExp (QRegExp (text, Qt::CaseInsensitive, QRegExp::FixedString));
}

Playlist::~Playlist ()
{
    delete model;
    delete proxyModel;
}

void Playlist::keyPressEvent (QKeyEvent * e)
{
    switch (e->modifiers ())
    {
    case Qt::NoModifier:
        switch (e->key ())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            playCurrentIndex ();
            break;
        case Qt::Key_Right:
            aud_drct_seek (aud_drct_get_time () + 5000);
            break;
        case Qt::Key_Left:
            aud_drct_seek (aud_drct_get_time () - 5000);
            break;
        case Qt::Key_Space:
            aud_drct_play_pause ();
            break;
        case Qt::Key_Delete:
            deleteCurrentSelection ();
            break;
        case Qt::Key_Z:
            aud_drct_pl_prev ();
            return;
            break;
        case Qt::Key_X:
            aud_drct_play ();
            return;
            break;
        case Qt::Key_C:
            aud_drct_pause ();
            return;
            break;
        case Qt::Key_V:
            aud_drct_stop ();
            return;
            break;
        case Qt::Key_B:
            aud_drct_pl_next ();
            return;
            break;
        case Qt::Key_Q:
            toggleQueue ();
            return;
            break;
        }
        break;
    case Qt::ControlModifier:
        switch (e->key ())
        {
        case Qt::Key_L:
            scrollToCurrent ();
            break;
        }
        break;
    }

     QTreeView::keyPressEvent (e);
}

void Playlist::mouseDoubleClickEvent (QMouseEvent * event)
{
    playCurrentIndex ();
}

int Playlist::playlist () const
{
    return model->playlist ();
}

int Playlist::uniqueId () const
{
    return model->uniqueId ();
}

void Playlist::scrollToCurrent ()
{
    int row = aud_playlist_get_position (playlist ());
    auto index = proxyModel->mapFromSource (model->index (row));
    setCurrentIndex (index);
    scrollTo (index);
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

    updateQueue ();
}

void Playlist::positionUpdate ()
{
    int row = aud_playlist_get_position (playlist ());
    if (! aud_playlist_update_pending ())
    {
        model->updateRow (previousEntry);
        previousEntry = row;
        setFocus ();
        scrollToCurrent ();
        updateQueue ();
    }
}

void Playlist::playCurrentIndex ()
{
    aud_playlist_set_position (playlist (), proxyModel->mapToSource (currentIndex ()).row ());
    aud_drct_play_playlist (playlist ());
}

void Playlist::deleteCurrentSelection ()
{
    aud_playlist_entry_delete (playlist (), proxyModel->mapToSource (currentIndex ()).row (), 1);
}

void Playlist::toggleQueue ()
{
    int row = proxyModel->mapToSource (currentIndex ()).row ();
    int at = aud_playlist_queue_find_entry (playlist (), row);

    if (at < 0)
        aud_playlist_queue_insert (playlist (), -1, row);
    else
        aud_playlist_queue_delete (playlist (), at, 1);
    model->updateRow (row);
}

void Playlist::updateQueue ()
{
    for (int i = aud_playlist_queue_count (playlist ()); i --;)
    {
        int row = aud_playlist_queue_get_entry (playlist (), i);
        model->updateRow (row);
    }
}
