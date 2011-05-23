/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
 * Copyright (c) 2011 John Lindgren
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

/* #define AUD_DEBUG 1 */

#include <glib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <audacious/debug.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "util.h"

static gboolean playlist_load_pls (const gchar * filename, VFSFile * file,
 gchar * * title, struct index * filenames, struct index * tuples)
{
    gint i, count;
    gchar line_key[16];
    gchar * line;

    INIFile * inifile = open_ini_file (file);

    * title = NULL;

    if (!(line = read_ini_string(inifile, "playlist", "NumberOfEntries")))
    {
        close_ini_file(inifile);
        return FALSE;
    }

    count = atoi(line);
    g_free(line);

    for (i = 1; i <= count; i++) {
        g_snprintf(line_key, sizeof(line_key), "File%d", i);
        if ((line = read_ini_string(inifile, "playlist", line_key)))
        {
            gchar *uri = aud_construct_uri(line, filename);
            g_free(line);

            if (uri != NULL)
                index_append (filenames, uri);
        }
    }

    close_ini_file(inifile);
    return TRUE;
}

static gboolean playlist_save_pls (const gchar * filename, VFSFile * file,
 const gchar * title, struct index * filenames, struct index * tuples)
{
    gint entries = index_count (filenames);
    gint count;

    vfs_fprintf(file, "[playlist]\n");
    vfs_fprintf(file, "NumberOfEntries=%d\n", entries);

    for (count = 0; count < entries; count ++)
    {
        const gchar * filename = index_get (filenames, count);
        gchar *fn;

        if (vfs_is_remote (filename))
            fn = g_strdup (filename);
        else
            fn = g_filename_from_uri (filename, NULL, NULL);

        vfs_fprintf (file, "File%d=%s\n", 1 + count, fn);
        g_free(fn);
    }

    return TRUE;
}

static const gchar * const pls_exts[] = {"pls", NULL};

AUD_PLAYLIST_PLUGIN
(
 .name = "PLS Playlist Format",
 .extensions = pls_exts,
 .load = playlist_load_pls,
 .save = playlist_save_pls
)
