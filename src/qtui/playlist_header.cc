/*
 * playlist_header.cc
 * Copyright 2017 John Lindgren and Eugene Paskevich
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

#include "playlist_header.h"
#include "playlist_columns.h"
#include "playlist.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>

PlaylistHeader::PlaylistHeader (PlaylistWidget * playlist) :
    QHeaderView (Qt::Horizontal, playlist),
    m_playlist (playlist)
{
    setSectionsMovable (true);

    // avoid resize signalling for the last visible section
    setStretchLastSection (false);

    connect (this, & QHeaderView::sectionResized, this, & PlaylistHeader::sectionResized);
    connect (this, & QHeaderView::sectionMoved, this, & PlaylistHeader::sectionMoved);
}

static void toggle_column (int col, bool on)
{
    int pos = -1;

    for (int i = 0; i < pl_num_cols; i ++)
    {
        if (pl_cols[i] == col)
        {
            pos = i;
            break;
        }
    }

    if (on)
    {
        if (pos >= 0)
            return;

        pl_cols[pl_num_cols ++] = col;
    }
    else
    {
        if (pos < 0)
            return;

        pl_num_cols --;
        for (int i = pos; i < pl_num_cols; i ++)
            pl_cols[i] = pl_cols[i + 1];
    }

    hook_call ("qtui update playlist columns", nullptr);

    pl_col_save ();
}

void PlaylistHeader::contextMenuEvent (QContextMenuEvent * event)
{
    auto menu = new QMenu (this);
    QAction * actions[PL_COLS];

    for (int c = 0; c < PL_COLS; c ++)
    {
        actions[c] = new QAction (_(pl_col_names[c]), menu);
        actions[c]->setCheckable (true);

        QObject::connect (actions[c], & QAction::toggled, [c] (bool on) {
            toggle_column (c, on);
        });

        menu->addAction (actions[c]);
    }

    for (int i = 0; i < pl_num_cols; i ++)
        actions[pl_cols[i]]->setChecked (true);

    menu->popup (event->globalPos ());
}

void PlaylistHeader::updateColumns ()
{
    m_inColumnUpdate = true;

    pl_col_init (); // no-op if already called

    // Due to QTBUG-33974, column #0 cannot be moved by the user.
    // As a workaround, hide column #0 and start the real columns at #1.
    // However, Qt will hide the header completely if no columns are visible.
    // This is bad since the user can't right-click to add any columns again.
    // To prevent this, show column #0 if no real columns are visible.
    m_playlist->setColumnHidden (0, (pl_num_cols > 0));

    bool shown[PL_COLS] {};

    for (int i = 0; i < pl_num_cols; i++)
    {
        int col = pl_cols[i];
        moveSection (visualIndex (1 + col), 1 + i);
        shown[col] = true;
    }

    for (int col = 0; col < PL_COLS; col++)
    {
        // TODO: set column width based on font size
        m_playlist->setColumnWidth (1 + col, pl_col_widths[col]);
        m_playlist->setColumnHidden (1 + col, ! shown[col]);
    }

    m_inColumnUpdate = false;
}

void PlaylistHeader::sectionMoved (int /*logicalIndex*/, int oldVisualIndex, int newVisualIndex)
{
    if (m_inColumnUpdate)
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
    pl_col_save ();

    // update all the other playlists
    hook_call ("qtui update playlist columns", nullptr);
}

void PlaylistHeader::sectionResized (int logicalIndex, int /*oldSize*/, int newSize)
{
    if (m_inColumnUpdate || newSize == 0)
        return;

    int col = logicalIndex - 1;
    if (col < 0 || col > PL_COLS)
        return;

    pl_col_widths[col] = newSize;
    pl_col_save ();

    // update all the other playlists
    hook_call ("qtui update playlist columns", nullptr);
}
