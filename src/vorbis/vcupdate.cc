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

#include <glib.h>
#include <glib/gstdio.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/multihash.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include "vorbis.h"
#include "vcedit.h"

static bool write_and_pivot_files(vcedit_state * state);

typedef SimpleHash<String, String> Dictionary;

static Dictionary dictionary_from_vorbis_comment (vorbis_comment * vc)
{
    Dictionary dict;

    for (int i = 0; i < vc->comments; i ++)
    {
        char **frags;

        AUDDBG("%s\n", vc->user_comments[i]);
        frags = g_strsplit(vc->user_comments[i], "=", 2);

        if (frags[0] && frags[1])
        {
            char * key = g_ascii_strdown (frags[0], -1);
            dict.add (String (key), String (frags[1]));
            g_free (key);
        }

        g_strfreev(frags); /* Don't use g_free() for string lists! --eugene */
    }

    return dict;
}

static void add_tag_cb (const String & key, String & field, void * vc)
{
    vorbis_comment_add_tag ((vorbis_comment *) vc, key, field);
}

static void dictionary_to_vorbis_comment (vorbis_comment * vc, Dictionary & dict)
{
    vorbis_comment_clear(vc);
    dict.iterate (add_tag_cb, vc);
}

static void insert_str_tuple_field_to_dictionary (const Tuple & tuple,
 int fieldn, Dictionary & dict, const char * key)
{
    String val = tuple.get_str (fieldn);

    if (val && val[0])
        dict.add (String (key), std::move (val));
    else
        dict.remove (String (key));
}

static void insert_int_tuple_field_to_dictionary (const Tuple & tuple,
 int fieldn, Dictionary & dict, const char * key)
{
    int val = tuple.get_int (fieldn);

    if (val > 0)
        dict.add (String (key), String (int_to_str (val)));
    else
        dict.remove (String (key));
}

bool vorbis_update_song_tuple (const char * filename, VFSFile * fd, const Tuple & tuple)
{

    vcedit_state *state;
    vorbis_comment *comment;
    bool ret;

    if(!tuple || !fd) return false;

    state = vcedit_new_state();

    if(vcedit_open(state, fd) < 0) {
        vcedit_clear(state);
        return false;
    }

    comment = vcedit_comments(state);
    Dictionary dict = dictionary_from_vorbis_comment (comment);

    insert_str_tuple_field_to_dictionary(tuple, FIELD_TITLE, dict, "title");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_ARTIST, dict, "artist");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_ALBUM, dict, "album");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_COMMENT, dict, "comment");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_GENRE, dict, "genre");

    insert_int_tuple_field_to_dictionary(tuple, FIELD_YEAR, dict, "date");
    insert_int_tuple_field_to_dictionary(tuple, FIELD_TRACK_NUMBER, dict, "tracknumber");

    dictionary_to_vorbis_comment(comment, dict);

    ret = write_and_pivot_files(state);

    vcedit_clear(state);

    return ret;
}

#define COPY_BUF 65536

bool copy_vfs (VFSFile * in, VFSFile * out)
{
    if (vfs_fseek (in, 0, VFS_SEEK_SET) < 0 || vfs_fseek (out, 0, VFS_SEEK_SET) < 0)
        return false;

    char * buffer = g_new (char, COPY_BUF);
    int64_t size = 0, readed;

    while ((readed = vfs_fread (buffer, 1, COPY_BUF, in)) > 0)
    {
        if (vfs_fwrite (buffer, 1, readed, out) != readed)
            goto FAILED;

        size += readed;
    }

    if (vfs_ftruncate (out, size) < 0)
        goto FAILED;

    g_free (buffer);
    return true;

FAILED:
    g_free (buffer);
    return false;
}

#undef COPY_BUF

bool write_and_pivot_files (vcedit_state * state)
{
    char * temp;
    GError * error;
    int handle = g_file_open_tmp (nullptr, & temp, & error);

    if (handle < 0)
    {
        AUDERR ("Failed to create temp file: %s.\n", error->message);
        g_error_free (error);
        return false;
    }

    close (handle);

    StringBuf temp_uri = filename_to_uri (temp);
    g_return_val_if_fail (temp_uri, false);
    VFSFile * temp_vfs = vfs_fopen (temp_uri, "r+");
    g_return_val_if_fail (temp_vfs, false);

    if (vcedit_write (state, temp_vfs) < 0)
    {
        AUDERR ("Tag update failed: %s.\n", state->lasterror);
        vfs_fclose (temp_vfs);
        g_free (temp);
        return false;
    }

    if (! copy_vfs (temp_vfs, (VFSFile *) state->in))
    {
        AUDERR ("Failed to copy temp file.  The temp file has not "
         "been deleted: %s.\n", temp);
        vfs_fclose (temp_vfs);
        g_free (temp);
        return false;
    }

    vfs_fclose (temp_vfs);

    if (g_unlink (temp) < 0)
        AUDERR ("Failed to delete temp file: %s.\n", temp);

    g_free (temp);
    return true;
}
