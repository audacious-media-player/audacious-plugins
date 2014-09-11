/*
 * asx.c
 * Copyright 2010-2013 William Pitcock and John Lindgren
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

typedef struct {
    const char * filename;
    bool valid_heading;
    Index<PlaylistAddItem> & items;
} ASXLoadState;

void asx_handle_heading (const char * heading, void * data)
{
    ASXLoadState * state = (ASXLoadState *) data;

    state->valid_heading = ! strcmp_nocase (heading, "reference");
}

void asx_handle_entry (const char * key, const char * value, void * data)
{
    ASXLoadState * state = (ASXLoadState *) data;

    if (! state->valid_heading || ! str_has_prefix_nocase (key, "ref"))
        return;

    StringBuf uri = uri_construct (value, state->filename);
    if (! uri)
        return;

    if (! strncmp ("http://", uri, 7))
        state->items.append ({String (str_printf ("mms://%s", uri + 7))});
    else
        state->items.append ({String (uri)});
}

static bool playlist_load_asx (const char * filename, VFSFile * file,
 String & title, Index<PlaylistAddItem> & items)
{
    ASXLoadState state = {
        filename,
        false,
        items
    };

    inifile_parse (file, asx_handle_heading, asx_handle_entry, & state);

    return (items.len () > 0);
}

static const char * const asx_exts[] = {"asx", nullptr};

#define AUD_PLUGIN_NAME        N_("ASXv1/ASXv2 Playlists")
#define AUD_PLAYLIST_EXTS      asx_exts
#define AUD_PLAYLIST_LOAD      playlist_load_asx

#define AUD_DECLARE_PLAYLIST
#include <libaudcore/plugin-declare.h>
