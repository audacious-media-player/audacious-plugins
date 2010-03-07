/*
 * mad plugin for audacious
 * Copyright (C) 2005-2009 William Pitcock, Yoshiki Yazawa, John Lindgren
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

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif                          /* HAVE_ASSERT_H */

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif                          /* HAVE_SYS_TYPES_H */

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif                          /* HAVE_SYS_SOCKET_H */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif                          /* HAVE_NETINET_IN_H */

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif                          /* HAVE_ARPA_INET_H */

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif                          /* HAVE_NETDB_H */

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif                          /* HAVE_SYS_STAT_H */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif                          /* HAVE_SYS_TIME_H */

#include <fcntl.h>
#include <errno.h>

#include "input.h"
#include "replaygain.h"

#define DIR_SEPARATOR '/'
#define HEADER_SIZE 256
#define LINE_LENGTH 256

extern gboolean scan_file(struct mad_info_t *info, gboolean fast);

/**
 * init the mad_info_t struct.
 */
gboolean
input_init(struct mad_info_t * info, const char *url, VFSFile *fd)
{
    AUDDBG("f: input_init\n");

    memset(info, 0, sizeof(struct mad_info_t)); // all fields are cleared to 0 --yaz

    info->channels = -1;
    info->mpeg_layer = -1;
    info->size = -1;
    info->freq = -1;
    info->seek = -1;
    info->filename = g_strdup(url);
    info->length = 0;
    info->tuple = NULL;
    info->playback = NULL;

    // from input_read_replaygain()
    info->replaygain_track_peak = 0.0;
    info->replaygain_track_scale = 0.0;
    info->replaygain_album_peak = 0.0;
    info->replaygain_album_scale = 0.0;
    info->mp3gain_undo = -77;
    info->mp3gain_minmax = -77;

    if(!fd){
        info->infile = aud_vfs_fopen(info->filename, "rb");
        if (info->infile == NULL) {
            return FALSE;
        }
    }
    else{
        AUDDBG("input_init: aud_vfs_dup\n");

        info->infile = aud_vfs_dup(fd);
    }

    // obtain file size
    info->size = aud_vfs_fsize(info->infile);

    return TRUE;
}

/* return length in letters */
size_t
mad_ucs4len(id3_ucs4_t *ucs)
{
    id3_ucs4_t *ptr = ucs;
    size_t len = 0;

    while(*ptr++ != 0)
        len++;

    return len;
}

/* duplicate id3_ucs4_t string. new string will be terminated with 0. */
id3_ucs4_t *
mad_ucs4dup(id3_ucs4_t *org)
{
    id3_ucs4_t *res;
    size_t len = mad_ucs4len(org);

    res = g_malloc0((len + 1) * sizeof(id3_ucs4_t));
    memcpy(res, org, len * sizeof(id3_ucs4_t));
    *(res + len) = 0; //terminate

    return res;
}

#define BYTES(x) ((x) * sizeof(id3_ucs4_t))

