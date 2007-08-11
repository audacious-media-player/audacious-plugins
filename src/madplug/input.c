/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include <audacious/util.h>

#include "input.h"
#include "replaygain.h"

#define DIR_SEPARATOR '/'
#define HEADER_SIZE 256
#define LINE_LENGTH 256

extern gboolean scan_file(struct mad_info_t *info, gboolean fast);

/**
 * init the mad_info_t struct.
 */
gboolean input_init(struct mad_info_t * info, const char *url, VFSFile *fd)
{
#ifdef DEBUG
    g_message("f: input_init");
#endif
    memset(info, 0, sizeof(struct mad_info_t));

    info->fmt = FMT_S16_LE;
    info->channels = -1;
    info->mpeg_layer = -1;
    info->size = -1;
    info->freq = -1;
    info->seek = -1;
    info->duration = mad_timer_zero;
    info->pos = mad_timer_zero;
    info->url = g_strdup(url);
    info->current_frame = 0;
    info->frames = 0;
    info->bitrate = 0;
    info->vbr = 0;
    info->mode = 0;
    info->title = 0;
    info->offset = 0;
    info->prev_title = NULL;

    info->replaygain_album_str = 0;
    info->replaygain_track_str = 0;
    info->replaygain_album_peak_str = 0;
    info->replaygain_track_peak_str = 0;
    info->mp3gain_undo_str = 0;
    info->mp3gain_minmax_str = 0;

    // from input_read_replaygain()
    info->has_replaygain = FALSE;
    info->replaygain_album_scale = -1;
    info->replaygain_track_scale = -1;
    info->mp3gain_undo = -77;
    info->mp3gain_minmax = -77;

    info->tuple = NULL;

    info->filename = g_strdup(url);

    if(!fd){
        info->infile = vfs_fopen(info->filename, "rb");
        if (info->infile == NULL) {
            return FALSE;
        }
    }
    else{
#ifdef DEBUG
        printf("input_init: vfs_dup\n");
#endif
        info->infile = vfs_dup(fd);
    }

    // obtain file size
    info->size = vfs_fsize(info->infile);
    info->remote = info->size == 0 ? TRUE : FALSE; //proxy connection may result in non-zero size.
    if(audmad_is_remote((gchar *)url))
        info->remote = TRUE;

    info->fileinfo_request = FALSE;

#ifdef DEBUG
    g_message("i: info->size = %lu", (long unsigned int)info->size);
    g_message("e: input_init");
#endif
    return TRUE;
}

/* return length in letters */
size_t mad_ucs4len(id3_ucs4_t *ucs)
{
    id3_ucs4_t *ptr = ucs;
    size_t len = 0;

    while(*ptr++ != 0)
        len++;

    return len;
}

/* duplicate id3_ucs4_t string. new string will be terminated with 0. */
id3_ucs4_t *mad_ucs4dup(id3_ucs4_t *org)
{
    id3_ucs4_t *new = NULL;
    size_t len = mad_ucs4len(org);

    new = g_malloc0((len + 1) * sizeof(id3_ucs4_t));
    memcpy(new, org, len * sizeof(id3_ucs4_t));
    *(new + len) = 0; //terminate

    return new;
}

#define BYTES(x) ((x) * sizeof(id3_ucs4_t))

