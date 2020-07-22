/*
 * playlist_selection.cc
 * Copyright 2020 Ariadne Conill
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

#include "playlist_selection.h"

namespace Moonstone {

QVariant PlaylistsModel::data (const QModelIndex & index, int role) const
{
    switch (role)
    {
    case Qt::DisplayRole:
    {
        auto list = Playlist::by_index (index.row ());
        switch (index.column ())
        {
        case ColumnTitle:
            return QString (list.get_title ());
        case ColumnEntries:
            return list.n_entries ();
        }
    }
        break;

    case Qt::FontRole:
        if (index.row () == m_playing)
            return m_bold;
        break;

    case Qt::TextAlignmentRole:
        if (index.column () == ColumnEntries)
            return Qt::AlignRight;
        break;
    }

    return QVariant ();
}

QVariant PlaylistsModel::headerData (int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case ColumnTitle:
            return QString (_("Title"));
        case ColumnEntries:
            return QString (_("Entries"));
        }
    }

    return QVariant ();
}

void PlaylistsModel::update_rows (int row, int count)
{
    if (count < 1)
        return;

    auto topLeft = createIndex (row, 0);
    auto bottomRight = createIndex (row + count - 1, NColumns - 1);
    emit dataChanged (topLeft, bottomRight);
}

void PlaylistsModel::update_playing ()
{
    int playing = Playlist::playing_playlist ().index ();

    if (playing != m_playing)
    {
        if (m_playing >= 0)
            update_rows (m_playing, 1);
        if (playing >= 0)
            update_rows (playing, 1);

        m_playing = playing;
    }
}

void PlaylistsModel::update (const Playlist::UpdateLevel level)
{
    int rows = Playlist::n_playlists ();

    if (level == Playlist::Structure)
    {
        if (rows < m_rows)
        {
            beginRemoveRows (QModelIndex (), rows, m_rows - 1);
            m_rows = rows;
            endRemoveRows ();
        }
        else if (rows > m_rows)
        {
            beginInsertRows (QModelIndex (), m_rows, rows - 1);
            m_rows = rows;
            endInsertRows ();
        }
    }

    if (level >= Playlist::Metadata)
    {
        update_rows (0, m_rows);
        m_playing = Playlist::playing_playlist ().index ();
    }
    else
        update_playing ();
}

PlaylistsView::PlaylistsView ()
{
    m_model.setFont (font ());

    m_in_update ++;
    setModel (& m_model);
    update_sel ();
    m_in_update --;

    auto hdr = header ();
    hdr->setStretchLastSection (false);
    hdr->setSectionResizeMode (PlaylistsModel::ColumnTitle, QHeaderView::Stretch);
    hdr->setSectionResizeMode (PlaylistsModel::ColumnEntries, QHeaderView::Interactive);
    hdr->resizeSection (PlaylistsModel::ColumnEntries, audqt::to_native_dpi (64));

    setDragDropMode (InternalMove);
    setFrameShape (QFrame::NoFrame);
    setIndentation (0);
    setHeaderHidden (true);
}

void PlaylistsView::currentChanged (const QModelIndex & current, const QModelIndex & previous)
{
    audqt::TreeView::currentChanged (current, previous);
    if (! m_in_update)
        Playlist::by_index (current.row ()).activate ();
}

void PlaylistsView::dropEvent (QDropEvent * event)
{
    if (event->source () != this || event->proposedAction () != Qt::MoveAction)
        return;

    int from = currentIndex ().row ();
    if (from < 0)
        return;

    int to;
    switch (dropIndicatorPosition ())
    {
        case AboveItem: to = indexAt (event->pos ()).row (); break;
        case BelowItem: to = indexAt (event->pos ()).row () + 1; break;
        case OnViewport: to = Playlist::n_playlists (); break;
        default: return;
    }

    Playlist::reorder_playlists (from, (to > from) ? to - 1 : to, 1);
    event->acceptProposedAction ();
}

void PlaylistsView::update (Playlist::UpdateLevel level)
{
    m_in_update ++;
    m_model.update (level);
    update_sel ();
    m_in_update --;
}

void PlaylistsView::update_sel ()
{
    m_in_update ++;
    auto sel = selectionModel ();
    auto current = m_model.index (Playlist::active_playlist ().index (), 0);
    sel->setCurrentIndex (current, sel->ClearAndSelect | sel->Rows);
    m_in_update --;
}

}
