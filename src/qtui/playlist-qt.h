/*
 * playlist.h
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

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QTreeView>

#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>

class PlaylistModel;
class PlaylistProxyModel;
class QContextMenuEvent;
class QMenu;

class PlaylistWidget : public QTreeView
{
public:
    PlaylistWidget (QWidget * parent, Playlist playlist);
    ~PlaylistWidget ();

    Playlist playlist () const
        { return m_playlist; }

    void scrollToCurrent (bool force = false);
    void updatePlaybackIndicator ();
    void playlistUpdate ();
    void playCurrentIndex ();
    void setFilter (const char * text);
    void setFirstVisibleColumn (int col);
    void moveFocus (int distance);

    void setContextMenu (QMenu * menu)
        { contextMenu = menu; }

private:
    Playlist m_playlist;
    PlaylistModel * model;
    PlaylistProxyModel * proxyModel;
    QMenu * contextMenu = nullptr;

    int currentPos = -1;
    bool inUpdate = false;
    int firstVisibleColumn = 0;

    QModelIndex rowToIndex (int row);
    int indexToRow (const QModelIndex & index);

    void getSelectedRanges (int rowsBefore, int rowsAfter,
     QItemSelection & selected, QItemSelection & deselected);
    void updateSelection (int rowsBefore, int rowsAfter);

    void contextMenuEvent (QContextMenuEvent * event);
    void keyPressEvent (QKeyEvent * event);
    void mouseDoubleClickEvent (QMouseEvent * event);
    void dragMoveEvent (QDragMoveEvent * event);
    void dropEvent (QDropEvent * event);
    void currentChanged (const QModelIndex & current, const QModelIndex & previous);
    void selectionChanged (const QItemSelection & selected, const QItemSelection & deselected);

    void updateSettings ();

    const HookReceiver<PlaylistWidget>
     hook1 {"qtui update playlist settings", this, & PlaylistWidget::updateSettings};
};

#endif
