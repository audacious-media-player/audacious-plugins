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
#include <strings.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

typedef struct {
    const char * filename;
    bool_t valid_heading;
    Index * filenames;
} ASXLoadState;

void asx_handle_heading (const char * heading, void * data)
{
    ASXLoadState * state = data;

    state->valid_heading = ! strcasecmp (heading, "reference");
}

void asx_handle_entry (const char * key, const char * value, void * data)
{
    ASXLoadState * state = data;

    if (! state->valid_heading || strncasecmp (key, "ref", 3))
        return;

    char * uri = aud_construct_uri (value, state->filename);
    if (! uri)
        return;

    if (! strncmp ("http://", uri, 7))
        str_replace_fragment (uri, strlen (uri), "http://", "mms://");

    index_append (state->filenames, str_get (uri));
    free (uri);
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

AUD_PLAYLIST_PLUGIN
(
    .name = N_("ASXv1/ASXv2 Playlists"),
    .domain = PACKAGE,
    .extensions = asx_exts,
    .load = playlist_load_asx
)