id3_ucs4_t *
mad_parse_genre(const id3_ucs4_t *string)
{
    id3_ucs4_t *ret = NULL;
    id3_ucs4_t *tmp = NULL;
    id3_ucs4_t *genre = NULL;
    id3_ucs4_t *ptr, *end, *tail, *tp;
    size_t ret_len = 0; //num of ucs4 char!
    size_t tmp_len = 0;
    size_t string_len = 0;
    gboolean is_num = TRUE;

    if(!string)
        return NULL;

    string_len = mad_ucs4len((id3_ucs4_t *)string);
    tail = (id3_ucs4_t *)string + string_len;

    if(BYTES(string_len + 1) > 1024) {
        ret = g_malloc0(BYTES(string_len + 1));
    }
    else {
        ret = g_malloc0(1024);
    }

    for(ptr = (id3_ucs4_t *)string; ptr <= tail && *ptr != 0; ptr++) {
        if(*ptr == '(') {
            if(ptr < tail && *(++ptr) == '(') { // escaped text like: ((something)
                for(end = ptr; *end != ')' && *end != 0;) { // copy "(something)"
                    end++;
                }
                end++; //include trailing ')'
                memcpy(ret, ptr, BYTES(end - ptr));
                ret_len += (end - ptr);
                *(ret + ret_len) = 0; //terminate
                ptr = end + 1;
            }
            else if (ptr <= tail && *ptr != 0) {
                // reference to an id3v1 genre code
                for(tmp_len = 0, end = ptr; *end != ')' && *end != 0;) {
                    end++;
                    tmp_len++;
                }

                tmp = g_malloc0(BYTES(tmp_len + 1));
                memcpy(tmp, ptr, BYTES(tmp_len));
                *(tmp + tmp_len) = 0; //terminate
                ptr += tmp_len;

                /* id3_genre_name may, in some cases, return the given string
                 * so we must free it after we're done, not before.
                 */
                genre = (id3_ucs4_t *)id3_genre_name((const id3_ucs4_t *)tmp);

                tmp_len = mad_ucs4len(genre);
                memcpy(ret + ret_len, genre, BYTES(tmp_len));
                ret_len += tmp_len;
                *(ret + ret_len) = 0; //terminate
                g_free(tmp);
            }
        }
        else {
            for(end = ptr; *end != '(' && *end != 0; ) {
                end++;
            }
            // scan string to determine whether a genre code number or not
            tp = ptr;
            is_num = TRUE;
            while(tp < end) {
                if(*tp < '0' || *tp > '9') { // anything else than number appears.
                    is_num = FALSE;
                    break;
                }
                tp++;
            }

            if(is_num) {
                AUDDBG("is_num!\n");
                tmp_len = end - ptr;
                tmp = g_malloc0(BYTES(tmp_len + 1));
                memcpy(tmp, ptr, BYTES(tmp_len));
                *(tmp + tmp_len) = 0; //terminate
                ptr += tmp_len;

                /* id3_genre_name may, in some cases, return the given string
                 * so we must free it after we're done, not before.
                 */
                genre = (id3_ucs4_t *)id3_genre_name((const id3_ucs4_t *)tmp);
                AUDDBG("genre length = %zd\n", mad_ucs4len(genre));

                tmp_len = mad_ucs4len(genre);
                memcpy(ret + ret_len, genre, BYTES(tmp_len));
                ret_len += tmp_len;
                *(ret + ret_len) = 0; //terminate
                g_free(tmp);
            }
            else { // plain text
                tmp_len = end - ptr;
                AUDDBG("plain!\n");
                AUDDBG("ret_len = %zd\n", ret_len);
                AUDDBG("end - ptr = %lu\n", BYTES(tmp_len));

                memcpy(ret + ret_len, ptr, BYTES(tmp_len));
                ret_len = ret_len + tmp_len;
                *(ret + ret_len) = 0; //terminate
                ptr += tmp_len;
            }
        }
    }
    return ret;
}

