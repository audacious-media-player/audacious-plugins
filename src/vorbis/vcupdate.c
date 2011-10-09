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

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <audacious/debug.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudcore/audstrings.h>

#include <mowgli.h>

#include "vorbis.h"
#include "vcedit.h"

static gboolean write_and_pivot_files(vcedit_state * state);

static mowgli_patricia_t *
dictionary_from_vorbis_comment(vorbis_comment * vc)
{
    mowgli_patricia_t *dict;
    gint i;
    gchar *val;

    dict = mowgli_patricia_create (NULL);

    for (i = 0; i < vc->comments; i++) {
        gchar **frags;

        AUDDBG("%s\n", vc->user_comments[i]);
        frags = g_strsplit(vc->user_comments[i], "=", 2);

        /* FIXME: need more rigorous checks to guard against
           borqued comments */

        /* No RHS? */
        val = frags[1] ? frags[1] : "";

        mowgli_patricia_add(dict, frags[0], g_strdup(val));

        g_strfreev(frags); /* Don't use g_free() for string lists! --eugene */
    }

    return dict;
}

static gint add_tag_cb (const gchar * key, void * field, void * vc)
{
    vorbis_comment_add_tag (vc, key, field);
    return 0;
}

static void
dictionary_to_vorbis_comment(vorbis_comment * vc, mowgli_patricia_t * dict)
{
    vorbis_comment_clear(vc);
    mowgli_patricia_foreach (dict, add_tag_cb, vc);
}

static void insert_str_tuple_field_to_dictionary (const Tuple * tuple, int
 fieldn, mowgli_patricia_t * dict, char * key)
{
    g_free (mowgli_patricia_delete (dict, key));

    gchar *tmp = (gchar*)tuple_get_string(tuple, fieldn, NULL);
    if(tmp != NULL && strlen(tmp) != 0) mowgli_patricia_add(dict, key, g_strdup(tmp));
}

static void insert_int_tuple_field_to_dictionary (const Tuple * tuple, int
 fieldn, mowgli_patricia_t * dict, char * key)
{
    int val;

    g_free (mowgli_patricia_delete (dict, key));

    if(tuple_get_value_type(tuple, fieldn, NULL) == TUPLE_INT && (val = tuple_get_int(tuple, fieldn, NULL)) >= 0) {
        gchar *tmp = g_strdup_printf("%d", val);
        mowgli_patricia_add(dict, key, tmp);
    }
}

static void destroy_cb (const gchar * key, void * data, void * unused)
{
    g_free (data);
}

gboolean vorbis_update_song_tuple (const Tuple * tuple, VFSFile * fd)
{

    vcedit_state *state;
    vorbis_comment *comment;
    mowgli_patricia_t *dict;
    gboolean ret;

    if(!tuple || !fd) return FALSE;

    state = vcedit_new_state();

    if(vcedit_open(state, fd) < 0) {
        vcedit_clear(state);
        return FALSE;
    }

    comment = vcedit_comments(state);
    dict = dictionary_from_vorbis_comment(comment);

    insert_str_tuple_field_to_dictionary(tuple, FIELD_TITLE, dict, "title");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_ARTIST, dict, "artist");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_ALBUM, dict, "album");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_COMMENT, dict, "comment");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_GENRE, dict, "genre");

    insert_int_tuple_field_to_dictionary(tuple, FIELD_YEAR, dict, "date");
    insert_int_tuple_field_to_dictionary(tuple, FIELD_TRACK_NUMBER, dict, "tracknumber");

    dictionary_to_vorbis_comment(comment, dict);
    mowgli_patricia_destroy(dict, destroy_cb, NULL);

    ret = write_and_pivot_files(state);

    vcedit_clear(state);

    return ret;
}

gchar * filename_to_uri (const gchar * filename)
{
    gchar * utf8 = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
    if (! utf8)
        utf8 = g_strdup (filename);

    gchar * uri = g_filename_to_uri (utf8, NULL, NULL);

    g_free (utf8);
    return uri;
}

#define COPY_BUF 65536

gboolean copy_vfs (VFSFile * in, VFSFile * out)
{
    if (vfs_fseek (in, 0, SEEK_SET) < 0 || vfs_fseek (out, 0, SEEK_SET) < 0)
        return FALSE;

    gchar * buffer = g_malloc (COPY_BUF);
    gint64 size = 0, readed;

    while ((readed = vfs_fread (buffer, 1, COPY_BUF, in)) > 0)
    {
        if (vfs_fwrite (buffer, 1, readed, out) != readed)
            goto FAILED;

        size += readed;
    }

    if (vfs_ftruncate (out, size) < 0)
        goto FAILED;

    g_free (buffer);
    return TRUE;

FAILED:
    g_free (buffer);
    return FALSE;
}

#undef COPY_BUF

gboolean write_and_pivot_files (vcedit_state * state)
{
    gchar * temp;
    GError * error;
    gint handle = g_file_open_tmp (NULL, & temp, & error);

    if (handle < 0)
    {
        fprintf (stderr, "Failed to create temp file: %s.\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    close (handle);

    gchar * temp_uri = filename_to_uri (temp);
    g_return_val_if_fail (temp_uri, FALSE);
    VFSFile * temp_vfs = vfs_fopen (temp_uri, "r+");
    g_return_val_if_fail (temp_vfs, FALSE);

    g_free (temp_uri);

    if (vcedit_write (state, temp_vfs) < 0)
    {
        fprintf (stderr, "Tag update failed: %s.\n", state->lasterror);
        vfs_fclose (temp_vfs);
        g_free (temp);
        return FALSE;
    }

    if (! copy_vfs (temp_vfs, state->in))
    {
        fprintf (stderr, "Failed to copy temp file.  The temp file has not "
         "been deleted: %s.\n", temp);
        vfs_fclose (temp_vfs);
        g_free (temp);
        return FALSE;
    }

    vfs_fclose (temp_vfs);

    if (unlink (temp) < 0)
        fprintf (stderr, "Failed to delete temp file: %s.\n", temp);

    g_free (temp);
    return TRUE;
}
