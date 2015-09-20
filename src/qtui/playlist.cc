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
#include <libaudcore/runtime.h>

#include "playlist.h"
#include "playlist_model.h"

PlaylistWidget::PlaylistWidget (QTreeView * parent, int uniqueId) : QTreeView (parent)
{
    model = new PlaylistModel (nullptr, uniqueId);

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
    setSelectionMode (ExtendedSelection);

    updateSettings ();

    /* TODO: set column width based on font size */
    setColumnWidth (PL_COL_NOW_PLAYING, 25);
    setColumnWidth (PL_COL_TITLE, 275);
    setColumnWidth (PL_COL_ARTIST, 175);
    setColumnWidth (PL_COL_ALBUM, 175);
    setColumnWidth (PL_COL_QUEUED, 25);
    setColumnWidth (PL_COL_LENGTH, 50);

    /* get initial selection and focus from core */
    Playlist::Update upd {};
    upd.level = Playlist::Selection;
    update (upd);
}

void PlaylistWidget::setFilter (const QString & text)
{
    proxyModel->setFilterRegExp (QRegExp (text, Qt::CaseInsensitive, QRegExp::FixedString));
}

PlaylistWidget::~PlaylistWidget ()
{
    delete model;
    delete proxyModel;
}

QModelIndex PlaylistWidget::rowToIndex (int row)
{
    if (row < 0)
        return QModelIndex ();

    return proxyModel->mapFromSource (model->index (row));
}

int PlaylistWidget::indexToRow (const QModelIndex & index)
{
    if (! index.isValid ())
        return -1;

    return proxyModel->mapToSource (index).row ();
}

void PlaylistWidget::keyPressEvent (QKeyEvent * event)
{
    switch (event->modifiers ())
    {
    case Qt::NoModifier:
        switch (event->key ())
        {
        case Qt::Key_Escape:
            scrollToCurrent ();
            break;
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
        case Qt::Key_X:
            aud_drct_play ();
            return;
        case Qt::Key_C:
            aud_drct_pause ();
            return;
        case Qt::Key_V:
            aud_drct_stop ();
            return;
        case Qt::Key_B:
            aud_drct_pl_next ();
            return;
        case Qt::Key_Q:
            toggleQueue ();
            return;
        }
        break;
    }

     QTreeView::keyPressEvent (event);
}

void PlaylistWidget::mousePressEvent (QMouseEvent * event)
{
    QModelIndex index = indexAt (event->pos ());

    if (! index.isValid ())
        return;

    QTreeView::mousePressEvent (event);
}

void PlaylistWidget::mouseDoubleClickEvent (QMouseEvent * event)
{
    QModelIndex index = indexAt (event->pos ());

    if (! index.isValid ())
        return;

    if (event->button () == Qt::LeftButton)
        playCurrentIndex ();
}

void PlaylistWidget::currentChanged (const QModelIndex & current, const QModelIndex & previous)
{
    QTreeView::currentChanged (current, previous);

    if (! inUpdate)
        aud_playlist_set_focus (playlist (), indexToRow (current));
}

void PlaylistWidget::selectionChanged (const QItemSelection & selected,
 const QItemSelection & deselected)
{
    QTreeView::selectionChanged (selected, deselected);

    if (! inUpdate)
    {
        int list = playlist ();

        for (const QModelIndex & idx : selected.indexes ())
            aud_playlist_entry_set_selected (list, indexToRow (idx), true);
        for (const QModelIndex & idx : deselected.indexes ())
            aud_playlist_entry_set_selected (list, indexToRow (idx), false);
    }
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
    int list = playlist ();
    int entry = aud_playlist_get_position (list);

    aud_playlist_select_all (list, false);
    aud_playlist_entry_set_selected (list, entry, true);
    aud_playlist_set_focus (list, entry);

    // a playlist update should have been queued, unless the playlist is empty
    if (aud_playlist_update_pending (list))
        scrollQueued = true;
}

