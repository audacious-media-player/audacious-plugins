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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Please see the file COPYING in this directory for full copyright
 * information.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <string.h>

#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/main.h>
#include <audacious/output.h>
#include <audacious/strings.h>
#include <audacious/i18n.h>
#include <audacious/id3tag.h>

#define  PLUGIN_VERSION "1.4"
#define  PROJECT_URL "<http://www.true-audio.com>"

#include "ttalib.h"

#define OUTPUT_ERROR (MEMORY_ERROR+1)
#define MAX_BSIZE (MAX_BPS>>3)
#define BYTES(x) ((x) * sizeof(id3_ucs4_t))

static void init ();
static void cleanup ();
static int  is_our_file (char *filename);
static void play_file (InputPlayback *playback);
static void tta_pause (InputPlayback *playback, short paused);
static void stop (InputPlayback *playback);
static void mseek (InputPlayback *playback, gulong millisec);
static void seek (InputPlayback *playback, int sec);
static void get_song_info (char *filename, char **title, int *length);
static void file_info (char *filename);
static void about ();
static Tuple *get_song_tuple(char *filename);
//static gchar *extname(const char *filename);

static GThread *decode_thread = NULL;
static char sample_buffer[PCM_BUFFER_LENGTH * MAX_BSIZE * MAX_NCH];
static tta_info info;		// currently playing file info
static int seek_position = -1;
static int read_samples = -1;

gchar *tta_fmts[] = { "tta", NULL };

InputPlugin tta_ip =
{
	.description = "True Audio Plugin",
	.init = init,
	.about = about,
	.is_our_file = is_our_file,
	.play_file = play_file,
	.stop = stop,
	.pause = tta_pause,
	.seek = seek,
	.cleanup = cleanup,
	.get_song_info = get_song_info,
	.file_info_box = file_info,
	.get_song_tuple = get_song_tuple,
	.vfs_extensions = tta_fmts,
	.mseek = mseek,
};

InputPlugin *tta_iplist[] = { &tta_ip, NULL };

DECLARE_PLUGIN(tta, NULL, NULL, tta_iplist, NULL, NULL, NULL, NULL, NULL);

size_t
file_size (char *filename)
{
	VFSFile *f;
	size_t size = -1;

	if ((f = aud_vfs_fopen (filename, "r")))
	{
	    aud_vfs_fseek (f, 0, SEEK_END);
	    size = aud_vfs_ftell (f);
	    aud_vfs_fclose (f);
	}
	return size;
}

static void
tta_error (int error)
{
	char *message;
	static GtkWidget *errorbox;
	if (errorbox != NULL) return;

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

	audacious_info_dialog (_("TTA Decoder Error"), message,
	    _("Ok"), FALSE, NULL, NULL);

	gtk_signal_connect(GTK_OBJECT(errorbox), "destroy",
    	    G_CALLBACK(gtk_widget_destroyed), &errorbox);
}

static gchar *
get_song_title(Tuple *tuple)
{
	return aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
}

static void
get_song_info (char *filename, char **title, int *length)
{
	Tuple *tuple;

	*length = -1;
	*title = NULL;

	if ((tuple = get_song_tuple(filename)) != NULL) {
    	    *length = aud_tuple_get_int(tuple, FIELD_LENGTH, NULL);
    	    *title = get_song_title(tuple);
	}

	aud_tuple_free(tuple);
}

static void *
play_loop (InputPlayback *playback)
{
	int  bufsize = PCM_BUFFER_LENGTH  * info.BSIZE * info.NCH;

	////////////////////////////////////////
	// decode PCM_BUFFER_LENGTH samples
	// into the current PCM buffer position

	while (playback->playing)
	{
	    while ((read_samples = get_samples ((unsigned char *)sample_buffer)) > 0)
	    {

            while ((playback->output->buffer_free () < bufsize)
                   && seek_position == -1)
            {
                if (!playback->playing)
                    goto DONE;
                g_usleep (10000);
            }
            if (seek_position == -1)
            {
                playback->pass_audio(playback,
                              ((info.BPS == 8) ? FMT_U8 : FMT_S16_LE),
                              info.NCH,
                              read_samples * info.NCH * info.BSIZE,
                              sample_buffer,
                              &playback->playing);
            }
            else
            {
                set_position (seek_position);
                playback->output->flush (seek_position * SEEK_STEP);
                seek_position = -1;
            }
            if(!playback->playing)
                goto DONE;
	    }

	    playback->output->buffer_free ();
	    playback->output->buffer_free ();
	    while (playback->output->buffer_playing()) {
		    g_usleep(10000);
		    if(!playback->playing)
			    goto DONE;
	    }
	}

DONE:
	////////////////////////
	// destroy memory pools
	player_stop ();

	///////////////////////////////
	// close currently playing file
	close_tta_file (&info);

	return NULL;
}

