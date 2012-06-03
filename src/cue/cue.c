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

#include <stdlib.h>
#include <string.h>

#include <libcue/libcue.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "config.h"

typedef struct {
    int tuple_type;
    int pti;
} TuplePTIMap;

TuplePTIMap pti_map[] = {
    { FIELD_ARTIST, PTI_PERFORMER },
    { FIELD_TITLE, PTI_TITLE },
};

static void
tuple_attach_cdtext(Tuple *tuple, Track *track, int tuple_type, int pti)
{
    Cdtext *cdtext;
    const char *text;

    cdtext = track_get_cdtext(track);
    if (cdtext == NULL)
        return;

    text = cdtext_get(pti, cdtext);
    if (text == NULL)
        return;

    tuple_set_str(tuple, tuple_type, NULL, text);
}

static bool_t playlist_load_cue (const char * cue_filename, VFSFile * file,
 char * * title, Index * filenames, Index * tuples)
{
    int64_t size = vfs_fsize (file);
    char * buffer = malloc (size + 1);
    size = vfs_fread (buffer, 1, size, file);
    buffer[size] = 0;

    char * text = str_to_utf8 (buffer);
    free (buffer);
    if (text == NULL)
        return FALSE;

    * title = NULL;

    Cd * cd = cue_parse_string (text);
    free (text);
    if (cd == NULL)
        return FALSE;

    int tracks = cd_get_ntrack (cd);
    if (tracks == 0)
        return FALSE;

    Track * current = cd_get_track (cd, 1);
    if (current == NULL)
        return FALSE;

    char * track_filename = track_get_filename (current);
    if (track_filename == NULL)
        return FALSE;

    char * filename = aud_construct_uri (track_filename, cue_filename);

    Tuple * base_tuple = NULL;
    bool_t base_tuple_scanned = FALSE;

    for (int track = 1; track <= tracks; track ++)
    {
        if (current == NULL || filename == NULL)
            return FALSE;

        if (base_tuple == NULL && ! base_tuple_scanned)
        {
            base_tuple_scanned = TRUE;
            PluginHandle * decoder = aud_file_find_decoder (filename, FALSE);
            if (decoder != NULL)
                base_tuple = aud_file_read_tuple (filename, decoder);
        }

        Track * next = (track + 1 <= tracks) ? cd_get_track (cd, track + 1) : NULL;
        char * next_filename = (next != NULL) ? aud_construct_uri
         (track_get_filename (next), cue_filename) : NULL;
        bool_t last_track = (next_filename == NULL || strcmp (next_filename, filename));

        Tuple * tuple = (base_tuple != NULL) ? tuple_copy (base_tuple) :
         tuple_new_from_filename (filename);
        tuple_set_int (tuple, FIELD_TRACK_NUMBER, NULL, track);

        int begin = (int64_t) track_get_start (current) * 1000 / 75;
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
            int length = (int64_t) track_get_length (current) * 1000 / 75;
            tuple_set_int (tuple, FIELD_LENGTH, NULL, length);
            tuple_set_int (tuple, FIELD_SEGMENT_END, NULL, begin + length);
        }

        for (int i = 0; i < sizeof pti_map / sizeof pti_map[0]; i ++)
            tuple_attach_cdtext (tuple, current, pti_map[i].tuple_type, pti_map[i].pti);

        index_append (filenames, str_get (filename));
        index_append (tuples, tuple);

        current = next;
        free (filename);
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

static const char * const cue_exts[] = {"cue", NULL};

AUD_PLAYLIST_PLUGIN
(
 .name = N_("Cue Sheet Plugin"),
 .domain = PACKAGE,
 .extensions = cue_exts,
 .load = playlist_load_cue
)
