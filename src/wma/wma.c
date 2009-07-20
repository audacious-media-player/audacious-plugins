/*
 *  Audacious WMA input plugin
 *  (C) 2005, 2006, 2007 Audacious development team
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

#include "config.h"

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
#include <audacious/i18n.h>


#include <libaudtag/audtag.h>


#include "avcodec.h"
#include "avformat.h"

static const gchar * PLUGIN_NAME = "Audacious-WMA";
static const gchar * PLUGIN_VERSION = "v.1.0.5";
static const int ST_BUFF = 1024;

static int wma_decode = 0;
static gboolean wma_pause = 0;
static int wma_seekpos = -1;
static GThread *wma_decode_thread;

char description[64];
static void wma_about(void);
static void wma_init(void);
static gint wma_is_our_fd(const gchar *filename, VFSFile *fd);
static void wma_play_file(InputPlayback *data);
static void wma_stop(InputPlayback *data);
static void wma_seek(InputPlayback *data, gint time);
static void wma_do_pause(InputPlayback *data, gshort p);
static void wma_get_song_info(gchar *filename, gchar **title, gint *length);
static Tuple *wma_get_song_tuple(const gchar *filename);
static char *wsong_title;
static int wsong_time;

static GtkWidget *dialog1, *button1, *label1;
static gchar *fmts[] = { "wma", NULL };

InputPlugin wma_ip =
{
    .description = "Windows Media Audio (WMA) Plugin",
    .init = wma_init,
    .about = wma_about,
    .play_file = wma_play_file,
    .stop = wma_stop,
    .pause = wma_do_pause,
    .seek = wma_seek,
    .get_song_info = wma_get_song_info,
    .get_song_tuple = wma_get_song_tuple,
    .is_our_file_from_vfs = wma_is_our_fd,
    .vfs_extensions = fmts,
};

InputPlugin *wma_iplist[] = { &wma_ip, NULL };

DECLARE_PLUGIN(wma, NULL, NULL, wma_iplist, NULL, NULL, NULL, NULL, NULL);

static gchar *str_twenty_to_space(const gchar * str)
{
    gchar *res, *match, *match_end;

    g_return_val_if_fail(str != NULL, NULL);
    res = g_strdup(str);

    while ((match = strstr(res, "%20")) != NULL) {
        match_end = match + 3;
        *match++ = ' ';
        while (*match_end)
            *match++ = *match_end++;
        *match = 0;
    }

    return res;
}

static void wma_about(void)
{
    gchar *title;
    gchar *message;

    if (dialog1) return;

    title = (char *)g_malloc(80);
    message = (char *)g_malloc(1000);
    memset(title, 0, 80);
    memset(message, 0, 1000);

    sprintf(title, _("About %s"), PLUGIN_NAME);
    sprintf(message, "%s %s\n\n%s", PLUGIN_NAME, PLUGIN_VERSION,
            _("Adapted for use in Audacious by Tony Vroon (chainsaw@gentoo.org) from\n"
              "the BEEP-WMA plugin which is Copyright (C) 2004,2005 Mokrushin I.V. aka McMCC (mcmcc@mail.ru)\n"
              "and the BMP-WMA plugin which is Copyright (C) 2004 Roman Bogorodskiy <bogorodskiy@inbox.ru>.\n"
              "This plugin based on source code " LIBAVCODEC_IDENT "\nby Fabrice Bellard from"
              "http://ffmpeg.sourceforge.net.\n\n"
              "This program is free software; you can redistribute it and/or modify \n"
              "it under the terms of the GNU General Public License as published by \n"
              "the Free Software Foundation; either version 2 of the License, or \n"
              "(at your option) any later version. \n\n"
              "This program is distributed in the hope that it will be useful, \n"
              "but WITHOUT ANY WARRANTY; without even the implied warranty of \n"
              "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. \n"
              "See the GNU General Public License for more details.\n"
             ));

    dialog1 = gtk_dialog_new();
    g_signal_connect(G_OBJECT(dialog1), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &dialog1);
    gtk_window_set_title(GTK_WINDOW(dialog1), title);
    gtk_window_set_resizable(GTK_WINDOW(dialog1), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dialog1), 5);
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

static void wma_init(void)
{
    avcodec_init();
    avcodec_register_all();
    av_register_all();
    tag_init();
}

static gint wma_is_our_fd(const gchar *filename, VFSFile *fd)
{
    AVCodec *codec2 = NULL;
    AVCodecContext *c2 = NULL;
    AVFormatContext *ic2 = NULL;
    int wma_idx2;

    if(av_open_input_vfsfile(&ic2, filename, fd, NULL, 0, NULL) < 0) return 0;

    for(wma_idx2 = 0; wma_idx2 < ic2->nb_streams; wma_idx2++) {
        c2 = &ic2->streams[wma_idx2]->codec;
        if(c2->codec_type == CODEC_TYPE_AUDIO)
        {
	    av_find_stream_info(ic2);
            codec2 = avcodec_find_decoder(c2->codec_id);
            if (codec2) break;
	}
    }

    if (!codec2) return 0;

    av_find_stream_info(ic2);

    codec2 = avcodec_find_decoder(c2->codec_id);

    if (!codec2) {
        av_close_input_vfsfile(ic2);
        return 0;
    }

    av_close_input_vfsfile(ic2);

    return 1;
}

static void wma_do_pause(InputPlayback *playback, gshort p)
{
    wma_pause = p;
}

static void wma_seek(InputPlayback *playback, gint time)
{
    wma_seekpos = time;
    while(wma_decode && wma_seekpos!=-1) g_usleep(10000);
}

static Tuple *wma_get_song_tuple(const gchar * filename)
{
    gchar *c = str_twenty_to_space(filename);
    Tuple *ti = aud_tuple_new_from_filename(c);
    g_free(c);

    ti = tag_tuple_read(ti);

    return ti;
}

static gchar *get_song_title(AVFormatContext *in, gchar * filename)
{
    gchar *ret = NULL;
    Tuple *ti = aud_tuple_new_from_filename(filename);
    
    ti = tag_tuple_read(ti);

    aud_tuple_associate_string(ti, FIELD_CODEC, NULL, "Windows Media Audio (WMA)");
    aud_tuple_associate_string(ti, FIELD_QUALITY, NULL, "lossy");

    ret = aud_tuple_formatter_make_title_string(ti, aud_get_gentitle_format());

    return ret;
}

static guint get_song_time(AVFormatContext *in)
{
    if(in->duration)
        return in->duration/1000;
    else
        return 0;
}

static void wma_get_song_info(gchar *filename, gchar **title_real, gint *len_real)
{
    Tuple *tuple = wma_get_song_tuple(filename);

    if (tuple == NULL)
        return;

    (*len_real) = aud_tuple_get_int(tuple, FIELD_LENGTH, NULL);
    (*title_real) = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
}

static void do_seek (InputPlayback * playback, AVFormatContext * context, int index) {
   playback->output->flush (wma_seekpos * 1000);
   av_seek_frame (context, index, (long long) wma_seekpos * 1000000);
   wma_seekpos = -1;
}

static void do_pause (InputPlayback * playback, AVFormatContext * context, int index) {
   playback->output->pause (1);
   while (wma_pause) {
      if (wma_seekpos != -1)
         do_seek (playback, context, index);
      g_usleep(50000);
   }
   playback->output->pause (0);
}

static void wma_play_file(InputPlayback *playback)
{
    AVCodec *codec = NULL;
    AVCodecContext *c = NULL;
    AVFormatContext *ic = NULL;
    uint8_t *inbuf_ptr;
    int out_size, size, len;
    AVPacket pkt;
    guint8 *wma_outbuf, *wma_s_outbuf;
    int wma_st_buff, wma_idx;

    if(av_open_input_file(&ic, playback->filename, NULL, 0, NULL) < 0) return;

    for(wma_idx = 0; wma_idx < ic->nb_streams; wma_idx++) {
        c = &ic->streams[wma_idx]->codec;
        if(c->codec_type == CODEC_TYPE_AUDIO)
        {
	    av_find_stream_info(ic);
            codec = avcodec_find_decoder(c->codec_id);
            if (codec) break;
	}
    }

    if (!codec) return;

    if(avcodec_open(c, codec) < 0) return;

    wsong_title = get_song_title(ic, playback->filename);
    wsong_time = get_song_time(ic);

    if(playback->output->open_audio(FMT_S16_NE, c->sample_rate, c->channels) <= 0) return;

    wma_st_buff  = ST_BUFF;

    playback->set_params(playback, wsong_title, wsong_time, c->bit_rate, c->sample_rate, c->channels);

    /* av_malloc() will wrap posix_memalign() if necessary -nenolod */
    wma_s_outbuf = av_malloc(wma_st_buff);
    wma_outbuf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

    wma_seekpos = -1;
    wma_decode = 1;
    playback->playing = 1;
    wma_decode_thread = g_thread_self();
    playback->set_pb_ready(playback);

    while(playback->playing)
    {
        if(wma_seekpos != -1)
           do_seek (playback, ic, wma_idx);
        if (wma_pause)
           do_pause (playback, ic, wma_idx);

        if(av_read_frame(ic, &pkt) < 0) break;

        size = pkt.size;
        inbuf_ptr = pkt.data;

        if(size == 0) break;

        while(size > 0){
            FifoBuffer f;
            int sst_buff;

            len = avcodec_decode_audio(c, (short *)wma_outbuf, &out_size,
                                       inbuf_ptr, size);
            if(len < 0) break;

            if(out_size <= 0) continue;

            fifo_init(&f, out_size*2);
            fifo_write(&f, wma_outbuf, out_size, &f.wptr);

            while(!fifo_read(&f, wma_s_outbuf, wma_st_buff, &f.rptr) && wma_decode)
            {
                 sst_buff = wma_st_buff;
                 playback->pass_audio(playback, FMT_S16_NE,
                                      c->channels, sst_buff, (short *)wma_s_outbuf, NULL);
                 memset(wma_s_outbuf, 0, sst_buff);
            }

            fifo_free(&f);

            size -= len;
            inbuf_ptr += len;
            if(pkt.data) av_free_packet(&pkt);
        }
    }
    while(playback->playing && playback->output->buffer_playing()) g_usleep(30000);
    playback->playing = 0;
    if(wma_s_outbuf) g_free(wma_s_outbuf);
    if(wma_outbuf) g_free(wma_outbuf);
    if(pkt.data) av_free_packet(&pkt);
    if(c) avcodec_close(c);
    if(ic) av_close_input_file(ic);
}

static void wma_stop(InputPlayback *playback)
{
    wma_decode = 0;
    playback->playing = 0;
    if(wma_pause) wma_do_pause(playback, 0);
    g_thread_join(wma_decode_thread);
    playback->output->close_audio();
}