static void
init ()
{
	memset (&info, 0, sizeof (tta_info));
}

static void
cleanup ()
{
}

static void
about ()
{
	static GtkWidget *aboutbox;
        gchar *about_text;

	if (aboutbox != NULL) return;

	about_text = g_strjoin("", _("TTA input plugin "), PLUGIN_VERSION,
				   _(" for BMP\n"
		        	   "Copyright (c) 2004 True Audio Software\n"), PROJECT_URL, NULL);

	aboutbox = audacious_info_dialog(_("About True Audio Plugin"),
				     about_text,
				     _("Ok"), FALSE, NULL, NULL);

	gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
			   G_CALLBACK(gtk_widget_destroyed), &aboutbox);
	g_free(about_text);
}

static GtkWidget *window = NULL;
static GtkWidget *filename_entry, *title_entry,
		 *artist_entry, *album_entry,
		 *year_entry, *tracknum_entry,
		 *comment_entry, *genre_entry,
		 *info_frame;

static void
file_info (char *filename)
{
	tta_info ttainfo;
	char *title;
	gchar *utf_filename = NULL;
	gchar *realfn = NULL;

	if (!window) {
	    GtkWidget *vbox, *hbox, *left_vbox, *table;
	    GtkWidget *label, *filename_hbox, *button_ok;

	    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	    gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	    gtk_signal_connect(GTK_OBJECT(window), "destroy",
		G_CALLBACK(gtk_widget_destroyed), &window);
	    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	    vbox = gtk_vbox_new(FALSE, 10);
	    gtk_container_add(GTK_CONTAINER(window), vbox);

	    filename_hbox = gtk_hbox_new(FALSE, 5);
	    gtk_box_pack_start(GTK_BOX(vbox), filename_hbox, FALSE, TRUE, 0);
	    label = gtk_label_new(_("Filename:"));
	    gtk_box_pack_start(GTK_BOX(filename_hbox), label, FALSE, TRUE, 0);

	    filename_entry = gtk_entry_new_with_max_length(1024);
	    gtk_editable_set_editable(GTK_EDITABLE(filename_entry), FALSE);
	    gtk_box_pack_start(GTK_BOX(filename_hbox), filename_entry, TRUE, TRUE, 0);

	    hbox = gtk_hbox_new(FALSE, 10);
	    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	    left_vbox = gtk_vbox_new(FALSE, 10);
	    gtk_box_pack_start(GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);

	    info_frame = gtk_frame_new(_("ID3 Tag:"));
	    gtk_box_pack_start(GTK_BOX(left_vbox), info_frame, FALSE, FALSE, 0);

	    table = gtk_table_new(5, 5, FALSE);
	    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	    gtk_container_add(GTK_CONTAINER(info_frame), table);

	    label = gtk_label_new(_("Title:"));
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

	    title_entry = gtk_entry_new_with_max_length(1024);
	    gtk_editable_set_editable(GTK_EDITABLE(title_entry), FALSE);
	    gtk_table_attach(GTK_TABLE(table), title_entry, 1, 4, 0, 1,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	    label = gtk_label_new(_("Artist:"));
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
		GTK_FILL, GTK_FILL, 5, 5);

	    artist_entry = gtk_entry_new_with_max_length(1024);
	    gtk_editable_set_editable(GTK_EDITABLE(artist_entry), FALSE);
	    gtk_table_attach(GTK_TABLE(table), artist_entry, 1, 4, 1, 2,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	    label = gtk_label_new(_("Album:"));
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
		GTK_FILL, GTK_FILL, 5, 5);

	    album_entry = gtk_entry_new_with_max_length(1024);
	    gtk_editable_set_editable(GTK_EDITABLE(album_entry), FALSE);
	    gtk_table_attach(GTK_TABLE(table), album_entry, 1, 4, 2, 3,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	    label = gtk_label_new(_("Comment:"));
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
		GTK_FILL, GTK_FILL, 5, 5);

	    comment_entry = gtk_entry_new_with_max_length(1024);
	    gtk_editable_set_editable(GTK_EDITABLE(comment_entry), FALSE);
	    gtk_table_attach(GTK_TABLE(table), comment_entry, 1, 4, 3, 4,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	    label = gtk_label_new(_("Year:"));
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
		GTK_FILL, GTK_FILL, 5, 5);

	    year_entry = gtk_entry_new_with_max_length(4);
	    gtk_editable_set_editable(GTK_EDITABLE(year_entry), FALSE);
	    gtk_widget_set_usize(year_entry, 40, -1);
	    gtk_table_attach(GTK_TABLE(table), year_entry, 1, 2, 4, 5,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

 	    label = gtk_label_new(_("Track number:"));
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 4, 5,
		GTK_FILL, GTK_FILL, 5, 5);

	    tracknum_entry = gtk_entry_new_with_max_length(3);
	    gtk_editable_set_editable(GTK_EDITABLE(tracknum_entry), FALSE);
	    gtk_widget_set_usize(tracknum_entry, 40, -1);
	    gtk_table_attach(GTK_TABLE(table), tracknum_entry, 3, 4, 4, 5,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	    label = gtk_label_new(_("Genre:"));
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6,
		GTK_FILL, GTK_FILL, 5, 5);

	    genre_entry = gtk_entry_new_with_max_length(1024);
	    gtk_editable_set_editable(GTK_EDITABLE(genre_entry), FALSE);
	    gtk_widget_set_usize(genre_entry, 40, -1);
	    gtk_table_attach(GTK_TABLE(table), genre_entry, 1, 4, 5, 6,
        	GTK_FILL | GTK_EXPAND | GTK_SHRINK,
		GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	    button_ok = gtk_button_new_with_label(_("Ok"));
	    gtk_signal_connect_object(GTK_OBJECT(button_ok), "clicked",
		G_CALLBACK(gtk_widget_destroy), G_OBJECT(window));
	    GTK_WIDGET_SET_FLAGS(button_ok, GTK_CAN_DEFAULT);
	    gtk_box_pack_start(GTK_BOX(vbox), button_ok, TRUE, TRUE, 0);

	    gtk_widget_show_all (window);
	}
	realfn = g_filename_from_uri(filename, NULL, NULL);
	utf_filename = aud_str_to_utf8(realfn ? realfn : filename);
	g_free(realfn); realfn = NULL;
	title = g_strdup_printf(_("File Info - %s"), g_basename(utf_filename));
	gtk_window_set_title(GTK_WINDOW(window), title);
	g_free(title);

	gtk_entry_set_text(GTK_ENTRY(filename_entry), utf_filename);
	gtk_editable_set_position(GTK_EDITABLE(filename_entry), -1);

	title = g_strdup(g_basename(utf_filename));
	gtk_entry_set_text(GTK_ENTRY(title_entry), title);

	g_free(title);
	g_free(utf_filename);

	if (open_tta_file (filename, &ttainfo, 0) >= 0)
	{
	    gtk_entry_set_text(GTK_ENTRY(title_entry), (gchar *)ttainfo.ID3.title);
	    gtk_entry_set_text(GTK_ENTRY(artist_entry), (gchar *)ttainfo.ID3.artist);
	    gtk_entry_set_text(GTK_ENTRY(album_entry), (gchar *)ttainfo.ID3.album);
	    gtk_entry_set_text(GTK_ENTRY(year_entry), (gchar *)ttainfo.ID3.year);
	    gtk_entry_set_text(GTK_ENTRY(tracknum_entry), (gchar *)ttainfo.ID3.track);
	    gtk_entry_set_text(GTK_ENTRY(comment_entry), (gchar *)ttainfo.ID3.comment);
	    gtk_entry_set_text(GTK_ENTRY(genre_entry), (gchar *)ttainfo.ID3.genre);
	}
	close_tta_file (&ttainfo);

	gtk_widget_set_sensitive(info_frame, TRUE);
}

static int
is_our_file (char *filename)
{
	gchar *ext = strrchr(filename, '.');

	if (ext && !strncasecmp(ext, ".tta", 4))
	    return TRUE;

	return FALSE;
}

static void
play_file (InputPlayback *playback)
{
	gchar *filename = playback->filename;
	char *title = NULL;
	int datasize, origsize, bitrate;
	Tuple *tuple = NULL;

	////////////////////////////////////////
	// open TTA file
	if (open_tta_file (filename, &info, 0) > 0)
	{
	    tta_error (info.STATE);
	    close_tta_file (&info);
	    return;
	}

	////////////////////////////////////////
	// initialize TTA player
	if (player_init (&info) < 0)
	{
	    tta_error (info.STATE);
	    close_tta_file (&info);
	    return;
	}

	if (playback->output->open_audio ((info.BPS == 8) ? FMT_U8 : FMT_S16_LE,
	    info.SAMPLERATE, info.NCH) == 0)
	{
	    tta_error (OUTPUT_ERROR);
	    close_tta_file (&info);
	    return;
	}

	tuple = get_song_tuple(filename);
	title = get_song_title(tuple);
	aud_tuple_free(tuple);

	datasize = file_size(filename) - info.DATAPOS;
	origsize = info.DATALENGTH * info.BSIZE * info.NCH;

	bitrate  = (int) ((float) datasize / origsize *
	        (info.SAMPLERATE * info.NCH * info.BPS));

	playback->set_params(playback, title, 1000 * info.LENGTH, bitrate, info.SAMPLERATE, info.NCH);

	if (title)
	    g_free (title);

	playback->playing = 1;
	seek_position = -1;
	read_samples = -1;

	decode_thread = g_thread_self();
	playback->set_pb_ready(playback);
	play_loop(playback);
}

static void
tta_pause (InputPlayback *playback, short paused)
{
	playback->output->pause (paused);
}

static void
stop (InputPlayback *playback)
{
	if (playback->playing)
	{
	    playback->playing = 0;
	    g_thread_join(decode_thread);
	    decode_thread = NULL;
	    playback->output->close_audio ();
	    close_tta_file (&info);
	    read_samples = -1;
	}
}

static void
mseek (InputPlayback *data, gulong millisec)
{
	if (data->playing)
	{
	    seek_position = (int)(millisec / SEEK_STEP);

	    while (seek_position != -1)
		g_usleep (10000);
	}
}

static void
seek (InputPlayback *data, int sec)
{
    gulong millisec = 1000 * sec;
    mseek(data, millisec);
}

static Tuple *
get_song_tuple(char *filename)
{
	Tuple *tuple = NULL;
	tta_info *ttainfo;
	VFSFile *file;

	ttainfo = g_malloc0(sizeof(tta_info));

	if((file = aud_vfs_fopen(filename, "rb")) != NULL) {
		if(open_tta_file(filename, ttainfo, 0) >= 0) {
			tuple = aud_tuple_new_from_filename(filename);

			aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, "True Audio (TTA)");
			aud_tuple_associate_string(tuple, FIELD_QUALITY, NULL, "lossless");

			if (ttainfo->ID3.id3has) {
				if (ttainfo->ID3.artist)
					aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, (gchar *) ttainfo->ID3.artist);

				if (ttainfo->ID3.album)
					aud_tuple_associate_string(tuple, FIELD_ALBUM, NULL, (gchar *) ttainfo->ID3.album);

				if (ttainfo->ID3.title)
					aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, (gchar *) ttainfo->ID3.title);

				if (ttainfo->ID3.year)
					aud_tuple_associate_int(tuple, FIELD_YEAR, NULL, atoi((char *)ttainfo->ID3.year));

				if(ttainfo->ID3.track)
					aud_tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi((char *)ttainfo->ID3.track));

				if(ttainfo->ID3.genre)
					aud_tuple_associate_string(tuple, FIELD_GENRE, NULL, (gchar *) ttainfo->ID3.genre);

				if(ttainfo->ID3.comment)
					aud_tuple_associate_string(tuple, FIELD_COMMENT, NULL, (gchar *) ttainfo->ID3.comment);
				if(ttainfo->LENGTH)
					aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, 1000 * ttainfo->LENGTH);

			}
			close_tta_file (ttainfo);
		}
		aud_vfs_fclose(file);
	}
	return tuple;
}

