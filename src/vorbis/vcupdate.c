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

static mowgli_dictionary_t *
dictionary_from_vorbis_comment(vorbis_comment * vc)
{
    mowgli_dictionary_t *dict;
    gint i;
    gchar *val;

    dict = mowgli_dictionary_create(g_ascii_strcasecmp);

    for (i = 0; i < vc->comments; i++) {
        gchar **frags;

        AUDDBG("%s\n", vc->user_comments[i]);
        frags = g_strsplit(vc->user_comments[i], "=", 2);

        /* FIXME: need more rigorous checks to guard against
           borqued comments */

        /* No RHS? */
        val = frags[1] ? frags[1] : "";

        mowgli_dictionary_add(dict, frags[0], g_strdup(val));

        g_strfreev(frags); /* Don't use g_free() for string lists! --eugene */
    }

    return dict;
}

static void
dictionary_to_vorbis_comment(vorbis_comment * vc, mowgli_dictionary_t * dict)
{
    mowgli_dictionary_iteration_state_t state;
    gchar *field;

    vorbis_comment_clear(vc);

    MOWGLI_DICTIONARY_FOREACH(field, &state, dict) {
        vorbis_comment_add_tag(vc, state.cur->key, field);
    }
}

static void insert_str_tuple_field_to_dictionary (const Tuple * tuple, int
 fieldn, mowgli_dictionary_t * dict, char * key)
{

    if(mowgli_dictionary_find(dict, key) != NULL) g_free(mowgli_dictionary_delete(dict, key));

    gchar *tmp = (gchar*)tuple_get_string(tuple, fieldn, NULL);
    if(tmp != NULL && strlen(tmp) != 0) mowgli_dictionary_add(dict, key, g_strdup(tmp));
}

static void insert_int_tuple_field_to_dictionary (const Tuple * tuple, int
 fieldn, mowgli_dictionary_t * dict, char * key)
{
    int val;

    if(mowgli_dictionary_find(dict, key) != NULL) g_free(mowgli_dictionary_delete(dict, key));

    if(tuple_get_value_type(tuple, fieldn, NULL) == TUPLE_INT && (val = tuple_get_int(tuple, fieldn, NULL)) >= 0) {
        gchar *tmp = g_strdup_printf("%d", val);
        mowgli_dictionary_add(dict, key, tmp);
    }
}

static void
destroy_cb(mowgli_dictionary_elem_t *delem, void *privdata)
{
    g_free(delem->data);
}

gboolean vorbis_update_song_tuple (const Tuple * tuple, VFSFile * fd)
{

    vcedit_state *state;
    vorbis_comment *comment;
    mowgli_dictionary_t *dict;
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
    mowgli_dictionary_destroy(dict, destroy_cb, NULL);

    ret = write_and_pivot_files(state);

    vcedit_clear(state);

    return ret;
}

/* from stdio VFS plugin */
static gchar *
vfs_stdio_urldecode_path(const gchar * encoded_path)
{
    const gchar *cur, *ext;
    gchar *path, *tmp;
    guint realchar;

    if (!encoded_path)
        return NULL;

    if (!str_has_prefix_nocase(encoded_path, "file:"))
        return NULL;

    cur = encoded_path + 5;

    if (str_has_prefix_nocase(cur, "//localhost"))
        cur += 11;

    if (*cur == '/')
        while (cur[1] == '/')
            cur++;

    tmp = g_malloc0(strlen(cur) + 1);

    while ((ext = strchr(cur, '%')) != NULL) {
        strncat(tmp, cur, ext - cur);
        ext++;
        cur = ext + 2;
        if (!sscanf(ext, "%2x", &realchar)) {
            /* Assume it is a literal '%'.  Several file
             * managers send unencoded file: urls on drag
             * and drop. */
            realchar = '%';
            cur -= 2;
        }
        tmp[strlen(tmp)] = realchar;
    }

    path = g_strconcat(tmp, cur, NULL);
    g_free(tmp);
    return path;
}

gboolean
write_and_pivot_files(vcedit_state * state)
{
    gint retval;
    gchar *tmpfn, *unq_tmpfn, *unq_in;
    VFSFile *out;

    tmpfn = g_strdup_printf("%s.XXXXXX", ((VFSFile*)state->in)->uri);
    mktemp(tmpfn);

    AUDDBG("creating temp file: %s\n", tmpfn);

    if ((out = vfs_fopen(tmpfn, "wb")) == NULL) {
        g_free(tmpfn);
        AUDDBG("fileinfo.c: couldn't create temp file, %s\n", tmpfn);
        return FALSE;
    }

    if (vcedit_write(state, out) < 0) {
        g_free(tmpfn);
        vfs_fclose(out);
        AUDDBG("vcedit_write: %s\n", state->lasterror);
        return FALSE;
    }

    vfs_fclose(out);

    unq_tmpfn = vfs_stdio_urldecode_path(tmpfn);
    unq_in = vfs_stdio_urldecode_path(((VFSFile*)state->in)->uri);

    if((retval = rename(unq_tmpfn, unq_in)) == 0) {
        AUDDBG("fileinfo.c: file %s renamed successfully to %s\n", unq_tmpfn, unq_in);
    } else {
        remove(unq_tmpfn);
        AUDDBG("fileinfo.c: couldn't rename file\n");
    }

    g_free(unq_in);
    g_free(unq_tmpfn);
    g_free(tmpfn);

    return retval == 0;
}

