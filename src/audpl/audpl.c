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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

typedef struct {
    VFSFile * file;
    char * cur;
    int len;
    char buf[65536];
} ReadState;

static bool_t read_key_raw (ReadState * state, char * * keyp, char * * valp)
{
    char * newline = memchr (state->cur, '\n', state->len);

    if (! newline)
    {
        memmove (state->buf, state->cur, state->len);
        state->cur = state->buf;
        state->len += vfs_fread (state->buf + state->len, 1,
         sizeof state->buf - state->len, state->file);

        newline = memchr (state->cur, '\n', state->len);

        if (! newline)
            return FALSE;
    }

    * newline = 0;

    char * equals = strchr (state->cur, '=');

    if (! equals)
        return FALSE;

    * equals = 0;

    * keyp = state->cur;
    * valp = equals + 1;

    state->len -= newline + 1 - state->cur;
    state->cur = newline + 1;

    return TRUE;
}

static bool_t read_key (ReadState * state, char * * keyp, char * * valp)
{
    if (! read_key_raw (state, keyp, valp))
        return FALSE;

    if (strcmp (* keyp, "uri"))
        str_decode_percent (* valp, -1, * valp);

    return TRUE;
}

static bool_t write_key_raw (VFSFile * file, const char * key, const char * val)
{
    int keylen = strlen (key);
    int vallen = strlen (val);
    char buf[keylen + vallen + 2];

    memcpy (buf, key, keylen);
    buf[keylen] = '=';
    memcpy (buf + keylen + 1, val, vallen);
    buf[keylen + vallen + 1] = '\n';

    return (vfs_fwrite (buf, 1, keylen + vallen + 2, file) == keylen + vallen + 2);
}

static bool_t write_key (VFSFile * file, const char * key, const char * val)
{
    if (! strcmp (key, "uri"))
        return write_key_raw (file, key, val);

    char buf[3 * strlen (val) + 1];
    str_encode_percent (val, -1, buf);
    return write_key_raw (file, key, buf);
}

static bool_t audpl_load (const char * path, VFSFile * file, char * * title,
 Index * filenames, Index * tuples)
{
    ReadState * state = malloc (sizeof (ReadState));
    state->file = file;
    state->cur = state->buf;
    state->len = 0;

    char * key, * val;

    if (! read_key (state, & key, & val) || strcmp (key, "title"))
    {
        free (state);
        return FALSE;
    }

    * title = str_get (val);

    bool_t readed = read_key (state, & key, & val);

    while (readed && ! strcmp (key, "uri"))
    {
        char * uri = str_get (val);
        Tuple * tuple = NULL;

        while ((readed = read_key (state, & key, & val)) && strcmp (key, "uri"))
        {
            if (! tuple)
                tuple = tuple_new_from_filename (uri);

            if (! strcmp (key, "empty"))
                continue;

            int field = tuple_field_by_name (key);
            TupleValueType type = tuple_field_get_type (field);

            if (field < 0)
                break;

            if (type == TUPLE_STRING)
                tuple_set_str (tuple, field, NULL, val);
            else if (type == TUPLE_INT)
                tuple_set_int (tuple, field, NULL, atoi (val));
        }

        index_insert (filenames, -1, uri);
        index_insert (tuples, -1, tuple);
    }

    free (state);
    return TRUE;
}

static bool_t audpl_save (const char * path, VFSFile * file,
 const char * title, Index * filenames, Index * tuples)
{
    if (! write_key (file, "title", title))
        return FALSE;

    int count = index_count (filenames);

    for (int i = 0; i < count; i ++)
    {
        if (! write_key (file, "uri", index_get (filenames, i)))
            return FALSE;

        const Tuple * tuple = tuples ? index_get (tuples, i) : NULL;

        if (tuple)
        {
            int keys = 0;

            for (int f = 0; f < TUPLE_FIELDS; f ++)
            {
                if (f == FIELD_FILE_PATH || f == FIELD_FILE_NAME || f == FIELD_FILE_EXT)
                    continue;

                TupleValueType type = tuple_get_value_type (tuple, f, NULL);

                if (type == TUPLE_STRING)
                {
                    char * str = tuple_get_str (tuple, f, NULL);

                    if (! write_key (file, tuple_field_get_name (f), str))
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
                    snprintf (buf, sizeof buf, "%d", tuple_get_int (tuple, f, NULL));

                    if (! write_key (file, tuple_field_get_name (f), buf))
                        return FALSE;

                    keys ++;
                }
            }

            /* distinguish between an empty tuple and no tuple at all */
            if (! keys && ! write_key (file, "empty", "1"))
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