id3_ucs4_t *mad_parse_genre(const id3_ucs4_t *string)
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

    for(ptr = (id3_ucs4_t *)string; *ptr != 0 && ptr <= tail; ptr++) {
        if(*ptr == '(') {
            if(*(++ptr) == '(') { // escaped text like: ((something)
                for(end = ptr; *end != ')' && *end != 0;) { // copy "(something)"
                    end++;
                }
                end++; //include trailing ')'
                memcpy(ret, ptr, BYTES(end - ptr));
                ret_len += (end - ptr);
                *(ret + ret_len) = 0; //terminate
                ptr = end + 1;
            }
            else {
                // reference to an id3v1 genre code
                for(end = ptr; *end != ')' && *end != 0;) {
                    end++;
                }

                tmp = g_malloc0(BYTES(end - ptr + 1));
                memcpy(tmp, ptr, BYTES(end - ptr));
                *(tmp + (end - ptr)) = 0; //terminate
                ptr += end - ptr;

                genre = (id3_ucs4_t *)id3_genre_name((const id3_ucs4_t *)tmp);

                g_free(tmp);
                tmp = NULL;

                tmp_len = mad_ucs4len(genre);

                memcpy(ret + BYTES(ret_len), genre, BYTES(tmp_len));

                ret_len += tmp_len;
                *(ret + ret_len) = 0; //terminate
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
#ifdef DEBUG
                printf("is_num!\n");
#endif
                tmp = g_malloc0(BYTES(end - ptr + 1));
                memcpy(tmp, ptr, BYTES(end - ptr));
                *(tmp + (end - ptr)) = 0; //terminate
                ptr += end - ptr;

                genre = (id3_ucs4_t *)id3_genre_name((const id3_ucs4_t *)tmp);
#ifdef DEBUG
                printf("genre length = %d\n", mad_ucs4len(genre));
#endif
                g_free(tmp);
                tmp = NULL;

                tmp_len = mad_ucs4len(genre);

                memcpy(ret + BYTES(ret_len), genre, BYTES(tmp_len));

                ret_len += tmp_len;
                *(ret + ret_len) = 0; //terminate
            }
            else { // plain text
#ifdef DEBUG
                printf("plain!\n");
                printf("ret_len = %d\n", ret_len);
                printf("end - ptr = %d\n", BYTES(end - ptr));
#endif
                memcpy(ret + BYTES(ret_len), ptr, BYTES(end - ptr));
                ret_len = ret_len + (end - ptr);
                *(ret + ret_len) = 0; //terminate
                ptr += (end - ptr);
            }
        }
    }
    return ret;
}

gchar *input_id3_get_string(struct id3_tag * tag, const gchar *frame_name)
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
        rtn = str_to_utf8(rtn0);
        g_free(rtn0);
        break;
    case ID3_FIELD_TEXTENCODING_UTF_8:
    default:
        rtn = (gchar *)id3_ucs4_utf8duplicate(string);
        break;
    }
    g_free((void *)string);
        
#ifdef DEBUG
    g_print("i: string = %s\n", rtn);
#endif
    return rtn;
}

static void input_set_and_free_tag(struct id3_tag *tag, Tuple *tuple, const gchar *frame, const gchar *tuple_name)
{
    gchar *scratch = input_id3_get_string(tag, frame);

    tuple_associate_string(tuple, tuple_name, scratch);
    tuple_associate_string(tuple, frame, scratch);

    g_free(scratch);
}

static void input_alloc_tag(struct mad_info_t *info)
{
    Tuple *title_input;

    if (info->tuple == NULL) {
        title_input = tuple_new();
        info->tuple = title_input;
        tuple_associate_int(info->tuple, "length", -1);
    }
    else
        title_input = info->tuple;
}

/**
 * read the ID3 tag 
 */
