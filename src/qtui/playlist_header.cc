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
#include "playlist_model.h"
#include "playlist.h"

#include <string.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

static const char * const s_col_keys[] = {
    "playing",
    "number",
    "title",
    "artist",
    "year",
    "album",
    "album-artist",
    "track",
    "genre",
    "queued",
    "length",
    "path",
    "filename",
    "custom",
    "bitrate",
    "comment"
};

static const int s_default_widths[] = {
    25,   // playing
    25,   // entry number
    275,  // title
    175,  // artist
    50,   // year
    175,  // album
    175,  // album artist
    75,   // track
    100,  // genre
    25,   // queue position
    75,   // length
    275,  // path
    275,  // filename
    275,  // custom title
    75,   // bitrate
    275   // comment
};

static_assert (aud::n_elems (s_col_keys) == PlaylistModel::n_cols, "update s_col_keys");
static_assert (aud::n_elems (s_default_widths) == PlaylistModel::n_cols, "update s_default_widths");

static int s_num_cols;
static int s_cols[PlaylistModel::n_cols];
static int s_col_widths[PlaylistModel::n_cols];

static void loadConfig ()
{
    static bool loaded = false;

    if (loaded)
        return;

    s_num_cols = 0;

    String columns = aud_get_str ("qtui", "playlist_columns");
    Index<String> index = str_list_to_index (columns, " ");

    int count = aud::min (index.len (), (int) PlaylistModel::n_cols);

    for (int c = 0; c < count; c ++)
    {
        const String & column = index[c];

        int i = 0;
        while (i < PlaylistModel::n_cols && strcmp (column, s_col_keys[i]))
            i ++;

        if (i == PlaylistModel::n_cols)
            break;

        s_cols[s_num_cols ++] = i;
    }

    auto widths = str_list_to_index (aud_get_str ("qtui", "column_widths"), ", ");
    int nwidths = aud::min (widths.len (), (int) PlaylistModel::n_cols);

    for (int i = 0; i < nwidths; i ++)
        s_col_widths[i] = str_to_int (widths[i]);
    for (int i = nwidths; i < PlaylistModel::n_cols; i ++)
        s_col_widths[i] = s_default_widths[i];

    loaded = true;
}

static void saveConfig ()
{
    Index<String> index;
    for (int i = 0; i < s_num_cols; i ++)
        index.append (String (s_col_keys[s_cols[i]]));

    aud_set_str ("qtui", "playlist_columns", index_to_str_list (index, " "));
    aud_set_str ("qtui", "column_widths", int_array_to_str (s_col_widths, PlaylistModel::n_cols));
}

PlaylistHeader::PlaylistHeader (PlaylistWidget * playlist) :
    QHeaderView (Qt::Horizontal, playlist),
    m_playlist (playlist)
{
    loadConfig ();

    setSectionsMovable (true);

    // avoid resize signalling for the last visible section
    setStretchLastSection (false);

    connect (this, & QHeaderView::sectionResized, this, & PlaylistHeader::sectionResized);
    connect (this, & QHeaderView::sectionMoved, this, & PlaylistHeader::sectionMoved);
}

static void toggle_column (int col, bool on)
{
    int pos = -1;

    for (int i = 0; i < s_num_cols; i ++)
    {
        if (s_cols[i] == col)
        {
            pos = i;
            break;
        }
    }

    if (on)
    {
        if (pos >= 0)
            return;

        s_cols[s_num_cols ++] = col;
    }
    else
    {
        if (pos < 0)
            return;

        s_num_cols --;
        for (int i = pos; i < s_num_cols; i ++)
            s_cols[i] = s_cols[i + 1];
    }

    saveConfig ();

    // update all playlists
    hook_call ("qtui update playlist columns", nullptr);
}

void PlaylistHeader::contextMenuEvent (QContextMenuEvent * event)
{
    auto menu = new QMenu (this);
    QAction * actions[PlaylistModel::n_cols];

    for (int c = 0; c < PlaylistModel::n_cols; c ++)
    {
        actions[c] = new QAction (_(PlaylistModel::labels[c]), menu);
        actions[c]->setCheckable (true);

        QObject::connect (actions[c], & QAction::toggled, [c] (bool on) {
            toggle_column (c, on);
        });

        menu->addAction (actions[c]);
    }

    for (int i = 0; i < s_num_cols; i ++)
        actions[s_cols[i]]->setChecked (true);

    menu->popup (event->globalPos ());
}

void PlaylistHeader::updateColumns ()
{
    m_inUpdate = true;

    // Due to QTBUG-33974, column #0 cannot be moved by the user.
    // As a workaround, hide column #0 and start the real columns at #1.
    // However, Qt will hide the header completely if no columns are visible.
    // This is bad since the user can't right-click to add any columns again.
    // To prevent this, show column #0 if no real columns are visible.
    m_playlist->setColumnHidden (0, (s_num_cols > 0));

    bool shown[PlaylistModel::n_cols] {};

    for (int i = 0; i < s_num_cols; i++)
    {
        int col = s_cols[i];
        moveSection (visualIndex (1 + col), 1 + i);
        shown[col] = true;
    }

    for (int col = 0; col < PlaylistModel::n_cols; col++)
    {
        // TODO: set column width based on font size
        m_playlist->setColumnWidth (1 + col, s_col_widths[col]);
        m_playlist->setColumnHidden (1 + col, ! shown[col]);
    }

    m_inUpdate = false;
}

void PlaylistHeader::sectionMoved (int /*logicalIndex*/, int oldVisualIndex, int newVisualIndex)
{
    if (m_inUpdate)
        return;

    int old_pos = oldVisualIndex - 1;
    int new_pos = newVisualIndex - 1;

    if (old_pos < 0 || old_pos > s_num_cols || new_pos < 0 || new_pos > s_num_cols)
        return;

    int col = s_cols[old_pos];

    for (int i = old_pos; i < new_pos; i ++)
        s_cols[i] = s_cols[i + 1];
    for (int i = old_pos; i > new_pos; i --)
        s_cols[i] = s_cols[i - 1];

    s_cols[new_pos] = col;
    saveConfig ();

    // update all the other playlists
    hook_call ("qtui update playlist columns", nullptr);
}

void PlaylistHeader::sectionResized (int logicalIndex, int /*oldSize*/, int newSize)
{
    if (m_inUpdate || newSize == 0)
        return;

    int col = logicalIndex - 1;
    if (col < 0 || col > PlaylistModel::n_cols)
        return;

    s_col_widths[col] = newSize;
    saveConfig ();

    // update all the other playlists
    hook_call ("qtui update playlist columns", nullptr);
}
