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
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

static const char * const audpl_exts[] = {"audpl"};

class AudPlaylistLoader : public PlaylistPlugin
{
public:
    const PluginInfo info = {N_("Audacious Playlists (audpl)"), PACKAGE};

    AudPlaylistLoader () : PlaylistPlugin (info, audpl_exts, true) {}

    bool load (const char * filename, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items);
    bool save (const char * filename, VFSFile & file, const char * title,
     const Index<PlaylistAddItem> & items);

private:
    struct LoadState {
        String & title;
        Index<PlaylistAddItem> & items;
        String uri;
        Tuple tuple;
    };

    static void finish_item (LoadState * state);
    static void handle_heading (const char * heading, void * data);
    static void handle_entry (const char * key, const char * value, void * data);
};

AudPlaylistLoader aud_plugin_instance;

void AudPlaylistLoader::finish_item (LoadState * state)
{
    state->items.append (std::move (state->uri), std::move (state->tuple));
}

void AudPlaylistLoader::handle_heading (const char * heading, void * data)
{
    /* no headings */
}

void AudPlaylistLoader::handle_entry (const char * key, const char * value, void * data)
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
                state->tuple.set_str (field, str_decode_percent (value));
            else if (type == TUPLE_INT)
                state->tuple.set_int (field, atoi (value));
        }
    }
    else
    {
        /* top-level field */
        if (! strcmp (key, "title") && ! state->title)
            state->title = String (str_decode_percent (value));
    }
}

bool AudPlaylistLoader::load (const char * path, VFSFile & file, String & title,
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

    return true;
}

bool AudPlaylistLoader::save (const char * path, VFSFile & file,
 const char * title, const Index<PlaylistAddItem> & items)
{
    if (! inifile_write_entry (file, "title", str_encode_percent (title)))
        return false;

    for (auto & item : items)
    {
        if (! inifile_write_entry (file, "uri", item.filename))
            return false;

        const Tuple & tuple = item.tuple;

        if (tuple)
        {
            int keys = 0;

            for (int f = 0; f < TUPLE_FIELDS; f ++)
            {
                if (f == FIELD_FILE_PATH || f == FIELD_FILE_NAME || f == FIELD_FILE_EXT)
                    continue;

                const char * key = Tuple::field_get_name (f);
                TupleValueType type = tuple.get_value_type (f);

                if (type == TUPLE_STRING)
                {
                    String str = tuple.get_str (f);
                    if (! inifile_write_entry (file, key, str_encode_percent (str)))
                        return false;

                    keys ++;
                }
                else if (type == TUPLE_INT)
                {
                    int val = tuple.get_int (f);
                    if (! inifile_write_entry (file, key, int_to_str (val)))
                        return false;

                    keys ++;
                }
            }

            /* distinguish between an empty tuple and no tuple at all */
            if (! keys && ! inifile_write_entry (file, "empty", "1"))
                return false;
        }
    }

    return true;
}
