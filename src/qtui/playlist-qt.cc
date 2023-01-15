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
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include "playlist-qt.h"
#include "playlist_header.h"
#include "playlist_model.h"

#include "../ui-common/menu-ops.h"
#include "../ui-common/qt-compat.h"

PlaylistWidget::PlaylistWidget(QWidget * parent, Playlist playlist)
    : audqt::TreeView(parent), m_playlist(playlist),
      model(new PlaylistModel(this, playlist)),
      proxyModel(new PlaylistProxyModel(this, playlist))
{
    model->setFont(font());

    /* setting up filtering model */
    proxyModel->setSourceModel(model);

    inUpdate = true; /* prevents changing focused row */
    setModel(proxyModel);
    inUpdate = false;

    auto header = new PlaylistHeader(this);
    setHeader(header);
    /* this has to come after setHeader() to take effect */
    header->setSectionsClickable(true);

    setAllColumnsShowFocus(true);
    setAlternatingRowColors(true);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setUniformRowHeights(true);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(ExtendedSelection);
    setDragDropMode(DragDrop);
    setMouseTracking(true);

    updateSettings();
    header->updateColumns();

    /* get initial selection and focus from core */
    inUpdate = true;
    updateSelection(0, 0);
    inUpdate = false;

    connect(this, &QTreeView::activated, [this](const QModelIndex & index) {
        if (index.isValid())
        {
            m_playlist.set_position(indexToRow(index));
            m_playlist.start_playback();
        }
    });
}

PlaylistWidget::~PlaylistWidget()
{
    delete model;
    delete proxyModel;
}

QModelIndex PlaylistWidget::rowToIndex(int row)
{
    if (row < 0)
        return QModelIndex();

    return proxyModel->mapFromSource(model->index(row, firstVisibleColumn));
}

int PlaylistWidget::indexToRow(const QModelIndex & index)
{
    if (!index.isValid())
        return -1;

    return proxyModel->mapToSource(index).row();
}

QModelIndex PlaylistWidget::visibleIndexNear(int row)
{
    QModelIndex index = rowToIndex(row);
    if (index.isValid())
        return index;

    int n_entries = m_playlist.n_entries();

    for (int r = row + 1; r < n_entries; r++)
    {
        index = rowToIndex(r);
        if (index.isValid())
            return index;
    }

    for (int r = row - 1; r >= 0; r--)
    {
        index = rowToIndex(r);
        if (index.isValid())
            return index;
    }

    return index;
}

void PlaylistWidget::changeEvent(QEvent * event)
{
    if (event->type() == QEvent::FontChange)
        model->setFont(font());

    audqt::TreeView::changeEvent(event);
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent * event)
{
    if (contextMenu)
        contextMenu->popup(event->globalPos());
}

void PlaylistWidget::keyPressEvent(QKeyEvent * event)
{
    auto CtrlShiftAlt =
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;
    if (!(event->modifiers() & CtrlShiftAlt))
    {
        switch (event->key())
        {
        case Qt::Key_Right:
            aud_drct_seek(aud_drct_get_time() +
                          aud_get_int("step_size") * 1000);
            return;
        case Qt::Key_Left:
            aud_drct_seek(aud_drct_get_time() -
                          aud_get_int("step_size") * 1000);
            return;
        case Qt::Key_Space:
            aud_drct_play_pause();
            return;
        case Qt::Key_Delete:
            pl_remove_selected();
            return;
        case Qt::Key_Z:
            aud_drct_pl_prev();
            return;
        case Qt::Key_X:
            aud_drct_play();
            return;
        case Qt::Key_C:
            aud_drct_pause();
            return;
        case Qt::Key_V:
            aud_drct_stop();
            return;
        case Qt::Key_B:
            aud_drct_pl_next();
            return;
        }
    }

    audqt::TreeView::keyPressEvent(event);
}

void PlaylistWidget::mouseMoveEvent(QMouseEvent * event)
{
    int row = indexToRow(indexAt(event->pos()));

    if (row < 0)
        hidePopup();
    else if (aud_get_bool("show_filepopup_for_tuple") && m_popup_pos != row)
        triggerPopup(row);

    audqt::TreeView::mouseMoveEvent(event);
}

void PlaylistWidget::leaveEvent(QEvent * event)
{
    hidePopup();

    audqt::TreeView::leaveEvent(event);
}

/* Since Qt doesn't support both DragDrop and InternalMove at once,
 * this hack is needed to set the drag icon to "move" for internal drags. */
