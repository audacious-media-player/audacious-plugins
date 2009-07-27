/*
 * libtta.c
 *
 * Description:	 TTA input plug-in for Audacious
 * Developed by: Alexander Djourik <ald@true-audio.com>
 * Audacious port: Yoshiki Yazawa <yaz@cc.rim.or.jp>
 *
 * Copyright (c) 2007 Alexander Djourik. All rights reserved.
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Please see the file COPYING in this directory for full copyright
 * information.
 */

/* #define AUD_DEBUG 1 */

#include "config.h"

#include <stdio.h>
#include <glib.h>
#include <string.h>

#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <audacious/id3tag.h>

#define  PLUGIN_VERSION "1.5"
#define  PROJECT_URL "<http://www.true-audio.com>"

#include "ttalib.h"

#define OUTPUT_ERROR (MEMORY_ERROR+1)
#define MAX_BSIZE (MAX_BPS>>3)
#define BYTES(x) ((x) * sizeof(id3_ucs4_t))

static GMutex *seek_mutex = NULL;
static GCond *seek_cond = NULL;
static gint64 seek_value = -1;


static void tta_error(gint error)
{
    gchar *message;
    static GtkWidget *errorbox = NULL;

    if (errorbox != NULL)
        return;

    switch (error)
    {
      case OPEN_ERROR:
          message = _("Can't open file\n");
          break;
      case FORMAT_ERROR:
          message = _("Not supported file format\n");
          break;
      case FILE_ERROR:
          message = _("File is corrupted\n");
          break;
      case READ_ERROR:
          message = _("Can't read from file\n");
          break;
      case MEMORY_ERROR:
          message = _("Insufficient memory available\n");
          break;
      case OUTPUT_ERROR:
          message = _("Output plugin error\n");
          break;
      default:
          message = _("Unknown error\n");
          break;
    }

    audacious_info_dialog(_("TTA Decoder Error"), message, _("Ok"), FALSE, NULL, NULL);

    gtk_signal_connect(GTK_OBJECT(errorbox), "destroy", G_CALLBACK(gtk_widget_destroyed), &errorbox);
}

static Tuple *tta_get_song_tuple(const gchar * filename)
{
    Tuple *tuple = aud_tuple_new_from_filename(filename);
    tta_info ttainfo;

    if (open_tta_file(filename, &ttainfo, 0) >= 0)
    {
        aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, "True Audio (TTA)");
        aud_tuple_associate_string(tuple, FIELD_QUALITY, NULL, "lossless");

        if (ttainfo.ID3.id3has)
        {
            if (ttainfo.ID3.artist)
                aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, (gchar *) ttainfo.ID3.artist);

            if (ttainfo.ID3.album)
                aud_tuple_associate_string(tuple, FIELD_ALBUM, NULL, (gchar *) ttainfo.ID3.album);

            if (ttainfo.ID3.title)
                aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, (gchar *) ttainfo.ID3.title);

            if (ttainfo.ID3.year)
                aud_tuple_associate_int(tuple, FIELD_YEAR, NULL, atoi((gchar *) ttainfo.ID3.year));

            if (ttainfo.ID3.track)
                aud_tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi((gchar *) ttainfo.ID3.track));

            if (ttainfo.ID3.genre)
                aud_tuple_associate_string(tuple, FIELD_GENRE, NULL, (gchar *) ttainfo.ID3.genre);

            if (ttainfo.ID3.comment)
                aud_tuple_associate_string(tuple, FIELD_COMMENT, NULL, (gchar *) ttainfo.ID3.comment);

            if (ttainfo.LENGTH)
                aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, 1000 * ttainfo.LENGTH);

        }

        close_tta_file(&ttainfo);
    }

    return tuple;
}

static void tta_init()
{
    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();
}

static void tta_cleanup()
{
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond); 
}

