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

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

static bool_t playlist_load_asx (const char * filename, VFSFile * file,
 char * * title, Index * filenames, Index * tuples)
{
    INIFile * inifile = inifile_read (file);
    if (! inifile)
        return FALSE;

    * title = NULL;

    for (int i = 1;; i ++)
    {
        SPRINTF (key, "ref%d", i);

        const char * val = inifile_lookup (inifile, "reference", key);
        if (! val)
            break;

        char * uri = aud_construct_uri (val, filename);
        if (! uri)
            continue;

        if (! strncmp ("http://", uri, 7))
            str_replace_fragment (uri, strlen (uri), "http://", "mms://");

        index_append (filenames, str_get (uri));
        free (uri);
    }

    inifile_destroy (inifile);
    return TRUE;
}

static bool_t playlist_save_asx (const char * filename, VFSFile * file,
 const char * title, Index * filenames, Index * tuples)
{
    int entries = index_count (filenames);

    vfs_fprintf (file, "[Reference]\r\n");

    for (int count = 0; count < entries; count ++)
    {
        const char * filename = index_get (filenames, count);
        char * fn;

        if (! strncmp (filename, "file://", 7))
            fn = uri_to_filename (filename);
        else
            fn = strdup (filename);

        if (! fn)
            continue;

        vfs_fprintf (file, "Ref%d=%s\r\n", 1 + count, fn);
        free (fn);
    }

    return TRUE;
}

static const char * const asx_exts[] = {"asx", NULL};

AUD_PLAYLIST_PLUGIN
(
    .name = N_("ASXv1/ASXv2 Playlists"),
    .domain = PACKAGE,
    .extensions = asx_exts,
    .load = playlist_load_asx,
    .save = playlist_save_asx
)
