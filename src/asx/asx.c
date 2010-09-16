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

static void
playlist_load_asx(const gchar * filename, gint pos)
{
    gint i;
    gchar line_key[16];
    gchar * line;
    gchar *uri = NULL;
    struct index * add;

    g_return_if_fail(filename != NULL);

    if (!str_has_suffix_nocase(filename, ".asx"))
       return;

    uri = g_filename_to_uri(filename, NULL, NULL);

    INIFile *inifile = open_ini_file(uri ? uri : filename);
    g_free(uri); uri = NULL;

    add = index_new ();

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
                index_append (add, uri);
        }
        else
            break;
    }

    close_ini_file(inifile);

    aud_playlist_entry_insert_batch (aud_playlist_get_active (), pos, add, NULL);
}

static void
playlist_save_asx(const gchar *filename, gint pos)
{
    gint playlist = aud_playlist_get_active ();
    gint entries = aud_playlist_entry_count (playlist);
    gchar *uri = g_filename_to_uri(filename, NULL, NULL);
    VFSFile *file = vfs_fopen(uri, "wb");
    gint count;

    AUDDBG("filename=%s\n", filename);
    AUDDBG("uri=%s\n", uri);

    g_return_if_fail(file != NULL);

    vfs_fprintf(file, "[Reference]\r\n");

    for (count = pos; count < entries; count ++)
    {
        const gchar * filename = aud_playlist_entry_get_filename (playlist,
         count);
        gchar *fn;

        if (vfs_is_remote (filename))
            fn = g_strdup (filename);
        else
            fn = g_filename_from_uri (filename, NULL, NULL);

        vfs_fprintf (file, "Ref%d=%s\r\n", 1 + pos + count, fn);
        g_free(fn);
    }

    vfs_fclose(file);
}

PlaylistContainer plc_asx = {
    .name = "ASXv1/ASXv2 playlist format",
    .ext = "asx",
    .plc_read = playlist_load_asx,
    .plc_write = playlist_save_asx,
};

static void init(void)
{
    aud_playlist_container_register(&plc_asx);
}

static void cleanup(void)
{
    aud_playlist_container_unregister(&plc_asx);
}

DECLARE_PLUGIN (asx, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL)