#if 0
static gchar *
extname(const char *filename)
{
	gchar *ext = strrchr(filename, '.');

	if (ext != NULL)
    	    ++ext;

	return ext;
}
#endif

/* return length in letters */
size_t tta_ucs4len(id3_ucs4_t *ucs)
{
	id3_ucs4_t *ptr = ucs;
	size_t len = 0;

	while(*ptr++ != 0)
    	    len++;

	return len;
}

/* duplicate id3_ucs4_t string. new string will be terminated with 0. */
id3_ucs4_t *tta_ucs4dup(id3_ucs4_t *org)
{
	id3_ucs4_t *new = NULL;
	size_t len = tta_ucs4len(org);

	new = g_malloc0((len + 1) * sizeof(id3_ucs4_t));
	memcpy(new, org, len * sizeof(id3_ucs4_t));
	*(new + len) = 0; //terminate

	return new;
}

id3_ucs4_t *tta_parse_genre(const id3_ucs4_t *string)
{
    id3_ucs4_t *ret = NULL;
    id3_ucs4_t *tmp = NULL;
    id3_ucs4_t *genre = NULL;
    id3_ucs4_t *ptr, *end, *tail, *tp;
    size_t ret_len = 0; //num of ucs4 char!
    size_t tmp_len = 0;
    gboolean is_num = TRUE;

    tail = (id3_ucs4_t *)string + tta_ucs4len((id3_ucs4_t *)string);

    ret = g_malloc0(1024);

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

                tmp_len = tta_ucs4len(genre);

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
                printf("genre length = %d\n", tta_ucs4len(genre));
#endif
                g_free(tmp);
                tmp = NULL;

                tmp_len = tta_ucs4len(genre);

                memcpy(ret + BYTES(ret_len), genre, BYTES(tmp_len));

                ret_len += tmp_len;
                *(ret + ret_len) = 0; //terminate
            }
            else { // plain text
#ifdef DEBUG
                printf("plain!\n");
                printf("ret_len = %d\n", ret_len);
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

gchar *tta_input_id3_get_string(struct id3_tag * tag, char *frame_name)
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
    if (!frame)
        return NULL;

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

    string = tta_ucs4dup((id3_ucs4_t *)string_const);

    if (!strcmp(frame_name, ID3_FRAME_GENRE)) {
        id3_ucs4_t *string2 = NULL;
        string2 = tta_parse_genre(string);
        g_free((void *)string);
        string = string2;
    }

    ucsptr = (id3_ucs4_t *)string;
    while (*ucsptr) {
        if (*ucsptr > 0x000000ffL) {
            flagutf = TRUE;
            break;
        }
        ucsptr++;
    }

    if (flagutf) {
#ifdef DEBUG
        g_message("aud-tta: flagutf!\n");
#endif
        rtn = (gchar *)id3_ucs4_utf8duplicate(string);
    }
    else {
        rtn = (gchar *)id3_ucs4_latin1duplicate(string);
        rtn2 = aud_str_to_utf8(rtn);
        free(rtn);
        rtn = rtn2;
    }
    g_free(string);
    string = NULL;
#ifdef DEBUG
    g_print("string = %s\n", rtn);
#endif

    return rtn;
}