static void tta_about()
{
    static GtkWidget *aboutbox = NULL;
    gchar *about_text;

    if (aboutbox != NULL)
        return;

    about_text = g_strjoin("",
        _("TTA input plugin "), PLUGIN_VERSION,
        _(" for BMP\n" "Copyright (c) 2004 True Audio Software\n"),
        PROJECT_URL, NULL);

    aboutbox = audacious_info_dialog(
        _("About True Audio Plugin"),
        about_text, _("Ok"), FALSE, NULL, NULL);

    gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
        G_CALLBACK(gtk_widget_destroyed), &aboutbox);
    g_free(about_text);
}

static gint tta_is_our_file(const gchar * filename)
{
    gchar *ext = strrchr(filename, '.');

    if (ext && !strncasecmp(ext, ".tta", 4))
        return TRUE;

    return FALSE;
}

static void tta_play_file(InputPlayback * playback)
{
    gsize bufsize, read_samples;
    guint8 *sample_buffer = NULL;
    tta_info info;
    gchar *title;
    gsize datasize, origsize, bitrate;
    Tuple *tuple;

    if (open_tta_file(playback->filename, &info, 0) > 0)
    {
        tta_error(info.STATE);
        goto error_exit;
    }

    if (player_init(&info) < 0)
    {
        tta_error(info.STATE);
        goto error_exit;
    }

    bufsize = PCM_BUFFER_LENGTH * info.BSIZE * info.NCH;
    if ((sample_buffer = g_malloc(bufsize)) == NULL)
        goto error_exit;

    if (playback->output->open_audio((info.BPS == 8) ? FMT_U8 : FMT_S16_LE, info.SAMPLERATE, info.NCH) == 0)
    {
        tta_error(OUTPUT_ERROR);
        playback->error = TRUE;
        goto error_exit;
    }

    tuple = tta_get_song_tuple(playback->filename);
    title = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
    aud_tuple_free(tuple);

    datasize = aud_vfs_fsize(info.HANDLE) - info.DATAPOS;
    origsize = info.DATALENGTH * info.BSIZE * info.NCH;

    bitrate = ((gfloat)datasize / origsize * (info.SAMPLERATE * info.NCH * info.BPS));
    playback->set_params(playback, title, 1000 * info.LENGTH, bitrate, info.SAMPLERATE, info.NCH);
    g_free(title);

    playback->playing = 1;
    playback->set_pb_ready(playback);


    ////////////////////////////////////////
    // decode PCM_BUFFER_LENGTH samples
    // into the current PCM buffer position

    while (playback->playing)
    {
        /* Check for seek request and bail out if we have one */
        g_mutex_lock(seek_mutex);
        if (seek_value >= 0)
        {
            set_position(seek_value);
            playback->output->flush(seek_value * SEEK_STEP);
            g_cond_signal(seek_cond);
        }
        g_mutex_unlock(seek_mutex);

        if ((read_samples = get_samples(sample_buffer)) > 0)
        {
            playback->pass_audio(playback,
                ((info.BPS == 8) ? FMT_U8 : FMT_S16_LE),
                info.NCH, read_samples * info.NCH * info.BSIZE,
                sample_buffer, &playback->playing);
        }
        else
        {
            playback->playing = 0;
            playback->eof = TRUE;
        }
    }

error_exit:
    player_stop();
    close_tta_file(&info);
    playback->playing = 0;
}

static void tta_pause(InputPlayback * playback, gshort paused)
{
    playback->output->pause(paused);
}

static void tta_stop(InputPlayback * playback)
{
    playback->playing = 0;
}

static void tta_mseek(InputPlayback * data, gulong millisec)
{
    g_mutex_lock(seek_mutex);
    seek_value = millisec / SEEK_STEP;
    g_cond_wait(seek_cond, seek_mutex);
    g_mutex_unlock(seek_mutex);
}

static void tta_seek(InputPlayback * data, gint sec)
{
    gulong millisec = 1000 * sec;
    tta_mseek(data, millisec);
}

/* return length in letters */
gsize tta_ucs4len(id3_ucs4_t * ucs)
{
    id3_ucs4_t *ptr = ucs;
    gsize len = 0;

    while (*ptr++ != 0)
        len++;

    return len;
}

