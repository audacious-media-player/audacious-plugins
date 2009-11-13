/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa, Eugene Zagidullin
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
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

#include "config.h"

#include "plugin.h"
#include "tuple.h"
#include "input.h"

#include <math.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <audacious/plugin.h>
#include <audacious/id3tag.h>

#include <langinfo.h>

static void
update_id3_frame(struct id3_tag *tag, const char *frame_name, const char *data, int sjis)
{
    int res;
    struct id3_frame *frame;
    union id3_field *field;
    id3_ucs4_t *ucs4;

    if (data == NULL)
        return;

    /* printf ("updating id3: %s: %s\n", frame_name, data);


     An empty string removes the frame altogether.
    */
    if (strlen(data) == 0) {
        while ((frame = id3_tag_findframe(tag, frame_name, 0))) {
            AUDDBG("madplug: detachframe\n");
            id3_tag_detachframe(tag, frame);
        }
        return;
    }

    frame = id3_tag_findframe(tag, frame_name, 0);
    if (!frame) {
        AUDDBG("frame_new\n");
        frame = id3_frame_new(frame_name);
        id3_tag_attachframe(tag, frame);
    }

    /* setup ucs4 string */
    if(sjis) {
        ucs4 = id3_latin1_ucs4duplicate((id3_latin1_t *) data);
    }
    else {
        ucs4 = id3_utf8_ucs4duplicate((id3_utf8_t *) data);
    }

    /* set encoding */
    field = id3_frame_field(frame, 0);
    id3_field_settextencoding(field, sjis ? ID3_FIELD_TEXTENCODING_ISO_8859_1 :
			      ID3_FIELD_TEXTENCODING_UTF_8);

    /* setup genre code */
    if (!strcmp(frame_name, ID3_FRAME_GENRE)) {
        char *tmp;
        int index = id3_genre_number(ucs4);
        g_free(ucs4);

        if(index == -1) { /* unknown genre. remove TCON frame. */
            AUDDBG("madplug: remove genre frame\n");
            id3_tag_detachframe(tag, frame);
        }
        else { /* meaningful genre */
            tmp = g_strdup_printf("%d", index);
            ucs4 = id3_latin1_ucs4duplicate((unsigned char *) tmp);
        }

    }

    /* write string */
    if (!strcmp(frame_name, ID3_FRAME_COMMENT)) {
        field = id3_frame_field(frame, 3);
        field->type = ID3_FIELD_TYPE_STRINGFULL;
        res = id3_field_setfullstring(field, ucs4);
    }
    else {
        field = id3_frame_field(frame, 1);
        field->type = ID3_FIELD_TYPE_STRINGLIST;
        res = id3_field_setstrings(field, 1, &ucs4);
    }

    if (res != 0)
        g_print("error setting id3 field: %s\n", frame_name);
}

static void
update_id3_frame_from_tuple(struct id3_tag *id3tag, const gchar *field, Tuple *tuple, gint fieldn, gint sjis)
{
    gint val;
    const char *encoding = sjis ? "SJIS" : "UTF-8";

    if(aud_tuple_get_value_type(tuple, fieldn, NULL) == TUPLE_INT) {
        val = aud_tuple_get_int(tuple, fieldn, NULL);
        if(val > 0) {
            char *text = g_strdup_printf("%d", val);
            AUDDBG("madplug: updating field:\"%s\"=\"%s\", enc %s\n", field, text, encoding);
            update_id3_frame(id3tag, field, text, 0);
            g_free(text);
        } else {
            update_id3_frame(id3tag, field, "", 0); /* will be detached */
        }

    } else if(aud_tuple_get_value_type(tuple, fieldn, NULL) == TUPLE_STRING) {
        const gchar *text = aud_tuple_get_string(tuple, fieldn, NULL);
        if (text != NULL) {
            gchar *text2 = g_convert(text, -1, encoding, "UTF-8", NULL, NULL, NULL);
            if (text2 != NULL) {
                AUDDBG("madplug: updating field:\"%s\"=\"%s\", enc %s\n", field, text2, encoding);
                update_id3_frame(id3tag, field, text2, sjis);
                g_free(text2);
            }
        }
    }
}

gboolean
audmad_update_song_tuple(Tuple *tuple, VFSFile *fd)
{
    struct id3_file *id3file;
    struct id3_tag *id3tag;
    gchar *text;
    struct mad_info_t songinfo;

    if ((id3file = id3_file_vfsopen(fd, ID3_FILE_MODE_READWRITE)) == NULL) return FALSE;

    id3tag = id3_file_tag(id3file);
    if (!id3tag) {
        AUDDBG("no id3tag\n. append new tag.\n");
        id3tag = id3_tag_new();
        id3_tag_clearframes(id3tag);
        id3tag->options |= ID3_TAG_OPTION_APPENDEDTAG | ID3_TAG_OPTION_ID3V1;
    }

    id3_tag_options(id3tag, ID3_TAG_OPTION_ID3V1, ~0);    /* enables id3v1. TODO: make id3v1 optional */

    update_id3_frame_from_tuple(id3tag, ID3_FRAME_TITLE, tuple, FIELD_TITLE, audmad_config->sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_ARTIST, tuple, FIELD_ARTIST, audmad_config->sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_ALBUM, tuple, FIELD_ALBUM, audmad_config->sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_YEAR, tuple, FIELD_YEAR, audmad_config->sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_COMMENT, tuple, FIELD_COMMENT, audmad_config->sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_TRACK, tuple, FIELD_TRACK_NUMBER, audmad_config->sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_GENRE, tuple, FIELD_GENRE, audmad_config->sjis);

    if(!id3_tag_findframe(id3tag, "TLEN", 0) && input_init(&songinfo, fd->uri, fd)) {
        AUDDBG("update TLEN frame\n");
        input_get_info (& songinfo);
        text = g_strdup_printf ("%d", songinfo.length);
        AUDDBG("TLEN: \"%s\"\n", text);
        update_id3_frame(id3tag, "TLEN", text, 0);
        g_free(text);
        input_term(&songinfo);
    }

    if (id3_file_update(id3file) != 0) return FALSE;

    id3_file_close(id3file);
    return TRUE;
}