void PlaylistWidget::updatePlaybackIndicator ()
{
    int list = playlist ();

    if (aud_playlist_update_pending (list))
        needIndicatorUpdate = true;
    else if (currentPos >= 0)
        model->updateRows (currentPos, 1);
}

void PlaylistWidget::getSelectedRanges (const Playlist::Update & update,
 QItemSelection & selected, QItemSelection & deselected)
{
    int list = playlist ();
    int entries = aud_playlist_entry_count (list);

    QItemSelection ranges[2];
    QModelIndex first, last;
    bool prev = false;

    for (int row = update.before; row < entries - update.after; row ++)
    {
        auto idx = rowToIndex (row);
        if (! idx.isValid ())
            continue;

        bool sel = aud_playlist_entry_get_selected (list, row);

        if (sel != prev && first.isValid ())
            ranges[prev].merge (QItemSelection (first, last), QItemSelectionModel::Select);

        if (sel != prev || ! first.isValid ())
            first = idx;

        last = idx;
        prev = sel;
    }

    if (first.isValid ())
        ranges[prev].merge (QItemSelection (first, last), QItemSelectionModel::Select);

    selected = std::move (ranges[true]);
    deselected = std::move (ranges[false]);
}

void PlaylistWidget::update (const Playlist::Update & update)
{
    inUpdate = true;

    int list = playlist ();
    int entries = aud_playlist_entry_count (list);
    int changed = entries - update.before - update.after;

    if (update.level == Playlist::Structure)
    {
        int old_entries = model->rowCount ();
        int removed = old_entries - update.before - update.after;

        if (currentPos >= old_entries - update.after)
            currentPos += entries - old_entries;
        else if (currentPos >= update.before)
            currentPos = -1;

        model->removeRows (update.before, removed);
        model->insertRows (update.before, changed);
    }
    else if (update.level == Playlist::Metadata || update.queue_changed)
        model->updateRows (update.before, changed);

    if (update.queue_changed)
    {
        for (int i = aud_playlist_queue_count (list); i --; )
        {
            int entry = aud_playlist_queue_get_entry (list, i);
            if (entry < update.before || entry >= entries - update.after)
                model->updateRows (entry, 1);
        }
    }

    int pos = aud_playlist_get_position (list);

    if (needIndicatorUpdate || pos != currentPos)
    {
        if (currentPos >= 0)
            model->updateRows (currentPos, 1);
        if (pos >= 0 && pos != currentPos)
            model->updateRows (pos, 1);

        currentPos = pos;
        needIndicatorUpdate = false;
    }

    QItemSelection selected, deselected;
    getSelectedRanges (update, selected, deselected);

    auto sel = selectionModel ();

    if (! selected.isEmpty ())
        sel->select (selected, sel->Select | sel->Rows);
    if (! deselected.isEmpty ())
        sel->select (deselected, sel->Deselect | sel->Rows);

    auto current = rowToIndex (aud_playlist_get_focus (list));
    sel->setCurrentIndex (current, sel->NoUpdate);

    if (scrollQueued)
    {
        scrollTo (current);
        scrollQueued = false;
    }

    inUpdate = false;
}

void PlaylistWidget::playCurrentIndex ()
{
    aud_playlist_set_position (playlist (), indexToRow (currentIndex ()));
    aud_playlist_play (playlist ());
}

void PlaylistWidget::deleteCurrentSelection ()
{
    aud_playlist_delete_selected (playlist ());
}

void PlaylistWidget::toggleQueue ()
{
    int row = indexToRow (currentIndex ());
    int at = aud_playlist_queue_find_entry (playlist (), row);

    if (at < 0)
        aud_playlist_queue_insert (playlist (), -1, row);
    else
        aud_playlist_queue_delete (playlist (), at, 1);
}

void PlaylistWidget::updateSettings ()
{
    setHeaderHidden (! aud_get_bool ("qtui", "playlist_headers"));
}
