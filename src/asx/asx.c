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

#include <audacious/debug.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "util.h"

static gboolean playlist_load_asx (const gchar * filename,
 struct index * filenames, struct index * tuples)
{
    gint i;
    gchar line_key[16];
    gchar * line;
    gchar *uri = NULL;

    uri = g_filename_to_uri(filename, NULL, NULL);

    INIFile *inifile = open_ini_file(uri ? uri : filename);
    g_free(uri); uri = NULL;

    for (i = 1; ; i++) {
        g_snprintf(line_key, sizeof(line_key), "Ref%d", i);
        if ((line = read_ini_string(inifile, "Reference", line_key)))
        {
            gchar *uri;

            uri = aud_construct_uri(line, filename);
            g_free(line);

            if (!g_ascii_strncasecmp("http://", uri, 7))
                uri = str_replace_fragment(uri, strlen(uri), "http://", "mms://");

            if (uri != NULL)
                index_append (filenames, uri);
        }
        else
            break;
    }

    close_ini_file(inifile);
    return TRUE;
}

static gboolean playlist_save_asx (const gchar * filename,
 struct index * filenames, struct index * tuples)
{
    gint entries = index_count (filenames);
    gchar *uri = g_filename_to_uri(filename, NULL, NULL);
    VFSFile *file = vfs_fopen(uri, "wb");
    gint count;

    AUDDBG("filename=%s\n", filename);
    AUDDBG("uri=%s\n", uri);

    if (! file)
        return FALSE;

    vfs_fprintf(file, "[Reference]\r\n");

    for (count = 0; count < entries; count ++)
    {
        const gchar * filename = index_get (filenames, count);
        gchar *fn;

        if (vfs_is_remote (filename))
            fn = g_strdup (filename);
        else
            fn = g_filename_from_uri (filename, NULL, NULL);

        vfs_fprintf (file, "Ref%d=%s\r\n", 1 + count, fn);
        g_free(fn);
    }

    vfs_fclose(file);
    return TRUE;
}

static const gchar * const asx_exts[] = {"asx", NULL};

AUD_PLAYLIST_PLUGIN
(
 .name = "ASXv1/ASXv2 Playlist Format",
 .extensions = asx_exts,
 .load = playlist_load_asx,
 .save = playlist_save_asx
)
