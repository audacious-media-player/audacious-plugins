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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

typedef struct {
    const char * filename;
    bool_t valid_heading;
    Index * filenames;
} ASXLoadState;

void asx_handle_heading (const char * heading, void * data)
{
    ASXLoadState * state = (ASXLoadState *) data;

    state->valid_heading = ! g_ascii_strcasecmp (heading, "reference");
}

void asx_handle_entry (const char * key, const char * value, void * data)
{
    ASXLoadState * state = (ASXLoadState *) data;

    if (! state->valid_heading || g_ascii_strncasecmp (key, "ref", 3))
        return;

    char * uri = uri_construct (value, state->filename);
    if (! uri)
        return;

    if (! strncmp ("http://", uri, 7))
    {
        index_insert (state->filenames, -1, str_printf ("mms://%s", uri + 7));
        str_unref (uri);
    }
    else
        index_insert (state->filenames, -1, uri);
}

static bool_t playlist_load_asx (const char * filename, VFSFile * file,
 char * * title, Index * filenames, Index * tuples)
{
    ASXLoadState state = {
        .filename = filename,
        .valid_heading = FALSE,
        .filenames = filenames
    };

    inifile_parse (file, asx_handle_heading, asx_handle_entry, & state);

    return (index_count (filenames) != 0);
}

static const char * const asx_exts[] = {"asx", NULL};

#define AUD_PLUGIN_NAME        N_("ASXv1/ASXv2 Playlists")
#define AUD_PLAYLIST_EXTS      asx_exts
#define AUD_PLAYLIST_LOAD      playlist_load_asx

#define AUD_DECLARE_PLAYLIST
#include <libaudcore/plugin-declare.h>
