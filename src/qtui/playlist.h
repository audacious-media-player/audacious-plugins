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

#include <QtGui>
#include <QTreeView>

#include "playlist_model.h"
#include "filter_input.h"

class Playlist : public QTreeView
{
    Q_OBJECT

public:
    Playlist (QTreeView * parent = 0, int uniqueId = -1);
    ~Playlist ();
    void scrollToCurrent ();
    void update (int type, int at, int count);
    void positionUpdate ();
    void playCurrentIndex ();
    void setFilter (const QString &text);

private:
    PlaylistModel * model;
    QSortFilterProxyModel * proxyModel;
    int playlist ();

protected:
    void keyPressEvent (QKeyEvent * e); /* override default handler */
    void mouseDoubleClickEvent (QMouseEvent * event);
};

#endif
