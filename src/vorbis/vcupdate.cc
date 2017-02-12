/* Audacious - Cross-platform multimedia player
 * Copyright (C) 2005-2007  Audacious development team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>

#include "vorbis.h"
#include "vcedit.h"

typedef SimpleHash<String, String> Dictionary;

static Dictionary dictionary_from_vorbis_comment (vorbis_comment * vc)
{
    Dictionary dict;

    for (int i = 0; i < vc->comments; i ++)
    {
        const char * s = vc->user_comments[i];
        AUDDBG("%s\n", s);

        const char * eq = strchr (s, '=');
        if (eq && eq > s && eq[1])
            dict.add (String (str_toupper (str_copy (s, eq - s))), String (eq + 1));
    }

    return dict;
}

static void dictionary_to_vorbis_comment (vorbis_comment * vc, Dictionary & dict)
{
    vorbis_comment_clear (vc);

    dict.iterate ([vc] (const String & key, String & field) {
        vorbis_comment_add_tag (vc, key, field);
    });
}

static void insert_str_tuple_field_to_dictionary (const Tuple & tuple,
 Tuple::Field field, Dictionary & dict, const char * key)
{
    String val = tuple.get_str (field);

    if (val && val[0])
        dict.add (String (key), std::move (val));
    else
        dict.remove (String (key));
}

static void insert_int_tuple_field_to_dictionary (const Tuple & tuple,
 Tuple::Field field, Dictionary & dict, const char * key)
{
    int val = tuple.get_int (field);

    if (val > 0)
        dict.add (String (key), String (int_to_str (val)));
    else
        dict.remove (String (key));
}

bool VorbisPlugin::write_tuple (const char * filename, VFSFile & file, const Tuple & tuple)
{
    VCEdit edit;
    if (! edit.open (file))
        return false;

    Dictionary dict = dictionary_from_vorbis_comment (& edit.vc);

    insert_str_tuple_field_to_dictionary (tuple, Tuple::Title, dict, "TITLE");
    insert_str_tuple_field_to_dictionary (tuple, Tuple::Artist, dict, "ARTIST");
    insert_str_tuple_field_to_dictionary (tuple, Tuple::Album, dict, "ALBUM");
    insert_str_tuple_field_to_dictionary (tuple, Tuple::AlbumArtist, dict, "ALBUMARTIST");
    insert_str_tuple_field_to_dictionary (tuple, Tuple::Comment, dict, "COMMENT");
    insert_str_tuple_field_to_dictionary (tuple, Tuple::Genre, dict, "GENRE");

    insert_int_tuple_field_to_dictionary (tuple, Tuple::Year, dict, "DATE");
    insert_int_tuple_field_to_dictionary (tuple, Tuple::Track, dict, "TRACKNUMBER");

    dictionary_to_vorbis_comment (& edit.vc, dict);

    auto temp_vfs = VFSFile::tmpfile ();
    if (! temp_vfs)
        return false;

    if (! edit.write (file, temp_vfs))
    {
        AUDERR ("Tag update failed: %s.\n", edit.lasterror);
        return false;
    }

    return file.replace_with (temp_vfs);
}
