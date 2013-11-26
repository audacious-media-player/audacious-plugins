/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
 * Copyright (c) 2011-2013 John Lindgren
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
} PLSLoadState;

void pls_handle_heading (const char * heading, void * data)
{
    PLSLoadState * state = data;

    state->valid_heading = ! strcasecmp (heading, "playlist");
}

void pls_handle_entry (const char * key, const char * value, void * data)
{
    PLSLoadState * state = data;

    if (! state->valid_heading || strncasecmp (key, "file", 4))
        return;

    char * uri = aud_construct_uri (value, state->filename);
    if (! uri)
        return;

    index_append (state->filenames, str_get (uri));
    free (uri);
}

static bool_t playlist_load_pls (const char * filename, VFSFile * file,
 char * * title, Index * filenames, Index * tuples)
{
    PLSLoadState state = {
        .filename = filename,
        .valid_heading = FALSE,
        .filenames = filenames
    };

    inifile_parse (file, pls_handle_heading, pls_handle_entry, & state);

    return (index_count (filenames) != 0);
}

static bool_t playlist_save_pls (const char * filename, VFSFile * file,
                                   const char * title, Index * filenames, Index * tuples)
{
    int entries = index_count (filenames);

    vfs_fprintf (file, "[playlist]\n");
    vfs_fprintf (file, "NumberOfEntries=%d\n", entries);

    for (int count = 0; count < entries; count ++)
    {
        const char * filename = index_get (filenames, count);
        char * fn;

        if (! strncmp (filename, "file://", 7))
            fn = uri_to_filename (filename);
        else
            fn = strdup (filename);

        vfs_fprintf (file, "File%d=%s\n", 1 + count, fn);
        free (fn);
    }

    return TRUE;
}

static const char * const pls_exts[] = {"pls", NULL};

AUD_PLAYLIST_PLUGIN
(
    .name = N_("PLS Playlists"),
    .domain = PACKAGE,
    .extensions = pls_exts,
    .load = playlist_load_pls,
    .save = playlist_save_pls
)
