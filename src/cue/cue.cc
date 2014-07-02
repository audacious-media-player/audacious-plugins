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

static const struct {
    int tuple_type;
    int pti;
} pti_map[] = {
    { FIELD_ARTIST, PTI_PERFORMER },
    { FIELD_TITLE, PTI_TITLE },
};

static void
tuple_attach_cdtext(Tuple &tuple, Track *track, int tuple_type, int pti)
{
    Cdtext *cdtext;
    const char *text;

    cdtext = track_get_cdtext(track);
    if (cdtext == nullptr)
        return;

    text = cdtext_get(pti, cdtext);
    if (text == nullptr)
        return;

    tuple.set_str (tuple_type, text);
}

static bool playlist_load_cue (const char * cue_filename, VFSFile * file,
 String & title, Index<PlaylistAddItem> & items)
{
    void * buffer = nullptr;
    vfs_file_read_all (file, & buffer, nullptr);
    if (! buffer)
        return false;

    Cd * cd = cue_parse_string ((char *) buffer);
    g_free (buffer);
    if (cd == nullptr)
        return false;

    int tracks = cd_get_ntrack (cd);
    if (tracks == 0)
        return false;

    Track * current = cd_get_track (cd, 1);
    if (current == nullptr)
        return false;

    char * track_filename = track_get_filename (current);
    if (track_filename == nullptr)
        return false;

    String filename = String (uri_construct (track_filename, cue_filename));

    Tuple base_tuple;
    bool base_tuple_scanned = false;

    for (int track = 1; track <= tracks; track ++)
    {
        if (current == nullptr || ! filename)
            return false;

        if (! base_tuple_scanned)
        {
            base_tuple_scanned = true;
            PluginHandle * decoder = aud_file_find_decoder (filename, false);
            if (decoder != nullptr)
                base_tuple = aud_file_read_tuple (filename, decoder);
        }

        Track * next = (track + 1 <= tracks) ? cd_get_track (cd, track + 1) : nullptr;
        String next_filename = (next != nullptr) ? String (uri_construct
         (track_get_filename (next), cue_filename)) : String ();
        bool last_track = (! next_filename || strcmp (next_filename, filename));

        Tuple tuple = base_tuple.ref ();
        tuple.set_filename (filename);
        tuple.set_int (FIELD_TRACK_NUMBER, track);

        int begin = (int64_t) track_get_start (current) * 1000 / 75;
        tuple.set_int (FIELD_SEGMENT_START, begin);

        if (last_track)
        {
            if (base_tuple.get_value_type (FIELD_LENGTH) == TUPLE_INT)
                tuple.set_int (FIELD_LENGTH, base_tuple.get_int (FIELD_LENGTH) - begin);
        }
        else
        {
            int length = (int64_t) track_get_length (current) * 1000 / 75;
            tuple.set_int (FIELD_LENGTH, length);
            tuple.set_int (FIELD_SEGMENT_END, begin + length);
        }

        for (auto & pti : pti_map)
            tuple_attach_cdtext (tuple, current, pti.tuple_type, pti.pti);

        items.append ({filename, std::move (tuple)});

        current = next;
        filename = next_filename;

        if (last_track)
        {
            base_tuple = Tuple ();
            base_tuple_scanned = false;
        }
    }

    return true;
}

static const char * const cue_exts[] = {"cue", nullptr};

#define AUD_PLUGIN_NAME        N_("Cue Sheet Plugin")
#define AUD_PLAYLIST_EXTS      cue_exts
#define AUD_PLAYLIST_LOAD      playlist_load_cue

#define AUD_DECLARE_PLAYLIST
#include <libaudcore/plugin-declare.h>
