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

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

typedef struct {
    const char * filename;
    bool valid_heading;
    Index<PlaylistAddItem> & items;
} PLSLoadState;

void pls_handle_heading (const char * heading, void * data)
{
    PLSLoadState * state = (PLSLoadState *) data;

    state->valid_heading = ! g_ascii_strcasecmp (heading, "playlist");
}

void pls_handle_entry (const char * key, const char * value, void * data)
{
    PLSLoadState * state = (PLSLoadState *) data;

    if (! state->valid_heading || g_ascii_strncasecmp (key, "file", 4))
        return;

    StringBuf uri = uri_construct (value, state->filename);
    if (uri)
        state->items.append ({String (uri)});
}

static bool playlist_load_pls (const char * filename, VFSFile * file,
 String & title, Index<PlaylistAddItem> & items)
{
    PLSLoadState state = {
        filename,
        false,
        items
    };

    inifile_parse (file, pls_handle_heading, pls_handle_entry, & state);

    return (items.len () > 0);
}

static bool playlist_save_pls (const char * filename, VFSFile * file,
 const char * title, const Index<PlaylistAddItem> & items)
{
    int entries = items.len ();

    vfs_fprintf (file, "[playlist]\n");
    vfs_fprintf (file, "NumberOfEntries=%d\n", entries);

    for (int count = 0; count < entries; count ++)
    {
        const char * uri = items[count].filename;
        StringBuf local = uri_to_filename (uri);
        vfs_fprintf (file, "File%d=%s\n", 1 + count, local ? local : uri);
    }

    return true;
}

static const char * const pls_exts[] = {"pls", nullptr};

#define AUD_PLUGIN_NAME        N_("PLS Playlists")
#define AUD_PLAYLIST_EXTS      pls_exts
#define AUD_PLAYLIST_LOAD      playlist_load_pls
#define AUD_PLAYLIST_SAVE      playlist_save_pls

#define AUD_DECLARE_PLAYLIST
#include <libaudcore/plugin-declare.h>
