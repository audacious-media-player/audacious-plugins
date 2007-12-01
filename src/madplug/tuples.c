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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include "plugin.h"
#include "tuples.h"

#include <math.h>
#include <string.h>

#include <glib.h> 
#include <glib/gprintf.h>

#include <audacious/util.h>
#include <audacious/plugin.h>
#include <audacious/id3tag.h>

/* yaz */
#include <langinfo.h>

#define DEBUG

static void
update_id3_frame(struct id3_tag *tag, const char *frame_name, const char *data, int sjis)
{
    int res;
    struct id3_frame *frame;
    union id3_field *field;
    id3_ucs4_t *ucs4;

    if (data == NULL)
        return;

    // printf ("updating id3: %s: %s\n", frame_name, data);

    //
    // An empty string removes the frame altogether.
    //
    if (strlen(data) == 0) {
        while ((frame = id3_tag_findframe(tag, frame_name, 0))) {
#ifdef DEBUG
            fprintf(stderr, "madplug: detachframe\n");
#endif
            id3_tag_detachframe(tag, frame);
        }
        return;
    }

    frame = id3_tag_findframe(tag, frame_name, 0);
    if (!frame) {
#ifdef DEBUG
        printf("frame_new\n");
#endif
        frame = id3_frame_new(frame_name);
        id3_tag_attachframe(tag, frame);
    }

    // setup ucs4 string
    if(sjis) {
        ucs4 = id3_latin1_ucs4duplicate((id3_latin1_t *) data);
    }
    else {
        ucs4 = id3_utf8_ucs4duplicate((id3_utf8_t *) data);
    }

    // set encoding
    field = id3_frame_field(frame, 0);
    id3_field_settextencoding(field, sjis ? ID3_FIELD_TEXTENCODING_ISO_8859_1 :
			      ID3_FIELD_TEXTENCODING_UTF_8);

    // setup genre code
    if (!strcmp(frame_name, ID3_FRAME_GENRE)) {
        char *tmp;
        int index = id3_genre_number(ucs4);
        g_free(ucs4);

        if(index == -1) { // unknown genre. remove TCON frame.
#ifdef DEBUG
            fprintf(stderr, "madplug: remove genre frame\n");
#endif
            id3_tag_detachframe(tag, frame);
        }
        else { // meaningful genre
            tmp = g_strdup_printf("%d", index);
            ucs4 = id3_latin1_ucs4duplicate((unsigned char *) tmp);
        }

    }

    // write string
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
update_id3_frame_from_tuple(struct id3_tag *id3tag, const char *field, Tuple *tuple, int fieldn, int sjis)
{   
    int val;
    char *text, *text2;
    const char *encoding = sjis ? "SJIS" : "UTF-8";

    if(aud_tuple_get_value_type(tuple, fieldn, NULL) == TUPLE_INT) {
        val = aud_tuple_get_int(tuple, fieldn, NULL);
        if(val > 0) {
            text2 = g_strdup_printf("%d", val);
#ifdef DEBUG
            fprintf(stderr, "madplug: updating field:\"%s\"=\"%s\", enc %s\n", field, text2, encoding);
#endif
            update_id3_frame(id3tag, field, text2, 0);
            g_free(text2);
        } else {
            update_id3_frame(id3tag, field, "", 0); /* will be detached */
        }

    } else if(aud_tuple_get_value_type(tuple, fieldn, NULL) == TUPLE_STRING) {
        text = (char*)aud_tuple_get_string(tuple, fieldn, NULL);
        text2 = g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
#ifdef DEBUG
        fprintf(stderr, "madplug: updating field:\"%s\"=\"%s\", enc %s\n", field, text2, encoding);
#endif
        update_id3_frame(id3tag, field, text2, sjis);
        g_free(text2);
    }
}

gboolean
audmad_update_song_tuple(Tuple *tuple, VFSFile *fd)
{
    struct id3_file *id3file;
    struct id3_tag *id3tag;

    if ((id3file = id3_file_vfsopen(fd, ID3_FILE_MODE_READWRITE)) == NULL) return FALSE;
    
    id3tag = id3_file_tag(id3file);
    if (!id3tag) {
#ifdef DEBUG
        fprintf(stderr, "no id3tag\n. append new tag.\n");
#endif
        id3tag = id3_tag_new();
        id3_tag_clearframes(id3tag);
        id3tag->options |= ID3_TAG_OPTION_APPENDEDTAG | ID3_TAG_OPTION_ID3V1;
    }

    id3_tag_options(id3tag, ID3_TAG_OPTION_ID3V1, ~0);    /* enables id3v1. TODO: make id3v1 optional */
    
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_TITLE, tuple, FIELD_TITLE, audmad_config.sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_ARTIST, tuple, FIELD_ARTIST, audmad_config.sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_ALBUM, tuple, FIELD_ALBUM, audmad_config.sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_YEAR, tuple, FIELD_YEAR, audmad_config.sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_COMMENT, tuple, FIELD_COMMENT, audmad_config.sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_TRACK, tuple, FIELD_TRACK_NUMBER, audmad_config.sjis);
    update_id3_frame_from_tuple(id3tag, ID3_FRAME_GENRE, tuple, FIELD_GENRE, audmad_config.sjis);

    if (id3_file_update(id3file) != 0) return FALSE;

    id3_file_close(id3file);
    return TRUE;
}