/* duplicate id3_ucs4_t string. new string will be terminated with 0. */
id3_ucs4_t *tta_ucs4dup(id3_ucs4_t * org)
{
    id3_ucs4_t *str;
    gsize len = tta_ucs4len(org);

    str = g_new0(id3_ucs4_t, len + 1);
    memcpy(str, org, len * sizeof(id3_ucs4_t));
    *(str + len) = 0;

    return str;
}

id3_ucs4_t *tta_parse_genre(const id3_ucs4_t * string)
{
    id3_ucs4_t *ret = NULL;
    id3_ucs4_t *tmp = NULL;
    id3_ucs4_t *genre = NULL;
    id3_ucs4_t *ptr, *end, *tail, *tp;
    size_t ret_len = 0;         //num of ucs4 char!
    size_t tmp_len = 0;
    gboolean is_num = TRUE;

    tail = (id3_ucs4_t *) string + tta_ucs4len((id3_ucs4_t *) string);

    ret = g_malloc0(1024);

    for (ptr = (id3_ucs4_t *) string; *ptr != 0 && ptr <= tail; ptr++)
    {
        if (*ptr == '(')
        {
            if (*(++ptr) == '(')
            {                   // escaped text like: ((something)
                for (end = ptr; *end != ')' && *end != 0;)
                {               // copy "(something)"
                    end++;
                }
                end++;          //include trailing ')'
                memcpy(ret, ptr, BYTES(end - ptr));
                ret_len += (end - ptr);
                *(ret + ret_len) = 0;   //terminate
                ptr = end + 1;
            }
            else
            {
                // reference to an id3v1 genre code
                for (end = ptr; *end != ')' && *end != 0;)
                {
                    end++;
                }

                tmp = g_malloc0(BYTES(end - ptr + 1));
                memcpy(tmp, ptr, BYTES(end - ptr));
                *(tmp + (end - ptr)) = 0;       //terminate
                ptr += end - ptr;

                genre = (id3_ucs4_t *) id3_genre_name((const id3_ucs4_t *)tmp);

                g_free(tmp);
                tmp = NULL;

                tmp_len = tta_ucs4len(genre);

                memcpy(ret + BYTES(ret_len), genre, BYTES(tmp_len));

                ret_len += tmp_len;
                *(ret + ret_len) = 0;   //terminate
            }
        }
        else
        {
            for (end = ptr; *end != '(' && *end != 0;) end++;

            // scan string to determine whether a genre code number or not
            for (is_num = TRUE, tp = ptr; tp < end; tp++)
            if (!g_ascii_isdigit(*tp))
            {               // anything else than number appears.
                is_num = FALSE;
                break;
            }

            if (is_num)
            {
                AUDDBG("is_num!\n");
                tmp = g_malloc0(BYTES(end - ptr + 1));
                memcpy(tmp, ptr, BYTES(end - ptr));
                *(tmp + (end - ptr)) = 0;       //terminate
                ptr += end - ptr;

                genre = (id3_ucs4_t *) id3_genre_name((const id3_ucs4_t *)tmp);
                AUDDBG("genre length = %d\n", tta_ucs4len(genre));
                g_free(tmp);
                tmp = NULL;

                tmp_len = tta_ucs4len(genre);

                memcpy(ret + BYTES(ret_len), genre, BYTES(tmp_len));

                ret_len += tmp_len;
                *(ret + ret_len) = 0;   //terminate
            }
            else
            {                   // plain text
                AUDDBG("plain!\n");
                AUDDBG("ret_len = %d\n", ret_len);
                memcpy(ret + BYTES(ret_len), ptr, BYTES(end - ptr));
                ret_len = ret_len + (end - ptr);
                *(ret + ret_len) = 0;   //terminate
                ptr += (end - ptr);
            }
        }
    }

    return ret;
}

