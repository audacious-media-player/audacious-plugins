/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006-2010 William Pitcock, Tony Vroon, George Averill, Giacomo
 *  Lozito, Derek Pomery and Yoshiki Yazawa, and John Lindgren.
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
#include <stdlib.h>

#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

static void strip_char (gchar * text, gchar c)
{
    gchar * set = text;
    gchar a;

    while ((a = * text ++))
    {
        if (a != c)
            * set ++ = a;
    }

    * set = 0;
}

static gchar * read_win_text (const gchar * path)
{
    gchar * raw;
    gint64 size;

    vfs_file_get_contents (path, (void * *) & raw, & size);

    if (raw == NULL)
        return NULL;

    gchar * text = g_convert (raw, size, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
    g_free (raw);
    strip_char (text, '\r');
    return text;
}

static gchar * split_line (gchar * line)
{
    gchar * feed = strchr (line, '\n');

    if (feed == NULL)
        return NULL;

    * feed = 0;
    return feed + 1;
}

static gchar * convert_path (gchar * path, const gchar * base)
{
    gchar * pre = NULL;
    gchar * split;

    if ((split = strstr (path, "://")) != NULL)
    {
        pre = path;
        * split = 0;
        path = split + 3;
    }

    gchar * coded = string_encode_percent (path, TRUE);
    gchar buf[(pre != NULL ? strlen (pre) + 3 : 0) + strlen (coded) + 1];

    if (pre != NULL)
    {
        strcpy (buf, pre);
        strcpy (buf + strlen (pre), "://");
        strcpy (buf + strlen (pre) + 3, coded);
    }
    else
        strcpy (buf, coded);

    g_free (coded);
    return aud_construct_uri (buf, base);
}

static void playlist_load_m3u (const gchar * path, gint at)
{
    gchar * text = read_win_text (path);

    if (text == NULL)
        return;

    gchar * parse = text;
    struct index * add = index_new ();

    while (1)
    {
        gchar * next = split_line (parse);

        while (* parse == ' ' || * parse == '\t')
            parse ++;

        if (! * parse)
            break;

        if (* parse == '#')
            goto NEXT;

        gchar * s = convert_path (parse, path);

        if (s != NULL)
            index_append (add, s);

NEXT:
        parse = next;
    }

    aud_playlist_entry_insert_batch (aud_playlist_get_active (), at, add, NULL);
    g_free (text);
}

static void
playlist_save_m3u(const gchar *filename, gint pos)
{
    gint playlist = aud_playlist_get_active ();
    gint entries = aud_playlist_entry_count (playlist);
    gchar *outstr = NULL;
    VFSFile *file;
    gchar *fn = NULL;
    gint count;

    g_return_if_fail(filename != NULL);

    fn = g_filename_to_uri(filename, NULL, NULL);
    file = vfs_fopen(fn ? fn : filename, "wb");
    g_free(fn);
    g_return_if_fail(file != NULL);

    for (count = pos; count < entries; count ++)
    {
        const gchar * filename = aud_playlist_entry_get_filename (playlist,
         count);
        const gchar * title = aud_playlist_entry_get_title (playlist, count,
         FALSE);
        gint seconds = aud_playlist_entry_get_length (playlist, count, FALSE) /
         1000;

        if (title != NULL)
        {
            outstr = g_locale_from_utf8 (title, -1, NULL, NULL, NULL);

            if(outstr) {
                vfs_fprintf(file, "#EXTINF:%d,%s\n", seconds, outstr);
                g_free(outstr);
                outstr = NULL;
            }
            else
                vfs_fprintf (file, "#EXTINF:%d,%s\n", seconds, title);
        }

        fn = g_filename_from_uri (filename, NULL, NULL);
        vfs_fprintf (file, "%s\n", fn != NULL ? fn : filename);
        g_free(fn);
    }

    vfs_fclose(file);
}

PlaylistContainer plc_m3u = {
	.name = "M3U Playlist Format",
	.ext = "m3u",
	.plc_read = playlist_load_m3u,
	.plc_write = playlist_save_m3u,
};

static void init(void)
{
	aud_playlist_container_register(&plc_m3u);
}

static void cleanup(void)
{
	aud_playlist_container_unregister(&plc_m3u);
}

DECLARE_PLUGIN (m3u, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL)
