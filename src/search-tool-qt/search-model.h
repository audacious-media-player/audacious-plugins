/*
 * search-model.h
 * Copyright 2011-2019 John Lindgren and René J.V. Bertin
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

#ifndef SEARCHMODEL_H
#define SEARCHMODEL_H

#include <QAbstractListModel>

#include <libaudcore/audstrings.h>
#include <libaudcore/multihash.h>
#include <libaudcore/playlist.h>

enum class SearchField {
    Genre,
    Artist,
    Album,
    Title,
    count
};

struct Key
{
    SearchField field;
    String name;

    bool operator== (const Key & b) const
        { return field == b.field && name == b.name; }
    unsigned hash () const
        { return (unsigned) field + name.hash (); }
};

struct Item
{
    SearchField field;
    String name, folded;
    Item * parent;
    SimpleHash<Key, Item> children;
    Index<int> matches;

    Item (SearchField field, const String & name, Item * parent) :
        field (field),
        name (name),
        folded (str_tolower_utf8 (name)),
        parent (parent) {}

    Item (Item &&) = default;
    Item & operator= (Item &&) = default;
};

class SearchModel : public QAbstractListModel
{
public:
    bool database_valid () const { return m_database_valid; }
    int num_items () const { return m_items.len (); }
    const Item & item_at (int idx) const { return * m_items[idx]; }
    int num_hidden_items () const { return m_hidden_items; }

    void update ();
    void destroy_database ();
    void create_database (Playlist playlist);
    void do_search (const Index<String> & terms, int max_results);

protected:
    int rowCount (const QModelIndex & parent) const { return m_rows; }
    int columnCount (const QModelIndex & parent) const { return 1; }
    QVariant data (const QModelIndex & index, int role) const;

    Qt::ItemFlags flags (const QModelIndex & index) const
    {
        if (index.isValid ())
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        else
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }

    QStringList mimeTypes () const
    {
        return QStringList ("text/uri-list");
    }

    QMimeData * mimeData (const QModelIndexList & indexes) const;

private:
    Playlist m_playlist;
    SimpleHash<Key, Item> m_database;
    bool m_database_valid = false;
    Index<const Item *> m_items;
    int m_hidden_items = 0;
    int m_rows = 0;
};

#endif // SEARCHMODEL_H
