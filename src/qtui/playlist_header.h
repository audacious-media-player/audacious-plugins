/*
 * playlist_header.h
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

#ifndef PLAYLIST_HEADER_H
#define PLAYLIST_HEADER_H

#include <QHeaderView>
#include <libaudcore/hook.h>

class PlaylistWidget;
class QAction;
class QContextMenuEvent;
class QMenu;

class PlaylistHeader : public QHeaderView
{
public:
    PlaylistHeader(PlaylistWidget * parent);

    /* this should be called by the playlist after adding the header */
    void updateColumns();

private:
    PlaylistWidget * m_playlist;
    bool m_inUpdate = false;
    bool m_inStyleChange = false;
    int m_lastCol = -1;

    void sectionClicked(int logicalIndex);
    void sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void sectionResized(int logicalIndex, int /*oldSize*/, int newSize);

    void contextMenuEvent(QContextMenuEvent * event);
    bool event(QEvent * event);

    void updateStyle();

    const HookReceiver<PlaylistHeader> //
        hook1{"qtui update playlist columns", this,
              &PlaylistHeader::updateColumns},
        hook2{"qtui update playlist headers", this,
              &PlaylistHeader::updateStyle};
};

#endif // PLAYLIST_HEADER_H
