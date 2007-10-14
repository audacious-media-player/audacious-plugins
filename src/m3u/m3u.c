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

#include <audacious/plugin.h>
#include <audacious/main.h>
#include <audacious/util.h>
#include <audacious/playlist.h>
#include <audacious/playlist_container.h>
#include <audacious/strings.h>

static void
parse_extm3u_info(const gchar * info, gchar ** title, gint * length)
{
    gchar *str;
    gchar *temp;

    g_return_if_fail(info != NULL);
    g_return_if_fail(title != NULL);
    g_return_if_fail(length != NULL);

    *title = NULL;
    *length = -1;

    if (!aud_str_has_prefix_nocase(info, "#EXTINF:")) {
        g_message("Invalid m3u metadata (%s)", info);
        return;
    }

    info += 8;

    *length = atoi(info);
    if (*length <= 0)
        *length = -1;
    else
        *length *= 1000;

    if ((str = strchr(info, ','))) {
        temp = g_strstrip(g_strdup(str + 1));
        if (strlen(temp) > 0)
        	*title = g_locale_to_utf8(temp, -1, NULL, NULL, NULL);
        
        g_free(temp);
        temp = NULL;
    }
}

static void
playlist_load_m3u(const gchar * filename, gint pos)
{
    VFSFile *file;
    gchar *line;
    gchar *ext_info = NULL, *ext_title = NULL;
    gsize line_len = 1024;
    gint ext_len = -1;
    gboolean is_extm3u = FALSE;
    Playlist *playlist = playlist_get_active();
    gchar *uri;
    
    uri = g_filename_to_uri(filename, NULL, NULL);

    if ((file = aud_vfs_fopen(uri ? uri : filename, "rb")) == NULL)
        return;

    g_free(uri);

    line = g_malloc(line_len);
    while (aud_vfs_fgets(line, line_len, file)) {
        while (strlen(line) == line_len - 1 && line[strlen(line) - 1] != '\n') {
            line_len += 1024;
            line = g_realloc(line, line_len);
            aud_vfs_fgets(&line[strlen(line)], 1024, file);
        }

        while (line[strlen(line) - 1] == '\r' ||
               line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';

        if (aud_str_has_prefix_nocase(line, "#EXTM3U")) {
            is_extm3u = TRUE;
            continue;
        }

        if (is_extm3u && aud_str_has_prefix_nocase(line, "#EXTINF:")) {
            aud_str_replace_in(&ext_info, g_strdup(line));
            continue;
        }

        if (line[0] == '#' || strlen(line) == 0) {
            if (ext_info) {
                g_free(ext_info);
                ext_info = NULL;
            }
            continue;
        }

        if (is_extm3u) {
            if (cfg.use_pl_metadata && ext_info)
                parse_extm3u_info(ext_info, &ext_title, &ext_len);
            g_free(ext_info);
            ext_info = NULL;
        }

        uri = g_filename_to_uri(line, NULL, NULL);
        playlist_load_ins_file(playlist, uri ? uri : line, filename, pos, ext_title, ext_len);
        g_free(uri);

        aud_str_replace_in(&ext_title, NULL);
        ext_len = -1;

        if (pos >= 0)
            pos++;
    }

    aud_vfs_fclose(file);
    g_free(line);
}

static void
playlist_save_m3u(const gchar *filename, gint pos)
{
    GList *node;
    gchar *outstr = NULL;
    VFSFile *file;
    Playlist *playlist = playlist_get_active();
    gchar *fn = NULL;

    g_return_if_fail(filename != NULL);
    g_return_if_fail(playlist != NULL);

    fn = g_filename_to_uri(filename, NULL, NULL);
    file = aud_vfs_fopen(fn ? fn : filename, "wb");
    g_free(fn);
    g_return_if_fail(file != NULL);

    if (cfg.use_pl_metadata)
        aud_vfs_fprintf(file, "#EXTM3U\n");

    PLAYLIST_LOCK(playlist);

    for (node = playlist->entries; node; node = g_list_next(node)) {
        PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);

        if (entry->title && cfg.use_pl_metadata) {
            gint seconds;

            if (entry->length > 0)
                seconds = (entry->length) / 1000;
            else
                seconds = -1;

            outstr = g_locale_from_utf8(entry->title, -1, NULL, NULL, NULL);
            if(outstr) {
                aud_vfs_fprintf(file, "#EXTINF:%d,%s\n", seconds, outstr);
                g_free(outstr);
                outstr = NULL;
            } else {
                aud_vfs_fprintf(file, "#EXTINF:%d,%s\n", seconds, entry->title);
            }
        }

        fn = g_filename_from_uri(entry->filename, NULL, NULL);
        aud_vfs_fprintf(file, "%s\n", fn ? fn : entry->filename);
        g_free(fn);
    }

    PLAYLIST_UNLOCK(playlist);

    aud_vfs_fclose(file);
}

PlaylistContainer plc_m3u = {
	.name = "M3U Playlist Format",
	.ext = "m3u",
	.plc_read = playlist_load_m3u,
	.plc_write = playlist_save_m3u,
};

static void init(void)
{
	playlist_container_register(&plc_m3u);
}

static void cleanup(void)
{
	playlist_container_unregister(&plc_m3u);
}

DECLARE_PLUGIN(m3u, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL);