gchar *
input_id3_get_string(struct id3_tag * tag, const gchar *frame_name)
{
    gchar *rtn0 = NULL, *rtn = NULL;
    const id3_ucs4_t *string_const = NULL;
    id3_ucs4_t *string = NULL;
    struct id3_frame *frame;
    union id3_field *field;
    int encoding = -1;

    frame = id3_tag_findframe(tag, frame_name, 0);
    if (!frame)
        return NULL;

    field = id3_frame_field(frame, 0);
    encoding = id3_field_gettextencoding(field);

    if (!strcmp(frame_name, ID3_FRAME_COMMENT))
        field = id3_frame_field(frame, 3);
    else
        field = id3_frame_field(frame, 1);

    if (!field)
        return NULL;

    if (!strcmp(frame_name, ID3_FRAME_COMMENT))
        string_const = id3_field_getfullstring(field);
    else
        string_const = id3_field_getstrings(field, 0);

    if (!string_const)
        return NULL;

    if (!strcmp(frame_name, ID3_FRAME_GENRE)) {
        string = mad_parse_genre(string_const);
    }
    else {
        string = mad_ucs4dup((id3_ucs4_t *)string_const);
    }

    if (!string)
        return NULL;

    switch (encoding) {
    case ID3_FIELD_TEXTENCODING_ISO_8859_1:
        rtn0 = (gchar *)id3_ucs4_latin1duplicate(string);
        rtn = aud_str_to_utf8(rtn0);
        g_free(rtn0);
        break;
    case ID3_FIELD_TEXTENCODING_UTF_8:
    default:
        rtn = (gchar *)id3_ucs4_utf8duplicate(string);
        break;
    }
    g_free((void *)string);

    AUDDBG("i: string = %s\n", rtn);

    return rtn;
}

static void
input_set_and_free_tag(struct id3_tag *tag, Tuple *tuple, const gchar *frame, const gint nfield)
{
    gchar *scratch = input_id3_get_string(tag, frame);

    aud_tuple_associate_string(tuple, nfield, NULL, scratch);
    aud_tuple_associate_string(tuple, -1, frame, scratch);

    g_free(scratch);
}

/**
 * read the ID3 tag
 */
static void
input_read_tag(struct mad_info_t *info)
{
    gchar *string;
    glong curpos = 0;

    g_return_if_fail (info->tuple != NULL);

    if(info->infile) {
        curpos = aud_vfs_ftell(info->infile);
        info->id3file = id3_file_vfsopen(info->infile, ID3_FILE_MODE_READONLY);
    }
    else {
        info->id3file = id3_file_open(info->filename, ID3_FILE_MODE_READONLY);
    }

    if (!info->id3file) {
        AUDDBG("read_tag: no id3file\n");
        return;
    }

    info->tag = id3_file_tag(info->id3file);
    if (!info->tag) {
        AUDDBG("read_tag: no tag\n");
        return;
    }

    input_set_and_free_tag(info->tag, info->tuple, ID3_FRAME_ARTIST, FIELD_ARTIST);
    input_set_and_free_tag(info->tag, info->tuple, ID3_FRAME_TITLE, FIELD_TITLE);
    input_set_and_free_tag(info->tag, info->tuple, ID3_FRAME_ALBUM, FIELD_ALBUM);
    input_set_and_free_tag(info->tag, info->tuple, ID3_FRAME_GENRE, FIELD_GENRE);
    input_set_and_free_tag(info->tag, info->tuple, ID3_FRAME_COMMENT, FIELD_COMMENT);

    string = input_id3_get_string(info->tag, ID3_FRAME_TRACK);
    if (string) {
        aud_tuple_associate_int(info->tuple, FIELD_TRACK_NUMBER, NULL, atoi(string));
        g_free(string);
    }

    // year
    string = input_id3_get_string(info->tag, ID3_FRAME_YEAR);   //TDRC
    if (!string)
        string = input_id3_get_string(info->tag, "TYER");

    if (string) {
        aud_tuple_associate_int(info->tuple, FIELD_YEAR, NULL, atoi(string));
        g_free(string);
    }

    // length
    string = input_id3_get_string(info->tag, "TLEN");
    if (string && atoi(string)) {
        aud_tuple_associate_int(info->tuple, FIELD_LENGTH, NULL, atoi(string));
        AUDDBG("input_read_tag: TLEN = %d\n", atoi(string));
        g_free(string);
    }

    aud_tuple_associate_string(info->tuple, FIELD_CODEC, NULL, "MPEG Audio (MP3)");
    aud_tuple_associate_string(info->tuple, FIELD_QUALITY, NULL, "lossy");

    if (info->infile != NULL)
        aud_vfs_fseek(info->infile, curpos, SEEK_SET);

    AUDDBG("e: input_read_tag\n");
}

