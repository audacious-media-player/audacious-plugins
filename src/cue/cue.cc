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

static bool is_year (const char * s)
{
    auto is_digit = [] (char c)
        { return c >= '0' && c <= '9'; };

    return is_digit (s[0]) && is_digit (s[1]) &&
           is_digit (s[2]) && is_digit (s[3]) && ! s[4];
}

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

    bool same_file = false;
    String filename;
    PluginHandle * decoder = nullptr;
    Tuple base_tuple;

    for (int track = 1; track <= tracks; track ++)
    {
        if (! same_file)
        {
            filename = String (uri_construct (cur_name, cue_filename));
            decoder = nullptr;
            base_tuple = Tuple ();

            VFSFile file;
            if ((decoder = aud_file_find_decoder (filename, false, file)) &&
             aud_file_read_tag (filename, decoder, file, base_tuple))
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
                    if ((s = cdtext_get (PTI_COMPOSER, cdtext)))
                        base_tuple.set_str (Tuple::Composer, s);
                }

                Rem * rem = cd_get_rem (cd);

                if (rem)
                {
                    const char * s;

                    if ((s = rem_get (REM_DATE, rem)))
                    {
                        if (is_year (s))
                            base_tuple.set_int (Tuple::Year, str_to_int (s));
                        else
                            base_tuple.set_str (Tuple::Date, s);
                    }

                    if ((s = rem_get (REM_REPLAYGAIN_ALBUM_GAIN, rem)))
                        base_tuple.set_gain (Tuple::AlbumGain, Tuple::GainDivisor, s);
                    if ((s = rem_get (REM_REPLAYGAIN_ALBUM_PEAK, rem)))
                        base_tuple.set_gain (Tuple::AlbumPeak, Tuple::PeakDivisor, s);
                }
            }
        }

        Track * next = (track + 1 <= tracks) ? cd_get_track (cd, track + 1) : nullptr;
        const char * next_name = next ? track_get_filename (next) : nullptr;

        same_file = (next_name && ! strcmp (next_name, cur_name));

        if (base_tuple.valid ())
        {
            StringBuf tfilename = str_printf ("%s?%d", (const char *) cue_filename, track);
            Tuple tuple = base_tuple.ref ();
            tuple.set_filename (tfilename);
            tuple.set_int (Tuple::Track, track);
            tuple.set_str (Tuple::AudioFile, filename);

            int begin = (int64_t) track_get_start (cur) * 1000 / 75;
            tuple.set_int (Tuple::StartTime, begin);

            if (same_file)
            {
                int end = (int64_t) track_get_start (next) * 1000 / 75;
                tuple.set_int (Tuple::EndTime, end);
                tuple.set_int (Tuple::Length, end - begin);
            }
            else
            {
                int length = base_tuple.get_int (Tuple::Length);
                if (length > 0)
                    tuple.set_int (Tuple::Length, length - begin);
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

            Rem * rem = track_get_rem (cur);

            if (rem)
            {
                const char * s;
                if ((s = rem_get (REM_REPLAYGAIN_TRACK_GAIN, rem)))
                    tuple.set_gain (Tuple::TrackGain, Tuple::GainDivisor, s);
                if ((s = rem_get (REM_REPLAYGAIN_TRACK_PEAK, rem)))
                    tuple.set_gain (Tuple::TrackPeak, Tuple::PeakDivisor, s);
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
