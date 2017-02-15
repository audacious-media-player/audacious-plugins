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

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QSortFilterProxyModel>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

#include "playlist.h"
#include "playlist_columns.h"
#include "playlist_header.h"
#include "playlist_model.h"

#include "../ui-common/menu-ops.h"

PlaylistWidget::PlaylistWidget (QWidget * parent, int uniqueID) :
    QTreeView (parent),
    in_columnUpdate (false)
{
    model = new PlaylistModel (this, uniqueID);

    /* setting up filtering model */
    proxyModel = new PlaylistProxyModel (this, uniqueID);
    proxyModel->setSourceModel (model);

    inUpdate = true; /* prevents changing focused row */
    setModel (proxyModel);
    inUpdate = false;

    setHeader (new PlaylistHeader (this));

    setAllColumnsShowFocus (true);
    setAlternatingRowColors (true);
    setAttribute (Qt::WA_MacShowFocusRect, false);
    setUniformRowHeights (true);
    setFrameShape (QFrame::NoFrame);
    setSelectionMode (ExtendedSelection);
    setDragDropMode (DragDrop);

    updateSettings ();

    pl_col_init ();
    updateColumns ();

    auto hdr = header ();
    hdr->setSectionsMovable (true);
    // avoid resize signalling for the last visible section
    hdr->setStretchLastSection (false);
    QObject::connect (hdr, & QHeaderView::sectionResized, this, & PlaylistWidget::sectionResized);
    QObject::connect (hdr, & QHeaderView::sectionMoved, this, & PlaylistWidget::sectionMoved);

    /* get initial selection and focus from core */
    Playlist::Update upd {};
    upd.level = Playlist::Selection;
    update (upd);
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

void PlaylistWidget::contextMenuEvent (QContextMenuEvent * event)
{
    if (contextMenu)
        contextMenu->popup (event->globalPos ());
}

void PlaylistWidget::keyPressEvent (QKeyEvent * event)
{
    switch (event->modifiers ())
    {
    case Qt::NoModifier:
        switch (event->key ())
        {
        case Qt::Key_Escape:
            scrollToCurrent (true);
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            playCurrentIndex ();
            break;
        case Qt::Key_Right:
            aud_drct_seek (aud_drct_get_time () + aud_get_double ("qtui", "step_size") * 1000);
            break;
        case Qt::Key_Left:
            aud_drct_seek (aud_drct_get_time () - aud_get_double ("qtui", "step_size") * 1000);
            break;
        case Qt::Key_Space:
            aud_drct_play_pause ();
            break;
        case Qt::Key_Delete:
            pl_remove_selected ();
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
        }
        break;
    }

    QTreeView::keyPressEvent (event);
}

void PlaylistWidget::mouseDoubleClickEvent (QMouseEvent * event)
{
    QModelIndex index = indexAt (event->pos ());
    if (! index.isValid ())
        return;

    if (event->button () == Qt::LeftButton)
        playCurrentIndex ();
}

/* Since Qt doesn't support both DragDrop and InternalMove at once,
 * this hack is needed to set the drag icon to "move" for internal drags. */
void PlaylistWidget::dragMoveEvent (QDragMoveEvent * event)
{
    if (event->source () == this)
        event->setDropAction (Qt::MoveAction);

    QTreeView::dragMoveEvent (event);

    if (event->source () == this)
        event->setDropAction (Qt::MoveAction);
}

void PlaylistWidget::dropEvent (QDropEvent * event)
{
    /* let Qt forward external drops to the PlaylistModel */
    if (event->source () != this)
        return QTreeView::dropEvent (event);

    int list = model->playlist ();
    int from = indexToRow (currentIndex ());
    if (from < 0)
        return;

    int to;
    switch (dropIndicatorPosition ())
    {
        case AboveItem: to = indexToRow (indexAt (event->pos ())); break;
        case BelowItem: to = indexToRow (indexAt (event->pos ())) + 1; break;
        case OnViewport: to = aud_playlist_entry_count (list); break;
        default: return;
    }

    /* Adjust the shift amount so that the selected entry closest to the
     * destination ends up at the destination. */
    if (to > from)
        to -= aud_playlist_selected_count (list, from, to - from);
    else
        to += aud_playlist_selected_count (list, to, from - to);

    aud_playlist_shift (list, from, to - from);

    event->acceptProposedAction ();
}

void PlaylistWidget::currentChanged (const QModelIndex & current, const QModelIndex & previous)
{
    QTreeView::currentChanged (current, previous);

    if (! inUpdate)
        aud_playlist_set_focus (model->playlist (), indexToRow (current));
}

void PlaylistWidget::selectionChanged (const QItemSelection & selected,
 const QItemSelection & deselected)
{
    QTreeView::selectionChanged (selected, deselected);

    if (! inUpdate)
    {
        int list = model->playlist ();

        for (const QModelIndex & idx : selected.indexes ())
            aud_playlist_entry_set_selected (list, indexToRow (idx), true);
        for (const QModelIndex & idx : deselected.indexes ())
            aud_playlist_entry_set_selected (list, indexToRow (idx), false);
    }
}

void PlaylistWidget::scrollToCurrent (bool force)
{
    int list = model->playlist ();
    int entry = aud_playlist_get_position (list);

    if (aud_get_bool ("qtui", "autoscroll") || force)
    {
        aud_playlist_select_all (list, false);
        aud_playlist_entry_set_selected (list, entry, true);
        aud_playlist_set_focus (list, entry);

        // a playlist update should have been queued, unless the playlist is empty
        if (aud_playlist_update_pending (list))
            scrollQueued = true;
    }
}

void PlaylistWidget::updatePlaybackIndicator ()
{
    int list = model->playlist ();

    if (aud_playlist_update_pending (list))
        needIndicatorUpdate = true;
    else if (currentPos >= 0)
        model->entriesChanged (currentPos, 1);
}

void PlaylistWidget::getSelectedRanges (const Playlist::Update & update,
 QItemSelection & selected, QItemSelection & deselected)
{
    int list = model->playlist ();
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

    int list = model->playlist ();
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

        model->entriesRemoved (update.before, removed);
        model->entriesAdded (update.before, changed);
    }
    else if (update.level == Playlist::Metadata || update.queue_changed)
        model->entriesChanged (update.before, changed);

    if (update.queue_changed)
    {
        for (int i = aud_playlist_queue_count (list); i --; )
        {
            int entry = aud_playlist_queue_get_entry (list, i);
            if (entry < update.before || entry >= entries - update.after)
                model->entriesChanged (entry, 1);
        }
    }

    int pos = aud_playlist_get_position (list);

    if (needIndicatorUpdate || pos != currentPos)
    {
        if (currentPos >= 0)
            model->entriesChanged (currentPos, 1);
        if (pos >= 0 && pos != currentPos)
            model->entriesChanged (pos, 1);

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
    int list = model->playlist ();
    aud_playlist_set_position (list, indexToRow (currentIndex ()));
    aud_playlist_play (list);
}

void PlaylistWidget::setFilter (const char * text)
{
    proxyModel->setFilter (text);

    int list = model->playlist ();
    int focus = aud_playlist_get_focus (list);
    QModelIndex index;

    // If there was a valid focus before filtering, Qt updates it for us via
    // currentChanged().  If not, we will set focus on the first visible row.

    if (focus >= 0)
        index = rowToIndex (focus);
    else
    {
        if (! proxyModel->rowCount ())
            return;

        index = proxyModel->index (0, 0);
        focus = indexToRow (index);
        aud_playlist_set_focus (list, focus);
    }

    if (! aud_playlist_entry_get_selected (list, focus))
    {
        aud_playlist_select_all (list, false);
        aud_playlist_entry_set_selected (list, focus, true);
    }

    scrollTo (index);
}

void PlaylistWidget::moveFocus (int distance)
{
    int visibleRows = proxyModel->rowCount ();
    if (! visibleRows)
        return;

    int row = currentIndex ().row ();
    row = aud::clamp (row + distance, 0, visibleRows - 1);
    setCurrentIndex (proxyModel->index (row, 0));
}

void PlaylistWidget::updateSettings ()
{
    setHeaderHidden (! aud_get_bool ("qtui", "playlist_headers"));
}

void PlaylistWidget::updateColumns ()
{
    auto hdr = header ();

    in_columnUpdate = true;

    // Due to QTBUG-33974, column #0 cannot be moved by the user.
    // As a workaround, hide column #0 and start the real columns at #1.
    // However, Qt will hide the header completely if no columns are visible.
    // This is bad since the user can't right-click to add any columns again.
    // To prevent this, show column #0 if no real columns are visible.
    setColumnHidden (0, (pl_num_cols > 0));

    bool shown[PL_COLS] {};

    for (int i = 0; i < pl_num_cols; i++)
    {
        int col = pl_cols[i];
        hdr->moveSection (hdr->visualIndex (1 + col), 1 + i);
        shown[col] = true;
    }

    for (int col = 0; col < PL_COLS; col++)
    {
        /* TODO: set column width based on font size */
        setColumnWidth (1 + col, pl_col_widths[col]);
        setColumnHidden (1 + col, ! shown[col]);
    }

    in_columnUpdate = false;
}

void PlaylistWidget::sectionResized (int logicalIndex, int oldSize, int newSize)
{
    if (in_columnUpdate || newSize == 0)
        return;

    int col = logicalIndex - 1;
    if (col < 0 || col > PL_COLS)
        return;

    pl_col_widths[col] = newSize;
    pl_col_save ();
}

void PlaylistWidget::sectionMoved (int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    if (in_columnUpdate)
        return;

    int old_pos = oldVisualIndex - 1;
    int new_pos = newVisualIndex - 1;

    if (old_pos < 0 || old_pos > pl_num_cols || new_pos < 0 || new_pos > pl_num_cols)
        return;

    int col = pl_cols[old_pos];

    for (int i = old_pos; i < new_pos; i ++)
        pl_cols[i] = pl_cols[i + 1];
    for (int i = old_pos; i > new_pos; i --)
        pl_cols[i] = pl_cols[i - 1];

    pl_cols[new_pos] = col;

    hook_call ("qtui update playlist columns from ui", nullptr);
    pl_col_save ();
}
