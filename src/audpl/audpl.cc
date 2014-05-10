/*
 * Audacious playlist format plugin
 * Copyright 2011 John Lindgren
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

#include <stdlib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

typedef struct {
    String & title;
    Index<PlaylistAddItem> & items;
    String uri;
    Tuple tuple;
} LoadState;

static void finish_item (LoadState * state)
{
    state->items.append ({std::move (state->uri), std::move (state->tuple)});
}

static void handle_heading (const char * heading, void * data)
{
    /* no headings */
}

static void handle_entry (const char * key, const char * value, void * data)
{
    LoadState * state = (LoadState *) data;

    if (! strcmp (key, "uri"))
    {
        /* finish previous item */
        if (state->uri)
            finish_item (state);

        /* start new item */
        state->uri = String (value);
    }
    else if (state->uri)
    {
        /* item field */
        if (! state->tuple)
            state->tuple.set_filename (state->uri);

        if (strcmp (key, "empty"))
        {
            int field = Tuple::field_by_name (key);
            if (field < 0)
                return;

            TupleValueType type = Tuple::field_get_type (field);

            if (type == TUPLE_STRING)
            {
                char buf[strlen (value) + 1];
                str_decode_percent (value, -1, buf);
                state->tuple.set_str (field, buf);
            }
            else if (type == TUPLE_INT)
                state->tuple.set_int (field, atoi (value));
        }
    }
    else
    {
        /* top-level field */
        if (! strcmp (key, "title") && ! state->title)
        {
            char buf[strlen (value) + 1];
            str_decode_percent (value, -1, buf);
            state->title = String (buf);
        }
    }
}

static bool_t audpl_load (const char * path, VFSFile * file, String & title,
 Index<PlaylistAddItem> & items)
{
    LoadState state = {
        title,
        items
    };

    inifile_parse (file, handle_heading, handle_entry, & state);

    /* finish last item */
    if (state.uri)
        finish_item (& state);

    return TRUE;
}

static bool_t write_encoded_entry (VFSFile * file, const char * key, const char * val)
{
    char buf[3 * strlen (val) + 1];
    str_encode_percent (val, -1, buf);
    return inifile_write_entry (file, key, buf);
}

static bool_t audpl_save (const char * path, VFSFile * file,
 const char * title, const Index<PlaylistAddItem> & items)
{
    if (! write_encoded_entry (file, "title", title))
        return FALSE;

    for (auto & item : items)
    {
        if (! inifile_write_entry (file, "uri", item.filename))
            return FALSE;

        const Tuple & tuple = item.tuple;

        if (tuple)
        {
            int keys = 0;

            for (int f = 0; f < TUPLE_FIELDS; f ++)
            {
                if (f == FIELD_FILE_PATH || f == FIELD_FILE_NAME || f == FIELD_FILE_EXT)
                    continue;

                TupleValueType type = tuple.get_value_type (f);

                if (type == TUPLE_STRING)
                {
                    String str = tuple.get_str (f);

                    if (! write_encoded_entry (file, Tuple::field_get_name (f), str))
                        return FALSE;

                    keys ++;
                }
                else if (type == TUPLE_INT)
                {
                    char buf[32];
                    str_itoa (tuple.get_int (f), buf, sizeof buf);

                    if (! inifile_write_entry (file, Tuple::field_get_name (f), buf))
                        return FALSE;

                    keys ++;
                }
            }

            /* distinguish between an empty tuple and no tuple at all */
            if (! keys && ! inifile_write_entry (file, "empty", "1"))
                return FALSE;
        }
    }

    return TRUE;
}

static const char * const audpl_exts[] = {"audpl", NULL};

#define AUD_PLUGIN_NAME        N_("Audacious Playlists (audpl)")
#define AUD_PLAYLIST_EXTS      audpl_exts
#define AUD_PLAYLIST_LOAD      audpl_load
#define AUD_PLAYLIST_SAVE      audpl_save

#define AUD_DECLARE_PLAYLIST
#include <libaudcore/plugin-declare.h>
