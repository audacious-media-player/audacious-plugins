/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
 * Copyright (c) 2010-2011 John Lindgren <john.lindgren@tds.net>
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

#include <libcue/libcue.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "config.h"

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

    tuple_set_str(tuple, tuple_type, NULL, text);
}

static gboolean playlist_load_cue (const gchar * cue_filename, VFSFile * file,
 gchar * * title, Index * filenames, Index * tuples)
{
    gint64 size = vfs_fsize (file);
    gchar * buffer = g_malloc (size + 1);
    size = vfs_fread (buffer, 1, size, file);
    buffer[size] = 0;

    gchar * text = str_to_utf8 (buffer);
    g_free (buffer);
    g_return_val_if_fail (text, FALSE);

    * title = NULL;

    Cd * cd = cue_parse_string (text);
    g_free (text);
    if (cd == NULL)
        return FALSE;

    gint tracks = cd_get_ntrack (cd);
    if (tracks == 0)
        return FALSE;

    Track * current = cd_get_track (cd, 1);
    g_return_val_if_fail (current != NULL, FALSE);
    gchar * track_filename = track_get_filename (current);
    g_return_val_if_fail (track_filename != NULL, FALSE);
    gchar * filename = aud_construct_uri (track_filename,
     cue_filename);

    Tuple * base_tuple = NULL;
    gboolean base_tuple_scanned = FALSE;

    for (gint track = 1; track <= tracks; track ++)
    {
        g_return_val_if_fail (current != NULL, FALSE);
        g_return_val_if_fail (filename != NULL, FALSE);

        if (base_tuple == NULL && ! base_tuple_scanned)
        {
            base_tuple_scanned = TRUE;
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
        tuple_set_int (tuple, FIELD_TRACK_NUMBER, NULL, track);

        gint begin = (gint64) track_get_start (current) * 1000 / 75;
        tuple_set_int (tuple, FIELD_SEGMENT_START, NULL, begin);

        if (last_track)
        {
            if (base_tuple != NULL && tuple_get_value_type (base_tuple,
             FIELD_LENGTH, NULL) == TUPLE_INT)
                tuple_set_int (tuple, FIELD_LENGTH, NULL, tuple_get_int
                 (base_tuple, FIELD_LENGTH, NULL) - begin);
        }
        else
        {
            gint length = (gint64) track_get_length (current) * 1000 / 75;
            tuple_set_int (tuple, FIELD_LENGTH, NULL, length);
            tuple_set_int (tuple, FIELD_SEGMENT_END, NULL, begin + length);
        }

        for (gint i = 0; i < G_N_ELEMENTS (pti_map); i ++)
            tuple_attach_cdtext (tuple, current, pti_map[i].tuple_type,
             pti_map[i].pti);

        index_append (filenames, str_get (filename));
        index_append (tuples, tuple);

        current = next;
        g_free (filename);
        filename = next_filename;

        if (last_track && base_tuple != NULL)
        {
            tuple_unref (base_tuple);
            base_tuple = NULL;
            base_tuple_scanned = FALSE;
        }
    }

    return TRUE;
}

static const gchar * const cue_exts[] = {"cue", NULL};

AUD_PLAYLIST_PLUGIN
(
 .name = N_("Cue Sheet Plugin"),
 .domain = PACKAGE,
 .extensions = cue_exts,
 .load = playlist_load_cue
)