void PlaylistWidget::dragMoveEvent(QDragMoveEvent * event)
{
    if (event->source() == this)
        event->setDropAction(Qt::MoveAction);

    audqt::TreeView::dragMoveEvent(event);

    if (event->source() == this)
        event->setDropAction(Qt::MoveAction);
}

void PlaylistWidget::dropEvent(QDropEvent * event)
{
    /* let Qt forward external drops to the PlaylistModel */
    if (event->source() != this)
        return audqt::TreeView::dropEvent(event);

    int from = indexToRow(currentIndex());
    if (from < 0)
        return;

    int to;
    switch (dropIndicatorPosition())
    {
    case AboveItem:
        to = indexToRow(indexAt(QtCompat::pos(event)));
        break;
    case BelowItem:
        to = indexToRow(indexAt(QtCompat::pos(event))) + 1;
        break;
    case OnViewport:
        to = m_playlist.n_entries();
        break;
    default:
        return;
    }

    /* Adjust the shift amount so that the selected entry closest to the
     * destination ends up at the destination. */
    if (to > from)
        to -= m_playlist.n_selected(from, to - from);
    else
        to += m_playlist.n_selected(to, from - to);

    m_playlist.shift_entries(from, to - from);

    event->acceptProposedAction();
}

void PlaylistWidget::currentChanged(const QModelIndex & current,
                                    const QModelIndex & previous)
{
    audqt::TreeView::currentChanged(current, previous);

    if (!inUpdate)
        m_playlist.set_focus(indexToRow(current));
}

void PlaylistWidget::selectionChanged(const QItemSelection & selected,
                                      const QItemSelection & deselected)
{
    audqt::TreeView::selectionChanged(selected, deselected);

    if (!inUpdate)
    {
        for (const QModelIndex & idx : selected.indexes())
            m_playlist.select_entry(indexToRow(idx), true);
        for (const QModelIndex & idx : deselected.indexes())
            m_playlist.select_entry(indexToRow(idx), false);
    }
}

/* returns true if the focus changed or the playlist scrolled */
bool PlaylistWidget::scrollToCurrent(bool force)
{
    bool scrolled = false;
    int entry = m_playlist.get_position();

    if (entry >= 0 && (aud_get_bool("qtui", "autoscroll") || force))
    {
        if (m_playlist.get_focus() != entry)
            scrolled = true;

        m_playlist.select_all(false);
        m_playlist.select_entry(entry, true);
        m_playlist.set_focus(entry);

        auto index = rowToIndex(entry);
        auto rect = visualRect(index);

        scrollTo(index);

        if (visualRect(index) != rect)
            scrolled = true;
    }

    return scrolled;
}

void PlaylistWidget::updatePlaybackIndicator()
{
    if (currentPos >= 0)
        model->entriesChanged(currentPos, 1);
}

void PlaylistWidget::getSelectedRanges(int rowsBefore, int rowsAfter,
                                       QItemSelection & selected,
                                       QItemSelection & deselected)
{
    int entries = m_playlist.n_entries();
    int last_col = model->columnCount() - 1;

    auto make_range = [last_col](const QModelIndex & first,
                                 const QModelIndex & last) {
        // expand the range to cover all columns
        return QItemSelectionRange(first.sibling(first.row(), 0),
                                   last.sibling(last.row(), last_col));
    };

    QItemSelection ranges[2];
    QModelIndex first, last;
    bool prev = false;

    for (int row = rowsBefore; row < entries - rowsAfter; row++)
    {
        auto idx = rowToIndex(row);
        if (!idx.isValid())
            continue;

        bool sel = m_playlist.entry_selected(row);

        if (sel != prev && first.isValid())
            ranges[prev] += make_range(first, last);

        if (sel != prev || !first.isValid())
            first = idx;

        last = idx;
        prev = sel;
    }

    if (first.isValid())
        ranges[prev] += make_range(first, last);

    selected = std::move(ranges[true]);
    deselected = std::move(ranges[false]);
}