static void input_read_tag(struct mad_info_t *info)
{
    gchar *string = NULL;
    gchar *realfn = NULL;
    Tuple *title_input;
    glong curpos = 0;

#ifdef DEBUG
    g_message("f: input_read_tag");
#endif
    if (info->tuple == NULL) {
        title_input = tuple_new();
        info->tuple = title_input;
    }
    else
        title_input = info->tuple;

    if(info->infile) {
        curpos = vfs_ftell(info->infile);
        info->id3file = id3_file_vfsopen(info->infile, ID3_FILE_MODE_READONLY);
    }
    else {
        info->id3file = id3_file_open(info->filename, ID3_FILE_MODE_READONLY);
    }

    if (!info->id3file) {
#ifdef DEBUG
        g_message("read_tag: no id3file");
#endif
        return;
    }

    info->tag = id3_file_tag(info->id3file);
    if (!info->tag) {
#ifdef DEBUG
        g_message("read_tag: no tag");
#endif
        return;
    }

    input_set_and_free_tag(info->tag, title_input, ID3_FRAME_ARTIST, "artist");
    input_set_and_free_tag(info->tag, title_input, ID3_FRAME_TITLE, "title");
    input_set_and_free_tag(info->tag, title_input, ID3_FRAME_ALBUM, "album");
    input_set_and_free_tag(info->tag, title_input, ID3_FRAME_GENRE, "genre");
    input_set_and_free_tag(info->tag, title_input, ID3_FRAME_COMMENT, "comment");

    string = input_id3_get_string(info->tag, ID3_FRAME_TRACK);
    if (string) {
        tuple_associate_int(title_input, "track-number", atoi(string));
        g_free(string);
        string = NULL;
    }

    // year
    string = NULL;
    string = input_id3_get_string(info->tag, ID3_FRAME_YEAR);   //TDRC
    if (!string)
        string = input_id3_get_string(info->tag, "TYER");

    if (string) {
        tuple_associate_int(title_input, "year", atoi(string));
        g_free(string);
        string = NULL;
    }

    // length
    string = input_id3_get_string(info->tag, "TLEN");
    if (string) {
        tuple_associate_int(title_input, "length", atoi(string));
#ifdef DEBUG
        g_message("input_read_tag: TLEN = %d", atoi(string));
#endif	
        g_free(string);
        string = NULL;
    }
    
    realfn = g_filename_from_uri(info->filename, NULL, NULL);
    
    string = g_strdup(g_basename(realfn ? realfn : info->filename));
    tuple_associate_string(title_input, "file-name", string);
    g_free(string);

    string = g_path_get_dirname(realfn ? realfn : info->filename);
    tuple_associate_string(title_input, "file-path", string);
    g_free(string);

    if ((string = strrchr(realfn ? realfn : info->filename, '.'))) {
        *string = '\0';         // make filename end at dot.
        tuple_associate_string(title_input, "file-ext", string + 1);
    }

    g_free(realfn); realfn = NULL;

    tuple_associate_string(title_input, "codec", "MPEG Audio (MP3)");
    tuple_associate_string(title_input, "quality", "lossy");

    info->title = tuple_formatter_process_string(title_input, audmad_config.title_override == TRUE ?
        audmad_config.id3_format : cfg.gentitle_format);

    // for connection via proxy, we have to stop transfer once. I can't explain the reason.
    if (info->infile != NULL) {
        vfs_fseek(info->infile, -1, SEEK_SET); // an impossible request
        vfs_fseek(info->infile, curpos, SEEK_SET);
    }
    
#ifdef DEBUG
    g_message("e: input_read_tag");
#endif
}

void input_process_remote_metadata(struct mad_info_t *info)
{
    gboolean metadata = FALSE;

    if(info->remote && mad_timer_count(info->duration, MAD_UNITS_SECONDS) <= 0){
        gchar *tmp = NULL;
#ifdef DEBUG
#ifdef DEBUG_INTENSIVELY
        g_message("process_remote_meta");
#endif
#endif

        g_free(info->title);
        info->title = NULL;
        tuple_disassociate(info->tuple, "title");
        tuple_disassociate(info->tuple, "album");

        tmp = vfs_get_metadata(info->infile, "track-name");
        if(tmp){
            metadata = TRUE;
            gchar *scratch;

            scratch = str_to_utf8(tmp);
            tuple_associate_string(info->tuple, "title", scratch);
            g_free(scratch);

            g_free(tmp);
            tmp = NULL;
        }

        tmp = vfs_get_metadata(info->infile, "stream-name");
        if(tmp){
            metadata = TRUE;
            gchar *scratch;

            scratch = str_to_utf8(tmp);
            tuple_associate_string(info->tuple, "album", scratch);
            tuple_associate_string(info->tuple, "stream", scratch);
            g_free(scratch);

            g_free(tmp);
            tmp = NULL;
        }

        if (metadata)
            tmp = tuple_formatter_process_string(info->tuple, "${?title:${title}}${?stream: (${stream})");
        else {
            gchar *realfn = g_filename_from_uri(info->filename, NULL, NULL);
            gchar *tmp2 = g_path_get_basename(realfn ? realfn : info->filename); // info->filename is uri. --yaz
            tmp = str_to_utf8(tmp2);
            g_free(tmp2); tmp2 = NULL;
            g_free(realfn); realfn = NULL;
//            tmp = g_strdup(g_basename(info->filename)); //XXX maybe ok. --yaz
        }

        /* call set_info only if tmp is different from prev_tmp */
        if ( ( ( info->prev_title != NULL ) && ( strcmp(info->prev_title,tmp) ) ) ||
             ( info->prev_title == NULL ) )
        {
            mad_plugin->set_info(tmp,
                                 -1, // indicate the stream is unseekable
                                 info->bitrate, info->freq, info->channels);
            if (info->prev_title)
                g_free(info->prev_title);
            info->prev_title = g_strdup(tmp);
        }

        g_free(tmp);
    }
}