static gchar * get_stream_metadata (VFSFile * file, const gchar * name)
{
    gchar * raw = aud_vfs_get_metadata (file, name);
    gchar * converted = (raw != NULL && raw[0]) ? aud_str_to_utf8 (raw) : NULL;

    g_free (raw);
    return converted;
}

static gboolean update_stream_metadata (VFSFile * file, const gchar * name,
 Tuple * tuple, gint item)
{
    const gchar * old = aud_tuple_get_string (tuple, item, NULL);
    gchar * new = get_stream_metadata (file, name);
    gboolean changed = (new != NULL && (old == NULL || strcmp (old, new)));

    if (changed)
        aud_tuple_associate_string (tuple, item, NULL, new);

    g_free (new);
    return changed;
}

void input_process_remote_metadata (struct mad_info_t * info)
{
    gboolean changed = FALSE;

    changed = changed || update_stream_metadata (info->infile, "track-name",
     info->tuple, FIELD_TITLE);
    changed = changed || update_stream_metadata (info->infile, "stream-name",
     info->tuple, FIELD_ALBUM);

    if (! changed)
        return;

    mowgli_object_ref (info->tuple);
    info->playback->set_tuple (info->playback, info->tuple);
}


/**
 * Retrieve meta-information about URL.
 * For local files this means ID3 tag etc.
 */
gboolean input_get_info (struct mad_info_t * info)
{
    g_return_val_if_fail (info->tuple == NULL, FALSE);

    info->tuple = tuple_new ();
    tuple_set_filename (info->tuple, info->filename);

    input_read_tag (info);
    audmad_read_replaygain (info);

    if (! scan_file (info, aud_vfs_is_remote (info->filename) ? TRUE :
     audmad_config->fast_play_time_calc))
        return FALSE;

    if (info->length > 0)
        tuple_associate_int (info->tuple, FIELD_LENGTH, NULL, info->length);

    /* reset the input file to the start */
    aud_vfs_fseek(info->infile, 0, SEEK_SET);
    info->offset = 0;

    AUDDBG("e: input_get_info\n");
    return TRUE;
}



/**
 * Read data from the source given my madinfo into the buffer
 * provided.  Return the number of bytes read.
 * @return 0 on EOF
 * @return -1 on error
 */
// this function may be called before info->playback initialized.
int
input_get_data(struct mad_info_t *info, guchar * buffer,
               int buffer_size)
{
    int len = 0;
#ifdef DEBUG_INTENSIVELY
  AUDDBG ("f: input_get_data: %d\n", buffer_size);
#endif
    /* simply read to data from the file */
    len = aud_vfs_fread(buffer, 1, buffer_size, info->infile); //aud_vfs_fread returns num of elements.

    if(len == 0 && info->playback){
        info->playback->eof = TRUE;
    }

#ifdef DEBUG_INTENSIVELY
    AUDDBG ("e: input_get_data: size=%d offset=%d\n", len, info->offset);
#endif

    info->offset += len;
    return len;
}

/**
 * Free up all mad_info_t related resourses.
 */
gboolean input_term (struct mad_info_t * info)
{
    g_free(info->filename);
    if (info->infile)
        aud_vfs_fclose(info->infile);
    if (info->id3file)
        id3_file_close(info->id3file);

    g_free(info->replaygain_album_str);
    g_free(info->replaygain_track_str);
    g_free(info->replaygain_album_peak_str);
    g_free(info->replaygain_track_peak_str);
    g_free(info->mp3gain_undo_str);
    g_free(info->mp3gain_minmax_str);

    if (info->tuple) {
        aud_tuple_free(info->tuple);
        info->tuple = NULL;
    }

    /* set everything to zero in case it gets used again. */
    memset(info, 0, sizeof(struct mad_info_t));

    AUDDBG("e: input_term\n");

    return TRUE;
}
