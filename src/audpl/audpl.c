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
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

typedef struct {
    char * * title;
    Index * filenames;
    Index * tuples;
    char * uri;
    Tuple * tuple;
} LoadState;

static void finish_item (LoadState * state)
{
    index_insert (state->filenames, -1, state->uri);
    index_insert (state->tuples, -1, state->tuple);
    state->uri = NULL;
    state->tuple = NULL;
}

static void handle_heading (const char * heading, void * data)
{
    /* no headings */
}

static void handle_entry (const char * key, const char * value, void * data)
{
    LoadState * state = data;

    if (! strcmp (key, "uri"))
    {
        /* finish previous item */
        if (state->uri)
            finish_item (state);

        /* start new item */
        state->uri = str_get (value);
    }
    else if (state->uri)
    {
        /* item field */
        if (! state->tuple)
            state->tuple = tuple_new_from_filename (state->uri);

        if (strcmp (key, "empty"))
        {
            int field = tuple_field_by_name (key);
            if (field < 0)
                return;

            TupleValueType type = tuple_field_get_type (field);

            if (type == TUPLE_STRING)
            {
                char buf[strlen (value) + 1];
                str_decode_percent (value, -1, buf);
                tuple_set_str (state->tuple, field, buf);
            }
            else if (type == TUPLE_INT)
                tuple_set_int (state->tuple, field, atoi (value));
        }
    }
    else
    {
        /* top-level field */
        if (! strcmp (key, "title") && ! * state->title)
        {
            char buf[strlen (value) + 1];
            str_decode_percent (value, -1, buf);
            * state->title = str_get (buf);
        }
    }
}

static bool_t audpl_load (const char * path, VFSFile * file, char * * title,
 Index * filenames, Index * tuples)
{
    LoadState state = {
        .title = title,
        .filenames = filenames,
        .tuples = tuples
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
 const char * title, Index * filenames, Index * tuples)
{
    if (! write_encoded_entry (file, "title", title))
        return FALSE;

    int count = index_count (filenames);

    for (int i = 0; i < count; i ++)
    {
        if (! inifile_write_entry (file, "uri", index_get (filenames, i)))
            return FALSE;

        const Tuple * tuple = tuples ? index_get (tuples, i) : NULL;

        if (tuple)
        {
            int keys = 0;

            for (int f = 0; f < TUPLE_FIELDS; f ++)
            {
                if (f == FIELD_FILE_PATH || f == FIELD_FILE_NAME || f == FIELD_FILE_EXT)
                    continue;

                TupleValueType type = tuple_get_value_type (tuple, f);

                if (type == TUPLE_STRING)
                {
                    char * str = tuple_get_str (tuple, f);

                    if (! write_encoded_entry (file, tuple_field_get_name (f), str))
                    {
                        str_unref (str);
                        return FALSE;
                    }

                    str_unref (str);
                    keys ++;
                }
                else if (type == TUPLE_INT)
                {
                    char buf[32];
                    str_itoa (tuple_get_int (tuple, f), buf, sizeof buf);

                    if (! inifile_write_entry (file, tuple_field_get_name (f), buf))
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

AUD_PLAYLIST_PLUGIN
(
    .name = N_("Audacious Playlists (audpl)"),
    .domain = PACKAGE,
    .extensions = audpl_exts,
    .load = audpl_load,
    .save = audpl_save
)
