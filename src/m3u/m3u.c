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
        if (a != c)
            * set ++ = a;

    * set = 0;
}

static gchar * read_win_text (VFSFile * file)
{
    gint64 size = vfs_fsize (file);
    if (size < 1)
        return NULL;

    gchar * raw = g_malloc (size + 1);
    size = vfs_fread (raw, 1, size, file);
    raw[size] = 0;

    strip_char (raw, '\r');
    gchar * text = str_to_utf8 (raw);
    g_free (raw);
    return text;
}

static gchar * split_line (gchar * line)
{
    gchar * feed = strchr (line, '\n');
    if (! feed)
        return NULL;

    * feed = 0;
    return feed + 1;
}

static gboolean playlist_load_m3u (const gchar * path, VFSFile * file,
 gchar * * title, struct index * filenames, struct index * tuples)
{
    gchar * text = read_win_text (file);
    if (! text)
        return FALSE;

    * title = NULL;

    gchar * parse = text;

    while (parse)
    {
        gchar * next = split_line (parse);

        while (* parse == ' ' || * parse == '\t')
            parse ++;

        if (! * parse)
            break;

        if (* parse == '#')
            goto NEXT;

        gchar * s = aud_construct_uri (parse, path);
        if (s)
            index_append (filenames, str_get (s));

        g_free (s);

NEXT:
        parse = next;
    }

    g_free (text);
    return TRUE;
}

static gboolean playlist_save_m3u (const gchar * path, VFSFile * file,
 const gchar * title, struct index * filenames, struct index * tuples)
{
    gint count = index_count (filenames);

    for (gint i = 0; i < count; i ++)
        vfs_fprintf (file, "%s\n", (const gchar *) index_get (filenames, i));

    return TRUE;
}

static const gchar * const m3u_exts[] = {"m3u", "m3u8", NULL};

AUD_PLAYLIST_PLUGIN
(
 .name = "M3U Playlist Format",
 .extensions = m3u_exts,
 .load = playlist_load_m3u,
 .save = playlist_save_m3u
)