int get_id3_tags (const char *filename, tta_info *ttainfo) {
	int id3v2_size = 0;
	gchar *str = NULL;

	struct id3_file *id3file = NULL;
	struct id3_tag  *tag = NULL;

	id3file = id3_file_open (filename, ID3_FILE_MODE_READONLY);

	if (id3file) {
		tag = id3_file_tag (id3file);

		if (tag) {
			ttainfo->ID3.id3has = 1;
			id3v2_size = tag->paddedsize;

			str = tta_input_id3_get_string (tag, ID3_FRAME_ARTIST);
			if(str) strncpy((char *)ttainfo->ID3.artist, str, MAX_LINE);
			free(str);
			str = NULL;

			str = tta_input_id3_get_string (tag, ID3_FRAME_ALBUM);
			if(str) strncpy((char *)ttainfo->ID3.album, str, MAX_LINE);
			free(str);
			str = NULL;

			str = tta_input_id3_get_string (tag, ID3_FRAME_TITLE);
			if(str) strncpy((char *)ttainfo->ID3.title, str, MAX_LINE);
			free(str);
			str = NULL;

			str = tta_input_id3_get_string (tag, ID3_FRAME_YEAR);
			if(!str) str = tta_input_id3_get_string (tag, "TYER");
			if(str) strncpy((char *)ttainfo->ID3.year, str, MAX_YEAR);
			free(str);
			str = NULL;

			str = tta_input_id3_get_string (tag, ID3_FRAME_TRACK);
			if(str) strncpy((char *)ttainfo->ID3.track, str, MAX_TRACK);
			free(str);
			str = NULL;

			str = tta_input_id3_get_string (tag, ID3_FRAME_GENRE);
			if(str) strncpy((char *)ttainfo->ID3.genre, str, MAX_GENRE);
			free(str);
			str = NULL;

			str = tta_input_id3_get_string (tag, ID3_FRAME_COMMENT);
			if(str) strncpy((char *)ttainfo->ID3.comment, str, MAX_LINE);
			free(str);
			str = NULL;
		}

		id3_file_close(id3file);
	}

	return id3v2_size; // not used
}
