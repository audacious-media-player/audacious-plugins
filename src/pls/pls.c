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

#include <audacious/main.h>
#include <audacious/util.h>
#include <audacious/playlist.h>
#include <audacious/playlist_container.h>
#include <audacious/plugin.h>
#include <audacious/strings.h>

static void
playlist_load_pls(const gchar * filename, gint pos)
{
    guint i, count, added_count = 0;
    gchar line_key[10], title_key[10];
    gchar *line, *title;
    Playlist *playlist = aud_playlist_get_active();
    gchar *uri = NULL;

    g_return_if_fail(filename != NULL);

    if (!aud_str_has_suffix_nocase(filename, ".pls"))
        return;

    uri = g_filename_to_uri(filename, NULL, NULL);

    INIFile *inifile = aud_open_ini_file(uri ? uri : filename);
    g_free(uri); uri = NULL;

    if (!(line = aud_read_ini_string(inifile, "playlist", "NumberOfEntries")))
    {
        aud_close_ini_file(inifile);
        return;
    }

    count = atoi(line);
    g_free(line);

    for (i = 1; i <= count; i++) {
        g_snprintf(line_key, sizeof(line_key), "File%d", i);
        if ((line = aud_read_ini_string(inifile, "playlist", line_key)))
        {
            gchar *uri = aud_construct_uri(line, filename);
            g_free(line);

            /* add file only if valid uri has been constructed */
            if (uri) {
                if (aud_cfg->use_pl_metadata)
                {
                    g_snprintf(title_key, sizeof(title_key), "Title%d", i);

                    if ((title = aud_read_ini_string(inifile, "playlist", title_key)))
                        aud_playlist_load_ins_file(playlist, uri, filename, pos, title, -1);
                    else
                        aud_playlist_load_ins_file(playlist, uri, filename, pos, NULL, -1);
                }
                else
                    aud_playlist_load_ins_file(playlist, uri, filename, pos, NULL, -1);

                added_count++;

                if (pos >= 0)
                    pos++;
            }
            g_free(uri);
        }
    }

    aud_close_ini_file(inifile);
}

static void
playlist_save_pls(const gchar *filename, gint pos)
{
    gchar *uri = g_filename_to_uri(filename, NULL, NULL);
    GList *node;
    VFSFile *file = aud_vfs_fopen(uri, "wb");
    Playlist *playlist = aud_playlist_get_active();

    AUDDBG("filename=%s\n", filename);
    AUDDBG("uri=%s\n", uri);

    g_return_if_fail(file != NULL);
    g_return_if_fail(playlist != NULL);

    aud_vfs_fprintf(file, "[playlist]\n");
    aud_vfs_fprintf(file, "NumberOfEntries=%d\n", aud_playlist_get_length(playlist));

    PLAYLIST_LOCK(playlist);

    for (node = playlist->entries; node; node = g_list_next(node)) {
        PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
        gchar *fn;

        if (aud_vfs_is_remote(entry->filename))
            fn = g_strdup(entry->filename);
        else
            fn = g_filename_from_uri(entry->filename, NULL, NULL);

        aud_vfs_fprintf(file, "File%d=%s\n", g_list_position(playlist->entries, node) + 1,
                    fn);

        g_free(fn);
    }

    PLAYLIST_UNLOCK(playlist);

    aud_vfs_fclose(file);
}

PlaylistContainer plc_pls = {
    .name = "Winamp .pls Playlist Format",
    .ext = "pls",
    .plc_read = playlist_load_pls,
    .plc_write = playlist_save_pls,
};

static void init(void)
{
    aud_playlist_container_register(&plc_pls);
}

static void cleanup(void)
{
    aud_playlist_container_unregister(&plc_pls);
}

DECLARE_PLUGIN(pls, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL);
