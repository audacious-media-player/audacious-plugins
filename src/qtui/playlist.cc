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

#include <QKeyEvent>
#include <QSortFilterProxyModel>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>

#include "playlist.h"
#include "playlist.moc"
#include "playlist_model.h"

PlaylistWidget::PlaylistWidget (QTreeView * parent, int uniqueId) : QTreeView (parent)
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

void PlaylistWidget::setFilter (const QString &text)
{
    proxyModel->setFilterRegExp (QRegExp (text, Qt::CaseInsensitive, QRegExp::FixedString));
}

PlaylistWidget::~PlaylistWidget ()
{
    delete model;
    delete proxyModel;
}

void PlaylistWidget::keyPressEvent (QKeyEvent * e)
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

void PlaylistWidget::mouseDoubleClickEvent (QMouseEvent * event)
{
    playCurrentIndex ();
}

int PlaylistWidget::playlist () const
{
    return model->playlist ();
}

int PlaylistWidget::uniqueId () const
{
    return model->uniqueId ();
}

void PlaylistWidget::scrollToCurrent ()
{
    int row = aud_playlist_get_position (playlist ());
    auto index = proxyModel->mapFromSource (model->index (row));
    setCurrentIndex (index);
    scrollTo (index);
}

void PlaylistWidget::update (void * level, int at, int count)
{
    if (level == PLAYLIST_UPDATE_STRUCTURE)
    {
        int old_entries = model->rowCount ();
        int entries = aud_playlist_entry_count (playlist ());

        model->removeRows (at, old_entries - (entries - count));
        model->insertRows (at, count);
    }
    else if (level == PLAYLIST_UPDATE_METADATA)
        model->updateRows (at, count);

    updateQueue ();
}

void PlaylistWidget::positionUpdate ()
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

void PlaylistWidget::playCurrentIndex ()
{
    aud_playlist_set_position (playlist (), proxyModel->mapToSource (currentIndex ()).row ());
    aud_playlist_play (playlist ());
}

void PlaylistWidget::deleteCurrentSelection ()
{
    aud_playlist_entry_delete (playlist (), proxyModel->mapToSource (currentIndex ()).row (), 1);
}

void PlaylistWidget::toggleQueue ()
{
    int row = proxyModel->mapToSource (currentIndex ()).row ();
    int at = aud_playlist_queue_find_entry (playlist (), row);

    if (at < 0)
        aud_playlist_queue_insert (playlist (), -1, row);
    else
        aud_playlist_queue_delete (playlist (), at, 1);
    model->updateRow (row);
}

void PlaylistWidget::updateQueue ()
{
    for (int i = aud_playlist_queue_count (playlist ()); i --;)
    {
        int row = aud_playlist_queue_get_entry (playlist (), i);
        model->updateRow (row);
    }
}