gchar *tta_input_id3_get_string(struct id3_tag * tag, gchar *frame_name)
{
    gchar *rtn;
    gchar *rtn2;
    const id3_ucs4_t *string_const;
    id3_ucs4_t *string;
    id3_ucs4_t *ucsptr;
    struct id3_frame *frame;
    union id3_field *field;
    gboolean flagutf = FALSE;

    frame = id3_tag_findframe(tag, frame_name, 0);
    if (frame == NULL)
        return NULL;

    if (!strcmp(frame_name, ID3_FRAME_COMMENT))
        field = id3_frame_field(frame, 3);
    else
        field = id3_frame_field(frame, 1);

    if (field == NULL)
        return NULL;

    if (!strcmp(frame_name, ID3_FRAME_COMMENT))
        string_const = id3_field_getfullstring(field);
    else
        string_const = id3_field_getstrings(field, 0);

    if (string_const == NULL)
        return NULL;

    string = tta_ucs4dup((id3_ucs4_t *) string_const);

    if (!strcmp(frame_name, ID3_FRAME_GENRE))
    {
        id3_ucs4_t *string2 = tta_parse_genre(string);
        g_free(string);
        string = string2;
    }

    ucsptr = (id3_ucs4_t *) string;
    while (*ucsptr)
    {
        if (*ucsptr > 0x000000ffL)
        {
            flagutf = TRUE;
            break;
        }
        ucsptr++;
    }

    if (flagutf)
    {
#ifdef DEBUG
        g_message("aud-tta: flagutf!\n");
#endif
        rtn = (gchar *) id3_ucs4_utf8duplicate(string);
    }
    else
    {
        rtn = (gchar *) id3_ucs4_latin1duplicate(string);
        rtn2 = aud_str_to_utf8(rtn);
        g_free(rtn);
        rtn = rtn2;
    }
    g_free(string);
    AUDDBG("string = %s\n", rtn);

    return rtn;
}

gint get_id3_tags(const gchar *filename, tta_info * ttainfo)
{
    gint id3v2_size = 0;
    struct id3_file *id3file;

    id3file = id3_file_open(filename, ID3_FILE_MODE_READONLY);

    if (id3file)
    {
        struct id3_tag *tag = id3_file_tag(id3file);

        if (tag)
        {
            ttainfo->ID3.id3has = 1;
            id3v2_size = tag->paddedsize;

            ttainfo->ID3.artist = tta_input_id3_get_string(tag, ID3_FRAME_ARTIST);
            ttainfo->ID3.album = tta_input_id3_get_string(tag, ID3_FRAME_ALBUM);
            ttainfo->ID3.title = tta_input_id3_get_string(tag, ID3_FRAME_TITLE);

            ttainfo->ID3.year = tta_input_id3_get_string(tag, ID3_FRAME_YEAR);
            if (ttainfo->ID3.year == NULL)
                ttainfo->ID3.year = tta_input_id3_get_string(tag, "TYER");

            ttainfo->ID3.track = tta_input_id3_get_string(tag, ID3_FRAME_TRACK);
            ttainfo->ID3.genre = tta_input_id3_get_string(tag, ID3_FRAME_GENRE);
            ttainfo->ID3.comment = tta_input_id3_get_string(tag, ID3_FRAME_COMMENT);
        }

        id3_file_close(id3file);
    }

    return id3v2_size;
}


static gchar *tta_fmts[] = { "tta", NULL };

static InputPlugin tta_ip = {
    .description = "True Audio Plugin",
    .init = tta_init,
    .about = tta_about,
    .is_our_file = tta_is_our_file,
    .play_file = tta_play_file,
    .stop = tta_stop,
    .pause = tta_pause,
    .seek = tta_seek,
    .cleanup = tta_cleanup,
    .get_song_tuple = tta_get_song_tuple,
    .vfs_extensions = tta_fmts,
    .mseek = tta_mseek,
};

static InputPlugin *tta_iplist[] = { &tta_ip, NULL };

DECLARE_PLUGIN(tta, NULL, NULL, tta_iplist, NULL, NULL, NULL, NULL, NULL);