void PlaylistWidget::updateSelection(int rowsBefore, int rowsAfter)
{
    QItemSelection selected, deselected;
    getSelectedRanges(rowsBefore, rowsAfter, selected, deselected);

    auto sel = selectionModel();
    auto old = sel->selection();

    // Qt's selection model is complex, with two layers that our internal
    // playlist selection model doesn't mirror. To avoid interfering with
    // interactive selections, we need to avoid changing Qt's model unless
    // it's actually out of sync. So, we start by computing the difference
    // between the two models.
    auto diff = old;
    diff.merge(selected, sel->Select);
    diff.merge(deselected, sel->Deselect);
    diff.merge(old, sel->Toggle);

    if (!diff.isEmpty())
    {
        // Toggle any cells that need to be toggled to bring the two
        // models in sync.
        sel->select(diff, sel->Toggle);

        // The prior call will have left any cells that were toggled
        // sitting in the interactive layer. Force Qt to finalize this
        // layer by making an empty selection, so that the next interactive
        // selection doesn't behave unexpectedly.
        sel->select(QModelIndex(), sel->Select);
    }

    auto focus = rowToIndex(m_playlist.get_focus());
    if (sel->currentIndex().row() != focus.row())
    {
        // The documentation for QAbstractItemView::setCurrentIndex says:
        //
        //     Unless the current selection mode is NoSelection, the
        //     item is also selected. Note that this function also updates
        //     the starting position for any new selections the user
        //     performs.
        //
        //     To set an item as the current item without selecting it,
        //     call:
        //
        //       selectionModel()->
        //         setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        //
        // We need to update that starting position (see bug #981), so
        // selectionModel()->setCurrentIndex() isn't enough here, but
        // we don't want to change the selection. Temporarily changing
        // the selection mode to NoSelection accomplishes what we want.
        //
        setSelectionMode(NoSelection);
        setCurrentIndex(focus);
        setSelectionMode(ExtendedSelection);
    }
}

void PlaylistWidget::playlistUpdate()
{
    auto update = m_playlist.update_detail();

    if (update.level == Playlist::NoUpdate)
        return;

    inUpdate = true;

    int entries = m_playlist.n_entries();
    int changed = entries - update.before - update.after;

    if (update.level == Playlist::Structure)
    {
        int old_entries = model->rowCount();
        int removed = old_entries - update.before - update.after;

        if (currentPos >= old_entries - update.after)
            currentPos += entries - old_entries;
        else if (currentPos >= update.before)
            currentPos = -1;

        model->entriesRemoved(update.before, removed);
        model->entriesAdded(update.before, changed);
    }
    else if (update.level == Playlist::Metadata || update.queue_changed)
        model->entriesChanged(update.before, changed);

    if (update.queue_changed)
    {
        for (int i = m_playlist.n_queued(); i--;)
        {
            int entry = m_playlist.queue_get_entry(i);
            if (entry < update.before || entry >= entries - update.after)
                model->entriesChanged(entry, 1);
        }
    }

    int pos = m_playlist.get_position();

    if (pos != currentPos)
    {
        if (currentPos >= 0)
            model->entriesChanged(currentPos, 1);
        if (pos >= 0)
            model->entriesChanged(pos, 1);

        currentPos = pos;
    }

    updateSelection(update.before, update.after);

    inUpdate = false;
}

void PlaylistWidget::setFilter(const char * text)
{
    // Save the current focus before filtering
    int focus = m_playlist.get_focus();

    // Empty the model before updating the filter.  This prevents Qt from
    // performing a series of "rows added" or "rows deleted" updates, which can
    // be very slow (worst case O(N^2) complexity) on a large playlist.
    model->entriesRemoved(0, model->rowCount());

    // Update the filter
    proxyModel->setFilter(text);

    // Repopulate the model
    model->entriesAdded(0, m_playlist.n_entries());

    // If the previously focused row is no longer visible with the new filter,
    // try to find a nearby one that is, and focus it.
    auto index = visibleIndexNear(focus);

    if (index.isValid())
    {
        focus = indexToRow(index);
        m_playlist.set_focus(focus);
        m_playlist.select_all(false);
        m_playlist.select_entry(focus, true);
        scrollTo(index);
    }
}

void PlaylistWidget::setFirstVisibleColumn(int col)
{
    inUpdate = true;
    firstVisibleColumn = col;

    // make sure current and selected indexes point to a visible column
    updateSelection(0, 0);

    inUpdate = false;
}

void PlaylistWidget::moveFocus(int distance)
{
    int visibleRows = proxyModel->rowCount();
    if (!visibleRows)
        return;

    int row = currentIndex().row();
    row = aud::clamp(row + distance, 0, visibleRows - 1);
    setCurrentIndex(proxyModel->index(row, 0));
}

void PlaylistWidget::showPopup()
{
    audqt::infopopup_show(m_playlist, m_popup_pos);
}

void PlaylistWidget::triggerPopup(int pos)
{
    audqt::infopopup_hide();

    m_popup_pos = pos;
    m_popup_timer.queue(aud_get_int("filepopup_delay") * 100,
                        [this]() { showPopup(); });
}

void PlaylistWidget::hidePopup()
{
    audqt::infopopup_hide();

    m_popup_pos = -1;
    m_popup_timer.stop();
}

void PlaylistWidget::updateSettings()
{
    setHeaderHidden(!aud_get_bool("qtui", "playlist_headers"));
}
