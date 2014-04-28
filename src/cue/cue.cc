/*
 * Cue Sheet Plugin for Audacious
 * Copyright (c) 2009-2011 William Pitcock and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <stdlib.h>
#include <string.h>

#include <glib.h>

extern "C" {
#include <libcue/libcue.h>
}

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/probe.h>

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

    tuple_set_str (tuple, tuple_type, text);
}

static bool_t playlist_load_cue (const char * cue_filename, VFSFile * file,
 char * * title, Index<PlaylistAddItem> & items)
{
    void * buffer = NULL;
    vfs_file_read_all (file, & buffer, NULL);
    if (! buffer)
        return FALSE;

    * title = NULL;

    Cd * cd = cue_parse_string ((char *) buffer);
    g_free (buffer);
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

    char * filename = uri_construct (track_filename, cue_filename);

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
        char * next_filename = (next != NULL) ? uri_construct
         (track_get_filename (next), cue_filename) : NULL;
        bool_t last_track = (next_filename == NULL || strcmp (next_filename, filename));

        Tuple * tuple = (base_tuple != NULL) ? tuple_copy (base_tuple) :
         tuple_new_from_filename (filename);
        tuple_set_int (tuple, FIELD_TRACK_NUMBER, track);

        int begin = (int64_t) track_get_start (current) * 1000 / 75;
        tuple_set_int (tuple, FIELD_SEGMENT_START, begin);

        if (last_track)
        {
            if (base_tuple != NULL && tuple_get_value_type (base_tuple,
             FIELD_LENGTH) == TUPLE_INT)
                tuple_set_int (tuple, FIELD_LENGTH, tuple_get_int
                 (base_tuple, FIELD_LENGTH) - begin);
        }
        else
        {
            int length = (int64_t) track_get_length (current) * 1000 / 75;
            tuple_set_int (tuple, FIELD_LENGTH, length);
            tuple_set_int (tuple, FIELD_SEGMENT_END, begin + length);
        }

        for (int i = 0; i < ARRAY_LEN (pti_map); i ++)
            tuple_attach_cdtext (tuple, current, pti_map[i].tuple_type, pti_map[i].pti);

        items.append ({str_get (filename), tuple});

        current = next;
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

#define AUD_PLUGIN_NAME        N_("Cue Sheet Plugin")
#define AUD_PLAYLIST_EXTS      cue_exts
#define AUD_PLAYLIST_LOAD      playlist_load_cue

#define AUD_DECLARE_PLAYLIST
#include <libaudcore/plugin-declare.h>
