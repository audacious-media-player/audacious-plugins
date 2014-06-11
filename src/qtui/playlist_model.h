/*
 * playlist_model.h
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

#ifndef PLAYLIST_MODEL_H
#define PLAYLIST_MODEL_H

#include <QAbstractTableModel>

class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT

public:
    PlaylistModel (QObject * parent = 0);
    ~PlaylistModel ();
    int rowCount (const QModelIndex & parent = QModelIndex ()) const;
    int columnCount (const QModelIndex & parent = QModelIndex ()) const;
    QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const;
    QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void setUniqueId (int id);
    int playlist () const;
    int uniqueId;
};

#endif