/*#endif                          // !NOGUI

void audmad_get_file_info(char *fileurl)
{
#ifndef NOGUI
    gchar *title;
    gchar message[128];
    static char const *const layer_str[3] = { "I", "II", "III" };
    static char const *const mode_str[4] = {
        ("single channel"), ("dual channel"), "joint stereo", "stereo"
    };
    gchar *tmp, *utf_filename;
    gchar *realfn = NULL;
#ifdef DEBUG
    {
        tmp = aud_str_to_utf8(fileurl);
        g_message("f: audmad_get_file_info: %s", tmp);
        g_free(tmp);
        tmp = NULL;
    }
#endif

    if(!aud_vfs_is_remote(fileurl) && !aud_vfs_file_test(fileurl, G_FILE_TEST_EXISTS)) {
        return;
    }

    input_init(&info, fileurl, NULL);

    if(audmad_is_remote(fileurl)) {
        info.remote = TRUE;
        if(aud_vfs_is_streaming(info.infile))
           return; //file info dialog for remote streaming doesn't make sense.
    }

    realfn = g_filename_from_uri(fileurl, NULL, NULL);
    utf_filename = aud_str_to_utf8(realfn ? realfn : fileurl);
    g_free(realfn); realfn = NULL;
    create_window();

    info.fileinfo_request = TRUE;
    input_get_info(&info, info.remote ? TRUE : FALSE);

    tmp = g_path_get_basename(utf_filename);
    title = g_strdup_printf(_("File Info - %s"), tmp);
    g_free(tmp); tmp = NULL;
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);

    gtk_entry_set_text(GTK_ENTRY(filename_entry), utf_filename);
    gtk_editable_set_position(GTK_EDITABLE(filename_entry), -1);

    free(utf_filename);

    id3_frame_to_entry(ID3_FRAME_ARTIST, GTK_ENTRY(artist_entry));
    id3_frame_to_entry(ID3_FRAME_TITLE, GTK_ENTRY(title_entry));
    id3_frame_to_entry(ID3_FRAME_ALBUM, GTK_ENTRY(album_entry));

// year
// id3_frame_to_entry (ID3_FRAME_YEAR, GTK_ENTRY (year_entry));
// to set year entry, we have to do manually because TYER is still used equally to TDRC.
    gtk_entry_set_text(GTK_ENTRY(year_entry), "");
    if (info.tag) {
        gchar *text = NULL;
        text = input_id3_get_string(info.tag, "TDRC");
        if (!text)
            text = input_id3_get_string(info.tag, "TYER");
        if (text) {
            gtk_entry_set_text(GTK_ENTRY(year_entry), text);
            g_free(text);
        }
    }

    id3_frame_to_entry(ID3_FRAME_TRACK, GTK_ENTRY(tracknum_entry));
    id3_frame_to_entry(ID3_FRAME_COMMENT, GTK_ENTRY(comment_entry));
    snprintf(message, 127, _("Layer %s"), layer_str[info.mpeg_layer - 1]);
    gtk_label_set_text(GTK_LABEL(mpeg_level), message);
    if (info.vbr) {
        snprintf(message, 127, _("VBR (avg. %d kbps)"), info.bitrate / 1000);
    }
    else {
        snprintf(message, 127, "%d kbps", info.bitrate / 1000);
    }
    gtk_label_set_text(GTK_LABEL(mpeg_bitrate), message);
    snprintf(message, 127, _("%d Hz"), info.freq);
    gtk_label_set_text(GTK_LABEL(mpeg_samplerate), message);
    if (info.frames != -1) {
        snprintf(message, 127, _("%d frames"), info.frames);
        gtk_label_set_text(GTK_LABEL(mpeg_frames), message);
    }
    else {
        gtk_label_set_text(GTK_LABEL(mpeg_frames), "");
    }
    gtk_label_set_text(GTK_LABEL(mpeg_flags), mode_str[info.mode]);
    {
        guint sec = mad_timer_count(info.duration, MAD_UNITS_SECONDS);
        snprintf(message, 127, _("%d:%02d (%d seconds)"), sec /60 ,sec % 60, sec);
    }
    gtk_label_set_text(GTK_LABEL(mpeg_duration), message);

    if (info.replaygain_album_str != NULL) {
        snprintf(message, 127, _("RG_album=%4s (x%4.2f)"),
                 info.replaygain_album_str, info.replaygain_album_scale);
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain), "");

    if (info.replaygain_track_str != NULL) {
        snprintf(message, 127, _("RG_track=%4s (x%4.2f)"),
                 info.replaygain_track_str, info.replaygain_track_scale);
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain2), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain2), "");

    if (info.replaygain_album_peak_str != NULL) {
        snprintf(message, 127, _("Peak album=%4s (%+5.3fdBFS)"),
                 info.replaygain_album_peak_str,
                 20 * log10(info.replaygain_album_peak));
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain3), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain3), "");

    if (info.replaygain_track_peak_str != NULL) {
        snprintf(message, 127, _("Peak track=%4s (%+5.3fdBFS)"),
                 info.replaygain_track_peak_str,
                 20 * log10(info.replaygain_track_peak));
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain4), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain3), "");

    if (info.mp3gain_undo_str != NULL) {
        snprintf(message, 127, _("mp3gain undo=%4s (%+5.3fdB)"),
                 info.mp3gain_undo_str, info.mp3gain_undo);
        gtk_label_set_text(GTK_LABEL(mp3gain1), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mp3gain1), "");

    if (info.mp3gain_minmax_str != NULL) {
        snprintf(message, 127, _("mp3gain minmax=%4s (max-min=%+6.3fdB)"),
                 info.mp3gain_minmax_str, info.mp3gain_minmax);
        gtk_label_set_text(GTK_LABEL(mp3gain2), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mp3gain2), "");

    gtk_label_set_text(GTK_LABEL(mpeg_fileinfo), "");


    // work out the index of the genre in the list
    {
        const id3_ucs4_t *string;
        id3_ucs4_t *genre;
        struct id3_frame *frame;
        union id3_field *field;
        frame = id3_tag_findframe(info.tag, ID3_FRAME_GENRE, 0);
        if (frame) {
            field = id3_frame_field(frame, 1);
            string = id3_field_getstrings(field, 0);
            genre = mad_parse_genre(string);
#ifdef DEBUG
            if (genre) {
                gchar *utf = (gchar *)id3_ucs4_utf8duplicate(genre);
                g_print("genre = %s\n", utf);
                g_print("genre num = %d\n", id3_genre_number(genre));
                g_free(utf);
            }
#endif
            if (genre) {
                gtk_list_select_item(GTK_LIST
                                     (GTK_COMBO(genre_combo)->list),
                                     id3_genre_number(genre)+1); //shift one for "Unknown".
                g_free((void *)genre);
            }
        }
    }

    gtk_widget_set_sensitive(id3_frame, TRUE);

#endif                          // !NOGUI
}*/

