/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
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
#include <sys/errno.h>

#include <audacious/debug.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "util.h"

static gboolean playlist_load_pls (const gchar * filename, gint list, gint pos)
{
    gint i, count;
    gchar line_key[16];
    gchar * line;
    gchar *uri = NULL;
    struct index * add;

    uri = g_filename_to_uri(filename, NULL, NULL);

    INIFile *inifile = open_ini_file(uri ? uri : filename);
    g_free(uri); uri = NULL;

    if (!(line = read_ini_string(inifile, "playlist", "NumberOfEntries")))
    {
        close_ini_file(inifile);
        return FALSE;
    }

    count = atoi(line);
    g_free(line);

    add = index_new ();

    for (i = 1; i <= count; i++) {
        g_snprintf(line_key, sizeof(line_key), "File%d", i);
        if ((line = read_ini_string(inifile, "playlist", line_key)))
        {
            gchar *uri = aud_construct_uri(line, filename);
            g_free(line);

            if (uri != NULL)
                index_append (add, uri);
        }
    }

    close_ini_file(inifile);

    aud_playlist_entry_insert_batch (list, pos, add, NULL);
    return TRUE;
}

static gboolean playlist_save_pls (const gchar * filename, gint playlist)
{
    gint entries = aud_playlist_entry_count (playlist);
    gchar *uri = g_filename_to_uri(filename, NULL, NULL);
    VFSFile *file = vfs_fopen(uri, "wb");
    gint count;

    AUDDBG("filename=%s\n", filename);
    AUDDBG("uri=%s\n", uri);

    if (! file)
        return FALSE;

    vfs_fprintf(file, "[playlist]\n");
    vfs_fprintf(file, "NumberOfEntries=%d\n", entries);

    for (count = 0; count < entries; count ++)
    {
        const gchar * filename = aud_playlist_entry_get_filename (playlist,
         count);
        gchar *fn;

        if (vfs_is_remote (filename))
            fn = g_strdup (filename);
        else
            fn = g_filename_from_uri (filename, NULL, NULL);

        vfs_fprintf (file, "File%d=%s\n", 1 + count, fn);
        g_free(fn);
    }

    vfs_fclose(file);
    return TRUE;
}

static const gchar * const pls_exts[] = {"pls", NULL};

static PlaylistPlugin pls_plugin = {
 .description = "PLS Playlist Format",
 .extensions = pls_exts,
 .load = playlist_load_pls,
 .save = playlist_save_pls
};

static PlaylistPlugin * const pls_plugins[] = {& pls_plugin, NULL};

SIMPLE_PLAYLIST_PLUGIN (pls, pls_plugins)
