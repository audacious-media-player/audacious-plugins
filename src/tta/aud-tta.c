/*
 * aud--tta.c
 *
 * Description:	 TTA input plug-in for Audacious
 * Developed by: Alexander Djourik <sasha@iszf.irk.ru>
 *               Pavel Zhilin <pzh@iszf.irk.ru>
 * Audacious port: Yoshiki Yazawa <yaz@cc.rim.or.jp>
 *
 * Copyright (c) 2004 Alexander Djourik. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <pthread.h>
#include <glib/gi18n.h>
#include <string.h>

#include <audacious/util.h>
#include <audacious/plugin.h>
#include <audacious/titlestring.h>
#include <audacious/vfs.h>
#include "aud-support.h"
#include "audacious/output.h"
#include "audacious/util.h"

#include <id3tag.h>



#define  PLUGIN_VERSION "1.2"
#define  PROJECT_URL "<http://www.true-audio.com>"

#pragma pack (1)
#include "ttalib.h"

#define OUTPUT_ERROR (MEMORY_ERROR+1)
#define MAX_BSIZE (MAX_BPS>>3)


static void init ();
static void cleanup ();
static int  is_our_file (char *filename);
static void play_file (char *filename);
static void tta_pause (short paused);
static void stop (void);
static void seek (int time);
static int  get_time (void);
static void get_song_info (char *filename, char **title, int *length);
static void file_info (char *filename);
static void about ();
static TitleInput *get_song_tuple(char *filename);
static gchar *extname(const char *filename);

InputPlugin tta_ip = 
{
    NULL,
    NULL,
    NULL,
    init,
    about,
    NULL,
    is_our_file,
    NULL,
    play_file,
    stop,
    tta_pause,
    seek,
    NULL,
    get_time,
    NULL,
    NULL,
    cleanup,
    NULL,
    NULL,
    NULL,
    NULL,
    get_song_info,
    file_info,	
    NULL,
    get_song_tuple, // get_song_tuple
    NULL, // set_song_tuple
    NULL, // buffer
    { "tta", NULL },
};

InputPlugin *
get_iplugin_info (void)
{
    tta_ip.description = g_strdup_printf ("True Audio Plugin %s", PLUGIN_VERSION);
    return &tta_ip;
}

static pthread_t decode_thread;
static char sample_buffer[PCM_BUFFER_LENGTH * MAX_BSIZE * MAX_NCH];
static tta_info info;		// currently playing file info
static long seek_position = -1;
static int  playing = FALSE;
static int  read_samples = -1;

static void
tta_error (int error)
{
    char *message;
    static GtkWidget *errorbox;
    if (errorbox != NULL) return;

    switch (error)
    {
      case OPEN_ERROR:
	  message = "Can't open file\n";
	  break;
      case FORMAT_ERROR:
	  message = "Not supported file format\n";
	  break;
      case FILE_ERROR:
	  message = "File is corrupted\n";
	  break;
      case READ_ERROR:
	  message = "Can't read from file\n";
	  break;
      case MEMORY_ERROR:
	  message = "Insufficient memory available\n";
	  break;
      case OUTPUT_ERROR:
	  message = "Output plugin error\n";
	  break;
    default:
	  message = "Unknown error\n";
	  break;
    }

    xmms_show_message ("TTA Decoder Error", message,
	"Ok", FALSE, NULL, NULL);

    gtk_signal_connect(GTK_OBJECT(errorbox), "destroy",
        G_CALLBACK(gtk_widget_destroyed), &errorbox);
}

static gchar *
get_title (char *filename, tta_info *ttainfo)
{
    char *name, *p;

    if (ttainfo->id3v2.id3has &&
	    (*ttainfo->id3v2.artist || *ttainfo->id3v2.title)) {
	    if (*ttainfo->id3v2.artist && *ttainfo->id3v2.title)
		    return g_strdup_printf("%s - %s", ttainfo->id3v2.artist, ttainfo->id3v2.title);
	    else if (*ttainfo->id3v2.artist && *ttainfo->id3v2.album)
		    return g_strdup_printf("%s - %s", ttainfo->id3v2.artist, ttainfo->id3v2.album);
	    else if (*ttainfo->id3v2.artist) return g_strdup(ttainfo->id3v2.artist);
	    else if (*ttainfo->id3v2.title)  return g_strdup(ttainfo->id3v2.title);
    } else if (ttainfo->id3v1.id3has &&
	    (*ttainfo->id3v1.artist || *ttainfo->id3v1.title)) {
	    if (*ttainfo->id3v1.artist && *ttainfo->id3v1.title)
		    return g_strdup_printf("%s - %s", ttainfo->id3v1.artist, ttainfo->id3v1.title);
	    else if (*ttainfo->id3v1.artist && *ttainfo->id3v1.album)
		    return g_strdup_printf("%s - %s", ttainfo->id3v1.artist, ttainfo->id3v1.album);
	    else if (*ttainfo->id3v1.artist) return g_strdup(ttainfo->id3v1.artist);
	    else if (*ttainfo->id3v1.title)  return g_strdup(ttainfo->id3v1.title);
    }
    name = g_strdup (g_basename(filename));
    p = name + strlen(name);
    while (*p != '.' && p >= name) p--;
    if (*p == '.') *p = '\0';
    p = g_strdup (name);
    g_free (name);
    return p;
}

static void *
play_loop (void *arg)
{
    int  bufsize = PCM_BUFFER_LENGTH  * info.BSIZE * info.NCH;

    ////////////////////////////////////////
    // decode PCM_BUFFER_LENGTH samples
    // into the current PCM buffer position

    while (playing)
    {
	while ((read_samples = get_samples (sample_buffer)) > 0)
	{

	    while ((tta_ip.output->buffer_free () < bufsize)
		   && seek_position == -1)
	    {
		if (!playing)
		    goto DONE;
		xmms_usleep (10000);
	    }
	    if (seek_position == -1)
	    {
		produce_audio(tta_ip.output->written_time(),
			      ((info.BPS == 8) ? FMT_U8 : FMT_S16_LE),
			      info.NCH,
			      read_samples * info.NCH * info.BSIZE,
			      sample_buffer,
			      NULL);
	    }
	    else
	    {
		set_position (seek_position);
		tta_ip.output->flush (seek_position * SEEK_STEP);
		seek_position = -1;
	    }
	}
	tta_ip.output->buffer_free ();
	tta_ip.output->buffer_free ();
	xmms_usleep(10000);
    }
  DONE:

    ////////////////////////
    // destroy memory pools
    player_stop ();

    ///////////////////////////////
    // close currently playing file
    close_tta_file (&info);

    pthread_exit (NULL);
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
    if (aboutbox != NULL) return;

    aboutbox = xmms_show_message(
	"About True Audio Plugin",
	"TTA input plugin" PLUGIN_VERSION "for BMP\n"
	"Copyright (c) 2004 True Audio Software\n"
	PROJECT_URL, "Ok", FALSE, NULL, NULL);

    gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
        G_CALLBACK(gtk_widget_destroyed), &aboutbox);
}

static GtkWidget *window = NULL;
static GtkWidget *filename_entry, *title_entry,
		 *artist_entry, *album_entry,
		 *year_entry, *tracknum_entry,
		 *comment_entry, *genre_entry,
		 *info_frame;

extern char *genre[];

static void
file_info (char *filename)
{
    tta_info ttainfo;
    char *title;
    gchar *utf_filename = NULL;

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
	label = gtk_label_new("Filename:");
	gtk_box_pack_start(GTK_BOX(filename_hbox), label, FALSE, TRUE, 0);

	filename_entry = gtk_entry_new_with_max_length(1024);
	gtk_editable_set_editable(GTK_EDITABLE(filename_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(filename_hbox), filename_entry, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	left_vbox = gtk_vbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);

	info_frame = gtk_frame_new("ID3 Tag:");
	gtk_box_pack_start(GTK_BOX(left_vbox), info_frame, FALSE, FALSE, 0);

	table = gtk_table_new(5, 5, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(info_frame), table);

	label = gtk_label_new("Title:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

	title_entry = gtk_entry_new_with_max_length(1024);
	gtk_editable_set_editable(GTK_EDITABLE(title_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), title_entry, 1, 4, 0, 1,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	label = gtk_label_new("Artist:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
	    GTK_FILL, GTK_FILL, 5, 5);

	artist_entry = gtk_entry_new_with_max_length(1024);
	gtk_editable_set_editable(GTK_EDITABLE(artist_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), artist_entry, 1, 4, 1, 2,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	label = gtk_label_new("Album:");
	    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
	    GTK_FILL, GTK_FILL, 5, 5);

	album_entry = gtk_entry_new_with_max_length(1024);
	gtk_editable_set_editable(GTK_EDITABLE(album_entry), FALSE);
	    gtk_table_attach(GTK_TABLE(table), album_entry, 1, 4, 2, 3,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	label = gtk_label_new("Comment:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
	    GTK_FILL, GTK_FILL, 5, 5);

	comment_entry = gtk_entry_new_with_max_length(1024);
	gtk_editable_set_editable(GTK_EDITABLE(comment_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), comment_entry, 1, 4, 3, 4,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	label = gtk_label_new("Year:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
	    GTK_FILL, GTK_FILL, 5, 5);

	year_entry = gtk_entry_new_with_max_length(4);
	    gtk_editable_set_editable(GTK_EDITABLE(year_entry), FALSE);
	    gtk_widget_set_usize(year_entry, 40, -1);
	    gtk_table_attach(GTK_TABLE(table), year_entry, 1, 2, 4, 5,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

 	label = gtk_label_new("Track number:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 4, 5,
	    GTK_FILL, GTK_FILL, 5, 5);

	tracknum_entry = gtk_entry_new_with_max_length(3);
	gtk_editable_set_editable(GTK_EDITABLE(tracknum_entry), FALSE);
	gtk_widget_set_usize(tracknum_entry, 40, -1);
	gtk_table_attach(GTK_TABLE(table), tracknum_entry, 3, 4, 4, 5,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	label = gtk_label_new("Genre:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6,
	    GTK_FILL, GTK_FILL, 5, 5);

	genre_entry = gtk_entry_new_with_max_length(1024);
	gtk_editable_set_editable(GTK_EDITABLE(genre_entry), FALSE);
	gtk_widget_set_usize(genre_entry, 40, -1);
	gtk_table_attach(GTK_TABLE(table), genre_entry, 1, 4, 5, 6,
            GTK_FILL | GTK_EXPAND | GTK_SHRINK,
	    GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

	button_ok = gtk_button_new_with_label("Ok");
	gtk_signal_connect_object(GTK_OBJECT(button_ok), "clicked",
		  G_CALLBACK(gtk_widget_destroy), G_OBJECT(window));
	GTK_WIDGET_SET_FLAGS(button_ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(vbox), button_ok, TRUE, TRUE, 0);

	gtk_widget_show_all (window);
    }
    
    utf_filename = str_to_utf8(filename);
    title = g_strdup_printf(_("File Info - %s"), g_basename(utf_filename));
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);
    
    gtk_entry_set_text(GTK_ENTRY(filename_entry), utf_filename);
    gtk_editable_set_position(GTK_EDITABLE(filename_entry), -1);

#if 1
    title = g_strdup(g_basename(utf_filename));
//    if ((tmp = strrchr(title, '.')) != NULL) *tmp = '\0';
    gtk_entry_set_text(GTK_ENTRY(title_entry), title);
    g_free(title);
#endif
    g_free(utf_filename);

    if (open_tta_file (filename, &ttainfo, 0) >= 0)
    {
	gtk_entry_set_text(GTK_ENTRY(title_entry), "");
	gtk_entry_set_text(GTK_ENTRY(artist_entry), "");
	gtk_entry_set_text(GTK_ENTRY(album_entry), "");
	gtk_entry_set_text(GTK_ENTRY(year_entry), "");
	gtk_entry_set_text(GTK_ENTRY(tracknum_entry), "");
	gtk_entry_set_text(GTK_ENTRY(comment_entry), "");
	gtk_entry_set_text(GTK_ENTRY(genre_entry), "");

	if (ttainfo.id3v2.id3has)
	{
	    gtk_entry_set_text(GTK_ENTRY(title_entry), ttainfo.id3v2.title);
	    gtk_entry_set_text(GTK_ENTRY(artist_entry), ttainfo.id3v2.artist);
	    gtk_entry_set_text(GTK_ENTRY(album_entry), ttainfo.id3v2.album);
	    gtk_entry_set_text(GTK_ENTRY(year_entry), ttainfo.id3v2.year);
	    gtk_entry_set_text(GTK_ENTRY(tracknum_entry), ttainfo.id3v2.track);
	    gtk_entry_set_text(GTK_ENTRY(comment_entry), ttainfo.id3v2.comment);
	    gtk_entry_set_text(GTK_ENTRY(genre_entry), ttainfo.id3v2.genre);
	}
	else if (ttainfo.id3v1.id3has)
	{ 
	    gchar *track = g_strdup_printf ("%2d", ttainfo.id3v1.track);
	    gtk_entry_set_text(GTK_ENTRY(title_entry), ttainfo.id3v1.title);
	    gtk_entry_set_text(GTK_ENTRY(artist_entry), ttainfo.id3v1.artist);
	    gtk_entry_set_text(GTK_ENTRY(album_entry), ttainfo.id3v1.album);
	    gtk_entry_set_text(GTK_ENTRY(year_entry), ttainfo.id3v1.year);
	    gtk_entry_set_text(GTK_ENTRY(tracknum_entry), track);
	    gtk_entry_set_text(GTK_ENTRY(comment_entry), ttainfo.id3v1.comment);
	    gtk_entry_set_text(GTK_ENTRY(genre_entry),
		genre[ttainfo.id3v1.genre <= GENRES ? ttainfo.id3v1.genre : 12]);
	    g_free (track);
	}
    }
    close_tta_file (&ttainfo);

    gtk_widget_set_sensitive(info_frame, TRUE);
}

static int
is_our_file (char *filename)
{
    if (!strcasecmp (filename + strlen (filename) - 4, ".tta"))
    {
	return TRUE;
    }
    return FALSE;
}

static void
play_file (char *filename)
{
    char *title;
    long datasize, origsize, bitrate;

    playing = FALSE;

    ////////////////////////////////////////
    // open TTA file
    if (open_tta_file (filename, &info, 0) < 0)
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


    if (tta_ip.output->open_audio ((info.BPS == 8) ? FMT_U8 : FMT_S16_LE,
				   info.SAMPLERATE, info.NCH) == 0)
    {
	tta_error (OUTPUT_ERROR);
	close_tta_file (&info);
	return;
    }
    title = get_title (filename, &info);
    printf("title @1 = %s\n", title);
    {
	    TitleInput *tuple;

	    tuple = get_song_tuple(filename);
	    if(tuple->track_name) {
		    g_free(title);
		    title = g_strdup(tuple->track_name);
		    printf("title @2 = %s\n", title);
	    }

	    bmp_title_input_free(tuple);
    }

    datasize = file_size(filename) - info.DATAPOS;
    origsize = info.DATALENGTH * info.BSIZE * info.NCH;

    bitrate  = (long) ((float) datasize / origsize *
	        (info.SAMPLERATE * info.NCH * info.BPS));

    tta_ip.set_info (title, 1000 * info.LENGTH, bitrate, info.SAMPLERATE, info.NCH);
    
    if (title)
	g_free (title);

    playing = TRUE;
    seek_position = -1;
    read_samples = -1;

    pthread_create (&decode_thread, NULL, play_loop, NULL);
}

static void
tta_pause (short paused)
{
    tta_ip.output->pause (paused);
}
static void
stop (void)
{
    if (playing)
    {
	playing = FALSE;
	pthread_join (decode_thread, NULL);
	tta_ip.output->close_audio ();
	close_tta_file (&info);
	read_samples = 0;
    }
}

static void
seek (int time)
{
    if (playing)
    {
	seek_position = 1000 * time / SEEK_STEP;

	while (seek_position != -1)
	    xmms_usleep (10000);
    }
}

static int
get_time (void)
{
    if (playing && (read_samples || tta_ip.output->buffer_playing()))
	return tta_ip.output->output_time();

    return -1;
}

static void
get_song_info (char *filename, char **title, int *length)
{
    tta_info ttainfo;

    if (open_tta_file (filename, &ttainfo, 0) >= 0)
    {
	*title = get_title (filename, &ttainfo);
	*length = ttainfo.LENGTH * 1000;
    }
    close_tta_file (&ttainfo);
}



static TitleInput *
get_song_tuple(char *filename)
{
	TitleInput *tuple = NULL;
	tta_info *ttainfo;
	VFSFile *file;

	ttainfo = g_malloc0(sizeof(tta_info));

	if((file = vfs_fopen(filename, "rb")) != NULL) {

#ifdef DEBUG
		printf("about to open_tta_file\n");
#endif
		if(open_tta_file(filename, ttainfo, 0) >= 0) {
			tuple = bmp_title_input_new();
#ifdef DEBUG
			printf("open_tta_file succeed\n");
#endif
			tuple->file_name = g_path_get_basename(filename);
			tuple->file_path = g_path_get_dirname(filename);
			tuple->file_ext = extname(filename);
			tuple->length = ttainfo->LENGTH * 1000;

			if (ttainfo->id3v2.id3has) {
				if(ttainfo->id3v2.artist)
					tuple->performer = g_strdup(ttainfo->id3v2.artist);

				if(ttainfo->id3v2.album)
					tuple->album_name = g_strdup(ttainfo->id3v2.album);

				if(ttainfo->id3v2.title)
					tuple->track_name = g_strdup(ttainfo->id3v2.title);

				tuple->year = atoi(ttainfo->id3v2.year);

				tuple->track_number = atoi(ttainfo->id3v2.track);

				if(ttainfo->id3v2.genre){
//					printf("genre = %s\n",ttainfo->id3v2.genre);
					tuple->genre = g_strdup(ttainfo->id3v2.genre);
				}
				if(ttainfo->id3v2.comment)
					tuple->comment = g_strdup(ttainfo->id3v2.comment);
			} else if (ttainfo->id3v1.id3has) {
				if(ttainfo->id3v1.artist)
					tuple->performer = g_strdup(ttainfo->id3v1.artist);

				if(ttainfo->id3v1.album)
					tuple->album_name = g_strdup(ttainfo->id3v1.album);

				if(ttainfo->id3v1.title)
					tuple->track_name = g_strdup(ttainfo->id3v1.title);

				tuple->year = atoi(ttainfo->id3v1.year);

				tuple->track_number = (int)ttainfo->id3v1.track;

				if(ttainfo->id3v1.genre)
					tuple->genre = g_strdup(genre[ttainfo->id3v1.genre <= GENRES ? ttainfo->id3v1.genre : 12]);
				if(ttainfo->id3v1.comment)
					tuple->comment = g_strdup(ttainfo->id3v1.comment);
			}

			close_tta_file (ttainfo);
		}

		vfs_fclose(file);
	}
	return tuple;
}

static gchar *
extname(const char *filename)
{
    gchar *ext = strrchr(filename, '.');

    if (ext != NULL)
        ++ext;

    return ext;
}

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

#define BYTES(x) ((x) * sizeof(id3_ucs4_t))

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

    if (frame_name == ID3_FRAME_COMMENT)
        field = id3_frame_field(frame, 3);
    else
        field = id3_frame_field(frame, 1);

    if (!field)
        return NULL;

    if (frame_name == ID3_FRAME_COMMENT)
        string_const = id3_field_getfullstring(field);
    else
        string_const = id3_field_getstrings(field, 0);

    if (!string_const)
        return NULL;

    string = tta_ucs4dup((id3_ucs4_t *)string_const);

    if (frame_name == ID3_FRAME_GENRE) {
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
        rtn = id3_ucs4_utf8duplicate(string);
    }
    else {
        rtn = id3_ucs4_latin1duplicate(string);
        rtn2 = str_to_utf8(rtn);
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
	int id3v2_size;
	gchar *str = NULL;

	struct id3_file *id3file = NULL;
	struct id3_tag  *tag = NULL;

	ttainfo->id3v2.id3has = 0;
	ttainfo->id3v1.id3has = 0;

	  id3file = id3_file_open (filename, ID3_FILE_MODE_READONLY);

	  if (id3file) {
		  tag = id3_file_tag (id3file);

		  if (tag) {
			  str = tta_input_id3_get_string (tag, ID3_FRAME_ARTIST);
			  if(str) {
				strcpy(ttainfo->id3v2.artist, str);
				strncpy(ttainfo->id3v1.artist, str, 30);
			  }
			  free(str);
			  str = NULL;

			  str = tta_input_id3_get_string (tag, ID3_FRAME_ALBUM);
			  if(str){
				  strcpy(ttainfo->id3v2.album, str);
				  strncpy(ttainfo->id3v1.album, str, 30);
			  }
			  free(str);
			  str = NULL;

			  str = tta_input_id3_get_string (tag, ID3_FRAME_TITLE);
			  if(str) {
				  strcpy(ttainfo->id3v2.title, str);
				  strncpy(ttainfo->id3v1.title, str, 30);
			  }
			  free(str);
			  str = NULL;

			  // year
			  str = tta_input_id3_get_string (tag, ID3_FRAME_YEAR); //TDRC
			  if(!str) {
				  str = tta_input_id3_get_string (tag, "TYER");
			  }

			  if(str){
				  strncpy(ttainfo->id3v2.year, str, 5);
				  strncpy(ttainfo->id3v1.year, str, 5);
			  }
			  free(str);
			  str = NULL;

			  // track number
			  str = tta_input_id3_get_string (tag, ID3_FRAME_TRACK);
			  if(str)
				  strcpy(ttainfo->id3v2.track, str);
			  free(str);
			  str = NULL;

			  // genre
			  str = tta_input_id3_get_string (tag, ID3_FRAME_GENRE);
			  if(str) {
				  id3_ucs4_t *tmp = NULL;
				  strcpy(ttainfo->id3v2.genre, str);
				  tmp = id3_latin1_ucs4duplicate((id3_latin1_t *)str);
				  ttainfo->id3v1.genre =  id3_genre_number(tmp);
				  g_free(tmp);
			  }
			  free(str);
			  str = NULL;

			  // comment
			  str = tta_input_id3_get_string (tag, ID3_FRAME_COMMENT);
			  if(str) {
				  strcpy(ttainfo->id3v2.comment, str);
				  strncpy(ttainfo->id3v2.comment, str, 30);
			  }
			  free(str);
			  str = NULL;

			  if(*(ttainfo->id3v2.title) && *(ttainfo->id3v2.artist)) {
				  ttainfo->id3v2.id3has = 1;
				  ttainfo->id3v2.id3has = 1;
			  }
		  }
		  id3_file_close(id3file);
	  }
	return id3v2_size; // not used
}
