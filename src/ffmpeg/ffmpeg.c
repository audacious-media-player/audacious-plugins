/*
 *  Audacious WMA input plugin
 *  (C) 2005 Audacious development team
 *
 *  Based on:
 *  xmms-wma - WMA player for BMP
 *  Copyright (C) 2004,2005 McMCC <mcmcc@mail.ru>
 *  bmp-wma - WMA player for BMP
 *  Copyright (C) 2004 Roman Bogorodskiy <bogorodskiy@inbox.ru>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <glib.h>

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/util.h>
#include <audacious/titlestring.h>
#include <audacious/vfs.h>
#include <audacious/strings.h>
#include <audacious/i18n.h>

#include "config.h"

#include "avcodec.h"
#include "avformat.h"
#include "avutil.h"

#define ABOUT_TXT "Adapted for use in audacious by Tony Vroon (chainsaw@gentoo.org) from\n \
the BEEP-WMA plugin which is Copyright (C) 2004,2005 Mokrushin I.V. aka McMCC (mcmcc@mail.ru)\n \
and the BMP-WMA plugin which is Copyright (C) 2004 Roman Bogorodskiy <bogorodskiy@inbox.ru>.\n \
This plugin based on source code " LIBAVCODEC_IDENT "\nby Fabrice Bellard from \
http://ffmpeg.sourceforge.net.\n\n \
This program is free software; you can redistribute it and/or modify \n \
it under the terms of the GNU General Public License as published by \n \
the Free Software Foundation; either version 2 of the License, or \n \
(at your option) any later version. \n\n \
This program is distributed in the hope that it will be useful, \n \
but WITHOUT ANY WARRANTY; without even the implied warranty of \n \
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. \n \
See the GNU General Public License for more details.\n"
#define PLUGIN_NAME "Audacious-WMA"
#define PLUGIN_VERSION "v.1.0.5"
#define ST_BUFF 1024

static int ffmpeg_decode = 0;
static gboolean ffmpeg_pause = 0;
static int ffmpeg_seekpos = -1;
static int ffmpeg_st_buff, ffmpeg_idx, ffmpeg_idx2;
static GThread *ffmpeg_decode_thread;
GStaticMutex ffmpeg_mutex = G_STATIC_MUTEX_INIT;
static AVCodecContext *c = NULL;
static AVFormatContext *ic = NULL;
static AVCodecContext *c2 = NULL;
static AVFormatContext *ic2 = NULL;
static uint8_t *ffmpeg_outbuf, *ffmpeg_s_outbuf;

char description[64];
static void ffmpeg_about(void);
static void ffmpeg_init(void);
static int ffmpeg_is_our_file(char *filename);
static int ffmpeg_is_our_fd(char *filename, VFSFile *fd);
static void ffmpeg_play_file(InputPlayback *data);
static void ffmpeg_stop(InputPlayback *data);
static void ffmpeg_seek(InputPlayback *data, int time);
static void ffmpeg_do_pause(InputPlayback *data, short p);
static int ffmpeg_get_time(InputPlayback *data);
static void ffmpeg_get_song_info(char *filename, char **title, int *length);
static TitleInput *ffmpeg_get_song_tuple(char *filename);
static char *wsong_title;
static int wsong_time;

static GtkWidget *dialog1, *button1, *label1;

InputPlugin *get_iplugin_info(void);

gchar *ffmpeg_fmts[] = { "wma", NULL };

InputPlugin ffmpeg_ip =
{
    NULL,           	// Filled in by xmms
    NULL,           	// Filled in by xmms
    description,    	// The description that is shown in the preferences box
    ffmpeg_init,           // Called when the plugin is loaded
    ffmpeg_about,          // Show the about box
    NULL,  	    	// Show the configure box
    ffmpeg_is_our_file,    // Return 1 if the plugin can handle the file
    NULL,           	// Scan dir
    ffmpeg_play_file,      // Play file
    ffmpeg_stop,           // Stop
    ffmpeg_do_pause,       // Pause
    ffmpeg_seek,           // Seek
    NULL,               // Set the equalizer, most plugins won't be able to do this
    ffmpeg_get_time,       // Get the time, usually returns the output plugins output time
    NULL,           	// Get volume
    NULL,           	// Set volume
    NULL,           	// OBSOLETE!
    NULL,           	// OBSOLETE!
    NULL,           	// Send data to the visualization plugins
    NULL,           	// Fill in the stuff that is shown in the player window
    NULL,           	// Show some text in the song title box. Filled in by xmms
    ffmpeg_get_song_info,  // Function to grab the title string
    NULL,               // Bring up an info window for the filename passed in
    NULL,           	// Handle to the current output plugin. Filled in by xmms
    ffmpeg_get_song_tuple, // Tuple builder
    NULL,
    NULL,
    ffmpeg_is_our_fd,	// vfs
    ffmpeg_fmts
};

InputPlugin *get_iplugin_info(void)
{
    memset(description, 0, 64);
    ffmpeg_ip.description = g_strdup_printf(_("WMA Player %s"), PACKAGE_VERSION);
    return &ffmpeg_ip;
}

static gchar *str_twenty_to_space(gchar * str)
{
    gchar *match, *match_end;

    g_return_val_if_fail(str != NULL, NULL);

    while ((match = strstr(str, "%20"))) {
        match_end = match + 3;
        *match++ = ' ';
        while (*match_end)
            *match++ = *match_end++;
        *match = 0;
    }

    return str;
}

static void ffmpeg_about(void) 
{
    char *title;
    char *message;

    if (dialog1) return;
    
    title = (char *)g_malloc(80);
    message = (char *)g_malloc(1000);
    memset(title, 0, 80);
    memset(message, 0, 1000);

    sprintf(title, _("About %s"), PLUGIN_NAME);
    sprintf(message, "%s %s\n\n%s", PLUGIN_NAME, PLUGIN_VERSION, ABOUT_TXT);

    dialog1 = gtk_dialog_new();
    g_signal_connect(G_OBJECT(dialog1), "destroy",
                        G_CALLBACK(gtk_widget_destroyed), &dialog1);
    gtk_window_set_title(GTK_WINDOW(dialog1), title);
    gtk_window_set_policy(GTK_WINDOW(dialog1), FALSE, FALSE, FALSE);
    gtk_container_border_width(GTK_CONTAINER(dialog1), 5);
    label1 = gtk_label_new(message);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog1)->vbox), label1, TRUE, TRUE, 0);
    gtk_widget_show(label1);

    button1 = gtk_button_new_with_label(_(" Close "));
    g_signal_connect_swapped(G_OBJECT(button1), "clicked",
	                        G_CALLBACK(gtk_widget_destroy),
    	                        GTK_OBJECT(dialog1));
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog1)->action_area), button1,
                     FALSE, FALSE, 0);

    gtk_widget_show(button1);
    gtk_widget_show(dialog1);
    gtk_widget_grab_focus(button1);
    g_free(title);
    g_free(message);
}

static void ffmpeg_init(void)
{
    avcodec_init();
    avcodec_register_all();
    av_register_all();
}

static int ffmpeg_is_our_file(char *filename)
{
    AVCodec *codec2;

    if(av_open_input_file(&ic2, str_twenty_to_space(filename), NULL, 0, NULL) < 0) return 0;

    for(ffmpeg_idx2 = 0; ffmpeg_idx2 < ic2->nb_streams; ffmpeg_idx2++) {
        c2 = ic2->streams[ffmpeg_idx2]->codec;
        if(c2->codec_type == CODEC_TYPE_AUDIO) break;
    }

    av_find_stream_info(ic2);

    codec2 = avcodec_find_decoder(c2->codec_id);

    if(!codec2) {
        av_close_input_file(ic2);
	return 0;
    }
	
    av_close_input_file(ic2);
    return 1;
}

static int ffmpeg_is_our_fd(char *filename, VFSFile *fd)
{
    AVCodec *codec2;

    if(av_open_input_vfsfile(&ic2, filename, fd, NULL, 0, NULL) < 0) return 0;

    for(ffmpeg_idx2 = 0; ffmpeg_idx2 < ic2->nb_streams; ffmpeg_idx2++) {
        c2 = ic2->streams[ffmpeg_idx2]->codec;
        if(c2->codec_type == CODEC_TYPE_AUDIO) break;
    }

    av_find_stream_info(ic2);

    codec2 = avcodec_find_decoder(c2->codec_id);

    return 1;
}

static void ffmpeg_do_pause(InputPlayback *playback, short p)
{
    ffmpeg_pause = p;
    playback->output->pause(ffmpeg_pause);
}

static void ffmpeg_seek(InputPlayback *playback, int time) 
{
    ffmpeg_seekpos = time;
    if(ffmpeg_pause) playback->output->pause(0);
    while(ffmpeg_decode && ffmpeg_seekpos!=-1) xmms_usleep(10000);
    if(ffmpeg_pause) playback->output->pause(1);
}

static int ffmpeg_get_time(InputPlayback *playback)
{
    playback->output->buffer_free();
    if(ffmpeg_decode) return playback->output->output_time();
    return -1;
}

static gchar *extname(const char *filename)
{
    gchar *ext = strrchr(filename, '.');
    if(ext != NULL) ++ext;
    return ext;
}

static char* w_getstr(char* str)
{
    if(str && strlen(str) > 0) return g_strdup(str);
    return NULL;
}

static TitleInput *ffmpeg_get_song_tuple(gchar * filename)
{
    TitleInput *tuple = NULL;
    AVFormatContext *in = NULL;
    gchar *filename_proxy = g_strdup(filename);

    if(av_open_input_file(&ic2, str_twenty_to_space(filename), NULL, 0, NULL) < 0)
	return NULL;

    tuple = bmp_title_input_new();

    tuple->file_name = g_path_get_basename(filename_proxy);
    tuple->file_path = g_path_get_dirname(filename_proxy);
    tuple->file_ext = extname(filename_proxy);
	
    av_find_stream_info(in);

    if((in->title[0] != '\0') || (in->author[0] != '\0') || (in->album[0] != '\0') ||
       (in->comment[0] != '\0') || (in->genre[0] != '\0') || (in->year != 0) || (in->track != 0))
    {	
	tuple->performer = str_to_utf8(w_getstr(in->author));
	tuple->album_name = str_to_utf8(w_getstr(in->album));
	tuple->track_name = str_to_utf8(w_getstr(in->title));
	tuple->year = in->year;
	tuple->track_number = in->track;
	tuple->genre = str_to_utf8(w_getstr(in->genre));
	tuple->comment = str_to_utf8(w_getstr(in->comment));
    }

    if (in->duration)
        tuple->length = in->duration / 1000;

    av_close_input_file(in);

    return tuple;
}

static gchar *get_song_title(AVFormatContext *in, gchar * filename)
{
    gchar *ret = NULL;
    TitleInput *input;

    input = bmp_title_input_new();
    
    if((in->title[0] != '\0') || (in->author[0] != '\0') || (in->album[0] != '\0') ||
       (in->comment[0] != '\0') || (in->genre[0] != '\0') || (in->year != 0) || (in->track != 0))
    {	
	input->performer = w_getstr(in->author);
	input->album_name = w_getstr(in->album);
	input->track_name = w_getstr(in->title);
	input->year = in->year;
	input->track_number = in->track;
	input->genre = w_getstr(in->genre);
	input->comment = w_getstr(in->comment);
    }
    input->file_name = g_path_get_basename(filename);
    input->file_path = g_path_get_dirname(filename);
    input->file_ext = extname(filename);
    ret = xmms_get_titlestring(xmms_get_gentitle_format(), input);
    if(input) g_free(input);

    if(!ret)
    {
	    ret = g_strdup(input->file_name);
            if (extname(ret) != NULL)
                    *(extname(ret) - 1) = '\0';
    }
    return ret;
}

static guint get_song_time(AVFormatContext *in)
{
    if(in->duration)
	return in->duration/1000;
    else
	return 0;
}

static void ffmpeg_get_song_info(char *filename, char **title_real, int *len_real)
{
    TitleInput *tuple = ffmpeg_get_song_tuple(filename);

    if (tuple == NULL)
        return;

    (*len_real) = tuple->length;
    (*title_real) = xmms_get_titlestring(xmms_get_gentitle_format(), tuple);
}

static void ffmpeg_playbuff(InputPlayback *playback, int out_size)
{
    AVFifoBuffer f;
    int sst_buff;
    
    av_fifo_init(&f, out_size*2);
    av_fifo_write(&f, ffmpeg_outbuf, out_size);
    while(!av_fifo_read(&f, ffmpeg_s_outbuf, ffmpeg_st_buff) && ffmpeg_decode)
    {
        sst_buff = ffmpeg_st_buff;
	if(ffmpeg_pause) memset(ffmpeg_s_outbuf, 0, sst_buff);	
    	while(playback->output->buffer_free() < ffmpeg_st_buff) xmms_usleep(20000);
	produce_audio(playback->output->written_time(), FMT_S16_NE,
    			    c->channels, sst_buff, (short *)ffmpeg_s_outbuf, NULL);
	memset(ffmpeg_s_outbuf, 0, sst_buff);
    }
    av_fifo_free(&f);
    return;
}

static void *ffmpeg_play_loop(void *arg)
{
    InputPlayback *playback = arg;
    uint8_t *inbuf_ptr;
    int out_size, size, len;
    AVPacket pkt;
    
    g_static_mutex_lock(&ffmpeg_mutex);
    while(ffmpeg_decode){

	if(ffmpeg_seekpos != -1)
	{
	    av_seek_frame(ic, ffmpeg_idx, ffmpeg_seekpos * 1000000LL, 0);
	    playback->output->flush(ffmpeg_seekpos * 1000);
	    ffmpeg_seekpos = -1;
	}

        if(av_read_frame(ic, &pkt) < 0) break;

        size = pkt.size;
        inbuf_ptr = pkt.data;
	
        if(size == 0) break;
	
        while(size > 0){
            len = avcodec_decode_audio(c, (short *)ffmpeg_outbuf, &out_size,
                                       inbuf_ptr, size);
	    if(len < 0) break;
	    
            if(out_size <= 0) continue;

	    ffmpeg_playbuff(playback, out_size);

            size -= len;
            inbuf_ptr += len;
            if(pkt.data) av_free_packet(&pkt);
        }
    }
    while(ffmpeg_decode && playback->output->buffer_playing()) xmms_usleep(30000);
    ffmpeg_decode = 0;
    if(ffmpeg_s_outbuf) g_free(ffmpeg_s_outbuf);
    if(ffmpeg_outbuf) g_free(ffmpeg_outbuf);
    if(pkt.data) av_free_packet(&pkt);
    if(c) avcodec_close(c);
    if(ic) av_close_input_file(ic);
    g_static_mutex_unlock(&ffmpeg_mutex);
    g_thread_exit(NULL);
    return(NULL);
}

static void ffmpeg_play_file(InputPlayback *playback)
{
    char *filename = playback->filename;
    AVCodec *codec;
    
    if(av_open_input_file(&ic, str_twenty_to_space(filename), NULL, 0, NULL) < 0) return;

    for(ffmpeg_idx = 0; ffmpeg_idx < ic->nb_streams; ffmpeg_idx++) {
        c = ic->streams[ffmpeg_idx]->codec;
        if(c->codec_type == CODEC_TYPE_AUDIO) break;
    }

    av_find_stream_info(ic);

    codec = avcodec_find_decoder(c->codec_id);

    if(!codec) return;

    if(avcodec_open(c, codec) < 0) return;
	    	    
    wsong_title = get_song_title(ic, filename);
    wsong_time = get_song_time(ic);

    if(playback->output->open_audio(FMT_S16_NE, c->sample_rate, c->channels) <= 0) return;

    ffmpeg_st_buff  = ST_BUFF;
	
    ffmpeg_ip.set_info(wsong_title, wsong_time, c->bit_rate, c->sample_rate, c->channels);

    /* av_malloc() will wrap posix_memalign() if necessary -nenolod */
    ffmpeg_s_outbuf = av_malloc(ffmpeg_st_buff);
    ffmpeg_outbuf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

    ffmpeg_seekpos = -1;
    ffmpeg_decode = 1;
    ffmpeg_decode_thread = g_thread_create((GThreadFunc)ffmpeg_play_loop, playback, TRUE, NULL);
}

static void ffmpeg_stop(InputPlayback *playback) 
{
    ffmpeg_decode = 0;
    if(ffmpeg_pause) ffmpeg_do_pause(playback, 0);
    g_thread_join(ffmpeg_decode_thread);
    playback->output->close_audio();
}	
