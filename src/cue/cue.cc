/*
 * Cue Sheet Plugin for Audacious
 * Copyright (c) 2009-2015 William Pitcock and John Lindgren
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

#include <string.h>
#include <pthread.h>

#ifdef HAVE_LIBCUE2
#include <libcue.h>
#else
extern "C" {
#include <libcue/libcue.h>
}
#endif

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/probe.h>

static const char * const cue_exts[] = {"cue"};

class CueLoader : public PlaylistPlugin
{
public:
    static constexpr PluginInfo info = {N_("Cue Sheet Plugin"), PACKAGE};
    constexpr CueLoader () : PlaylistPlugin (info, cue_exts, false) {}

    bool load (const char * filename, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items);
};

EXPORT CueLoader aud_plugin_instance;

bool CueLoader::load (const char * cue_filename, VFSFile & file, String & title,
 Index<PlaylistAddItem> & items)
{
    // XXX: cue_parse_string crashes if called concurrently
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    Index<char> buffer = file.read_all ();
    if (! buffer.len ())
        return false;

    buffer.append (0);  /* null-terminate */

    pthread_mutex_lock (& mutex);
    Cd * cd = cue_parse_string (buffer.begin ());
    pthread_mutex_unlock (& mutex);

    int tracks = cd ? cd_get_ntrack (cd) : 0;
    if (tracks < 1)
        return false;

    Track * cur = cd_get_track (cd, 1);
    const char * cur_name = cur ? track_get_filename (cur) : nullptr;

    if (! cur_name)
        return false;

    bool new_file = true;
    String filename;
    PluginHandle * decoder = nullptr;
    Tuple base_tuple;

    for (int track = 1; track <= tracks; track ++)
    {
        if (new_file)
        {
            filename = String (uri_construct (cur_name, cue_filename));
            decoder = filename ? aud_file_find_decoder (filename, false) : nullptr;
            base_tuple = decoder ? aud_file_read_tuple (filename, decoder) : Tuple ();

            if (base_tuple.valid ())
            {
                Cdtext * cdtext = cd_get_cdtext (cd);

                if (cdtext)
                {
                    const char * s;
                    if ((s = cdtext_get (PTI_PERFORMER, cdtext)))
                        base_tuple.set_str (Tuple::AlbumArtist, s);
                    if ((s = cdtext_get (PTI_TITLE, cdtext)))
                        base_tuple.set_str (Tuple::Album, s);
                    if ((s = cdtext_get (PTI_GENRE, cdtext)))
                        base_tuple.set_str (Tuple::Genre, s);
                }

                Rem * rem = cd_get_rem (cd);

                if (rem)
                {
                    const char * s;
                    if ((s = rem_get (REM_DATE, rem)))
                        base_tuple.set_int (Tuple::Year, str_to_int (s));
                }
            }
        }

        Track * next = (track + 1 <= tracks) ? cd_get_track (cd, track + 1) : nullptr;
        const char * next_name = next ? track_get_filename (next) : nullptr;

        new_file = (! next_name || strcmp (next_name, cur_name));

        if (base_tuple.valid ())
        {
            StringBuf tfilename = str_printf ("%s?%d", (const char *) cue_filename, track);
            Tuple tuple = base_tuple.ref ();
            tuple.set_filename (tfilename);
            tuple.set_int (Tuple::Track, track);
            tuple.set_str (Tuple::AudioFile, filename);

            int begin = (int64_t) track_get_start (cur) * 1000 / 75;
            tuple.set_int (Tuple::StartTime, begin);

            if (new_file)
            {
                int length = base_tuple.get_int (Tuple::Length);
                if (length > 0)
                    tuple.set_int (Tuple::Length, length - begin);
            }
            else
            {
                int length = (int64_t) track_get_length (cur) * 1000 / 75;
                tuple.set_int (Tuple::Length, length);
                tuple.set_int (Tuple::EndTime, begin + length);
            }

            Cdtext * cdtext = track_get_cdtext (cur);

            if (cdtext)
            {
                const char * s;
                if ((s = cdtext_get (PTI_PERFORMER, cdtext)))
                    tuple.set_str (Tuple::Artist, s);
                if ((s = cdtext_get (PTI_TITLE, cdtext)))
                    tuple.set_str (Tuple::Title, s);
                if ((s = cdtext_get (PTI_GENRE, cdtext)))
                    tuple.set_str (Tuple::Genre, s);
            }

            Rem * rem = cd_get_rem (cd);

            if (rem)
            {
                const char * s;
                if ((s = rem_get (REM_DATE, rem)))
                    tuple.set_int (Tuple::Year, str_to_int (s));
            }

            items.append (String (tfilename), std::move (tuple), decoder);
        }

        if (! next_name)
            break;

        cur = next;
        cur_name = next_name;
    }

    return true;
}
