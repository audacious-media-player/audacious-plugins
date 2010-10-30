/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
 * Copyright (c) 2010 John Lindgren <john.lindgren@tds.net>
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

#include <libcue/libcue.h>

typedef struct {
    gint tuple_type;
    gint pti;
} TuplePTIMap;

TuplePTIMap pti_map[] = {
    { FIELD_ARTIST, PTI_PERFORMER },
    { FIELD_TITLE, PTI_TITLE },
};

static void
tuple_attach_cdtext(Tuple *tuple, Track *track, gint tuple_type, gint pti)
{
    Cdtext *cdtext;
    const gchar *text;

    g_return_if_fail(tuple != NULL);
    g_return_if_fail(track != NULL);

    cdtext = track_get_cdtext(track);
    if (cdtext == NULL)
        return;

    text = cdtext_get(pti, cdtext);
    if (text == NULL)
        return;

    tuple_associate_string(tuple, tuple_type, NULL, text);
}

static void playlist_load_cue (const gchar * cue_filename, gint at)
{
    void * buffer;
    gint64 size;
    vfs_file_get_contents (cue_filename, & buffer, & size);
    if (buffer == NULL)
        return;

    buffer = g_realloc (buffer, size + 1);
    ((gchar *) buffer)[size] = 0;

    Cd * cd = cue_parse_string (buffer);
    g_free (buffer);
    if (cd == NULL)
        return;

    gint tracks = cd_get_ntrack (cd);
    if (tracks == 0)
        return;

    struct index * filenames = index_new ();
    struct index * tuples = index_new ();

    Track * current = cd_get_track (cd, 1);
    g_return_if_fail (current != NULL);
    gchar * filename = aud_construct_uri (track_get_filename (current),
     cue_filename);

    Tuple * base_tuple = NULL;

    for (gint track = 1; track <= tracks; track ++)
    {
        g_return_if_fail (current != NULL);
        g_return_if_fail (filename != NULL);

        if (base_tuple == NULL)
        {
            PluginHandle * decoder = aud_file_find_decoder (filename, FALSE);
            if (decoder != NULL)
                base_tuple = aud_file_read_tuple (filename, decoder);
        }

        Track * next = (track + 1 <= tracks) ? cd_get_track (cd, track + 1) :
         NULL;
        gchar * next_filename = (next != NULL) ? aud_construct_uri
         (track_get_filename (next), cue_filename) : NULL;
        gboolean last_track = (next_filename == NULL || strcmp (next_filename,
         filename));

        Tuple * tuple = (base_tuple != NULL) ? tuple_copy (base_tuple) :
         tuple_new_from_filename (filename);
        tuple_associate_int (tuple, FIELD_TRACK_NUMBER, NULL, track);

        gint begin = (gint64) track_get_start (current) * 1000 / 75;
        tuple_associate_int (tuple, FIELD_SEGMENT_START, NULL, begin);

        if (last_track)
        {
            if (base_tuple != NULL && tuple_get_value_type (base_tuple,
             FIELD_LENGTH, NULL) == TUPLE_INT)
                tuple_associate_int (tuple, FIELD_LENGTH, NULL, tuple_get_int
                 (base_tuple, FIELD_LENGTH, NULL) - begin);
        }
        else
        {
            gint length = (gint64) track_get_length (current) * 1000 / 75;
            tuple_associate_int (tuple, FIELD_LENGTH, NULL, length);
            tuple_associate_int (tuple, FIELD_SEGMENT_END, NULL, begin + length);
        }

        for (gint i = 0; i < G_N_ELEMENTS (pti_map); i ++)
            tuple_attach_cdtext (tuple, current, pti_map[i].tuple_type,
             pti_map[i].pti);

        index_append (filenames, filename);
        index_append (tuples, tuple);

        current = next;
        filename = next_filename;

        if (last_track && base_tuple != NULL)
        {
            tuple_free (base_tuple);
            base_tuple = NULL;
        }
    }

    aud_playlist_entry_insert_batch (aud_playlist_get_active (), at, filenames,
     tuples);
}

static void
playlist_save_cue(const gchar *filename, gint pos)
{
    AUDDBG("todo\n");
}

PlaylistContainer plc_cue = {
    .name = "cue Playlist Format",
    .ext = "cue",
    .plc_read = playlist_load_cue,
    .plc_write = playlist_save_cue,
};

static void init(void)
{
    aud_playlist_container_register(&plc_cue);
}

static void cleanup(void)
{
    aud_playlist_container_unregister(&plc_cue);
}

DECLARE_PLUGIN(cue, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL);
