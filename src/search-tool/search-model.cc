/*
 * search-model.cc
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

#include "search-model.h"
#include <string.h>

void SearchModel::destroy_database ()
{
    m_playlist = Playlist ();
    m_items.clear ();
    m_hidden_items = 0;
    m_database.clear ();
}

void SearchModel::add_to_database (int entry, std::initializer_list<Key> keys)
{
    Item * parent = nullptr;
    auto hash = & m_database;

    for (auto & key : keys)
    {
        if (! key.name)
            continue;

        Item * item = hash->lookup (key);
        if (! item)
            item = hash->add (key, Item (key.field, key.name, parent));

        item->matches.append (entry);

        parent = item;
        hash = & item->children;
    }
}

void SearchModel::create_database (Playlist playlist)
{
    destroy_database ();

    int entries = playlist.n_entries ();

    for (int e = 0; e < entries; e ++)
    {
        Tuple tuple = playlist.entry_tuple (e, Playlist::NoWait);
        String album_artist = tuple.get_str (Tuple::AlbumArtist);
        String artist = tuple.get_str (Tuple::Artist);

        if (album_artist && album_artist != artist)
        {
            /* album and song have different artists;
             * add separately under respective artists */
            add_to_database (e,
             {{SearchField::Artist, album_artist},
              {SearchField::Album, tuple.get_str (Tuple::Album)}});
            /* add Title node under a HiddenAlbum node so that it can
             * still be searched by album name (without listing the
             * album twice) */
            add_to_database (e,
             {{SearchField::Artist, artist},
              {SearchField::HiddenAlbum, tuple.get_str (Tuple::Album)},
              {SearchField::Title, tuple.get_str (Tuple::Title)}});
        }
        else
        {
            /* album and song have the same artist;
             * add hierarchically under that artist */
            add_to_database (e,
             {{SearchField::Artist, artist},
              {SearchField::Album, tuple.get_str (Tuple::Album)},
              {SearchField::Title, tuple.get_str (Tuple::Title)}});
        }

        /* add separately under genre */
        add_to_database (e,
         {{SearchField::Genre, tuple.get_str (Tuple::Genre)}});
    }

    m_playlist = playlist;
}

static void search_recurse (SimpleHash<Key, Item> & domain,
 const Index<String> & terms, int mask, Index<const Item *> & results)
{
    domain.iterate ([&] (const Key & key, Item & item)
    {
        int count = terms.len ();
        int new_mask = mask;

        for (int t = 0, bit = 1; t < count; t ++, bit <<= 1)
        {
            if (! (new_mask & bit))
                continue; /* skip term if it is already found */

            if (strstr (item.folded, terms[t]))
                new_mask &= ~bit; /* we found it */
            else if (! item.children.n_items ())
                break; /* quit early if there are no children to search */
        }

        /* adding an item with exactly one child is redundant, so avoid it */
        if (! new_mask && item.children.n_items () != 1 &&
         item.field != SearchField::HiddenAlbum)
            results.append (& item);

        search_recurse (item.children, terms, new_mask, results);
    });
}

static int item_compare (const Item * const & a, const Item * const & b)
{
    if (a->field < b->field)
        return -1;
    if (a->field > b->field)
        return 1;

    int val = str_compare (a->name, b->name);
    if (val)
        return val;

    if (a->parent)
        return b->parent ? item_compare (a->parent, b->parent) : 1;
    else
        return b->parent ? -1 : 0;
}

static int item_compare_pass1 (const Item * const & a, const Item * const & b)
{
    if (a->matches.len () > b->matches.len ())
        return -1;
    if (a->matches.len () < b->matches.len ())
        return 1;

    return item_compare (a, b);
}

void SearchModel::do_search (const Index<String> & terms, int max_results)
{
    m_items.clear ();
    m_hidden_items = 0;

    /* effectively limits number of search terms to 32 */
    search_recurse (m_database, terms, (1 << terms.len ()) - 1, m_items);

    /* first sort by number of songs per item */
    m_items.sort (item_compare_pass1);

    /* limit to items with most songs */
    if (m_items.len () > max_results)
    {
        m_hidden_items = m_items.len () - max_results;
        m_items.remove (max_results, -1);
    }

    /* sort by item type, then item name */
    m_items.sort (item_compare);
}