/**
 * Retrieve meta-information about URL.
 * For local files this means ID3 tag etc.
 */
gboolean input_get_info(struct mad_info_t *info, gboolean fast_scan)
{
#ifdef DEBUG
    gchar *tmp = g_filename_to_utf8(info->filename, -1, NULL, NULL, NULL);    
    g_message("f: input_get_info: %s, fast_scan = %s", tmp, fast_scan ? "TRUE" : "FALSE");
    g_free(tmp);
#endif                          /* DEBUG */

    input_alloc_tag(info);
    input_read_tag(info);

    if(!info->remote) { // reduce startup delay
        read_replaygain(info);
    }

    /* scan mp3 file, decoding headers */
    if (scan_file(info, fast_scan) == FALSE) {
#ifdef DEBUG
        g_message("input_get_info: scan_file failed");
#endif
        return FALSE;
    }

    /* reset the input file to the start */
    vfs_fseek(info->infile, 0, SEEK_SET);
    info->offset = 0;

    /* use the filename for the title as a last resort */
    if (!info->title) {
        char *pos = strrchr(info->filename, DIR_SEPARATOR); //XXX info->filename is uri. --yaz
        if (pos)
            info->title = g_strdup(pos + 1);
        else
            info->title = g_strdup(info->filename); //XXX info->filename is uri. --yaz
    }

#ifdef DEBUG
    g_message("e: input_get_info");
#endif                          /* DEBUG */
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
#ifdef DEBUG
#ifdef DEBUG_INTENSIVELY
  g_message ("f: input_get_data: %d", buffer_size);
#endif
#endif
    /* simply read to data from the file */
    len = vfs_fread(buffer, 1, buffer_size, info->infile); //vfs_fread returns num of elements.

    if(len == 0 && info->playback){
        info->playback->eof = TRUE;
    }

#ifdef DEBUG
#ifdef DEBUG_INTENSIVELY
    g_message ("e: input_get_data: size=%d offset=%d", len, info->offset);
#endif
#endif
    info->offset += len;
    return len;
}

/**
 * Free up all mad_info_t related resourses.
 */
gboolean input_term(struct mad_info_t * info)
{
#ifdef DEBUG
    g_message("f: input_term");
#endif

    if (info->title)
        g_free(info->title);
    if (info->url)
        g_free(info->url);
    if (info->filename)
        g_free(info->filename);
    if (info->infile)
        vfs_fclose(info->infile);
    if (info->id3file)
        id3_file_close(info->id3file);

    if (info->replaygain_album_str)
        g_free(info->replaygain_album_str);
    if (info->replaygain_track_str)
        g_free(info->replaygain_track_str);
    if (info->replaygain_album_peak_str)
        g_free(info->replaygain_album_peak_str);
    if (info->replaygain_track_peak_str)
        g_free(info->replaygain_track_peak_str);
    if (info->mp3gain_undo_str)
        g_free(info->mp3gain_undo_str);
    if (info->mp3gain_minmax_str)
        g_free(info->mp3gain_minmax_str);

    if (info->tuple) {
        tuple_free(info->tuple);
        info->tuple = NULL;
    }

    if (info->prev_title)
        g_free(info->prev_title);

    /* set everything to zero in case it gets used again. */
    memset(info, 0, sizeof(struct mad_info_t));
#ifdef DEBUG
    g_message("e: input_term");
#endif
    return TRUE;
}
