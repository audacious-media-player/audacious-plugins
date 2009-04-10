/* 
 * Audacious Monkey's Audio plugin
 *
 * Copyright (C) Eugene Zagidullin 2007
 *
 * Used some code from libdemac and example application:
 * Copyright (C) Dave Chapman 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 *  
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *  
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA
 *
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include "config.h" 

#include <ctype.h> 
#include <stdio.h> 

#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <assert.h> 

#include <glib.h> 
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <mowgli.h>

#include <audacious/i18n.h> 

#include <audacious/plugin.h> 
#include <audacious/output.h> 

#include "ape.h"
#include "apev2.h"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define MAX_BYTESPERSAMPLE 2 // currently only 16bps supported

static GThread *pb_thread;
static gpointer demac_decode_loop(InputPlayback *pb);
static Tuple *demac_get_tuple(char *filename);
static Tuple *demac_probe_for_tuple (gchar *uri, VFSFile *vfd);

static GMutex *demac_mutex;
static volatile int seek_to_msec = -1;
static volatile char pause_flag = 0;

static InputPlugin demac_ip;
static GtkWidget *about_window = NULL;

#ifdef AUD_DEBUG
# include "crc.c"
#endif

static gboolean demac_probe_vfs(char *filename, VFSFile* input_file) {
    APEContext *ctx;

    ctx = calloc(sizeof(APEContext), 1);
    if(ape_read_header(ctx, input_file, 1) < 0) {
      free(ctx);
      aud_vfs_rewind(input_file); /* Do we really need it? */
      return FALSE;
    }

    free(ctx);

    aud_vfs_rewind(input_file); /* Do we really need it? */

    return TRUE;
}

static void demac_play(InputPlayback *pb) {
    pb->playing = 1;
    pb->eof = 0;
    pb->error = FALSE;
    pb_thread = g_thread_self();
    pb->set_pb_ready(pb);

    demac_decode_loop(pb);
}

static void demac_do_mseek (APEContext * ctx, InputPlayback * playback) {
  if(ctx->seektable) {
     ctx->currentframe = (ctx->totalframes - 1) * (long long) seek_to_msec
      / ctx->duration;
     playback->output->flush (ctx->duration * (long long) ctx->currentframe
      / (ctx->totalframes - 1));
     seek_to_msec = -1;
  }
}

gpointer demac_decode_loop(InputPlayback *pb) {
    VFSFile *vfd;
    uint8_t *frame_buf = NULL;
    guint8 *wav = NULL;
    int wav_buffer_size;
    gchar *title = NULL;
    int audio_opened = 0;
    int playing;
    char paused = 0;
    APEContext *ctx = NULL;
    APEDecoderContext *dec = NULL;
    int decoded_bytes;
    int pkt_size, bytes_used;
#ifdef AUD_DEBUG
    uint32_t frame_crc;
#endif
    
    if ((vfd = aud_vfs_fopen(pb->filename, "r")) == NULL) {
        AUDDBG("** demac: plugin.c: Error opening URI: %s\n", pb->filename);
        pb->error = TRUE;
        goto cleanup;
    }

    ctx = calloc(sizeof(APEContext), 1);
    if(ape_read_header(ctx, vfd, 0) < 0) {
        pb->error = TRUE;
        AUDDBG("** demac: plugin.c: Cannot parse APE header or unsupported format: %s\n", pb->filename);
        goto cleanup;
    }

    dec = calloc(sizeof(APEDecoderContext), 1);
    if(ape_decode_init(dec, ctx) < 0) {
        pb->error = TRUE;
        AUDDBG("** demac: plugin.c: Error initializing decoder\n");
        goto cleanup;
    }
    
    frame_buf = malloc(ctx->max_packet_size);
   
    AUDDBG("** demac: plugin.c: Duration: %u msec\n", ctx->duration);

    if(!pb->output->open_audio(FMT_S16_LE, ctx->samplerate, ctx->channels)) {
        pb->error = TRUE;
        AUDDBG("** demac: plugin.c: Cannot open audio.\n");
        goto cleanup;
    }

    audio_opened = 1;

    Tuple *tpl = demac_probe_for_tuple (pb->filename, vfd);
    title = aud_tuple_formatter_make_title_string(tpl, aud_get_gentitle_format());
    pb->set_params(pb, title, ctx->duration, -1, ctx->samplerate, ctx->channels);
    aud_tuple_free(tpl);

    /* begin decoding */
    wav_buffer_size = ctx->blocksperframe * MAX_BYTESPERSAMPLE * ctx->channels;
    wav = malloc(wav_buffer_size);

    g_mutex_lock(demac_mutex);
    playing = pb->playing;
    g_mutex_unlock(demac_mutex);

    while (playing && (ctx->currentframe < ctx->totalframes))
    {
        g_mutex_lock(demac_mutex);
        playing = pb->playing;
        g_mutex_unlock(demac_mutex);
        if (seek_to_msec != -1) {
            demac_do_mseek (ctx, pb);
	    /* reset decoder */
	    dec->samples = 0;
	}

        ape_read_packet(ctx, vfd, frame_buf, &pkt_size);
#ifdef AUD_DEBUG
        assert(pkt_size <= ctx->max_packet_size);
	frame_crc = ape_initcrc();
#endif
	bytes_used = 0;

        /* Decode the frame a chunk at a time */
        while (playing && (bytes_used != pkt_size)) {
            if (pause_flag) {
               if (! paused) {
                  pb->output->pause (1);
                  paused = 1;
               }
               while (pause_flag && seek_to_msec == -1)
                  g_usleep (50000);
            }
            if (paused && ! pause_flag) {
               pb->output->pause (0);
               paused = 0;
            }
            if (seek_to_msec != -1)
               break;
            g_mutex_lock(demac_mutex);
            playing = pb->playing;
            g_mutex_unlock(demac_mutex);

	    decoded_bytes = wav_buffer_size;
            bytes_used = ape_decode_frame(dec, wav, &decoded_bytes, frame_buf, pkt_size);
	    if(bytes_used < 0) {
	        /* skip frame */
		dec->samples = 0;
		break;
	    }
            /* Write audio data */
            pb->pass_audio(pb, FMT_S16_LE, ctx->channels, decoded_bytes, wav, &playing);
#ifdef AUD_DEBUG
            frame_crc = ape_updatecrc(wav, decoded_bytes, frame_crc);
#endif

        }

#ifdef AUD_DEBUG
        frame_crc = ape_finishcrc(frame_crc);

        if (dec->CRC != frame_crc) {
            AUDDBG("** demac: plugin.c: CRC error in frame %d\n", ctx->currentframe-1);
        }
#endif
    }

cleanup:
    
    pb->eof = TRUE;
    pb->playing = 0;

    if(title) g_free(title);
    if(audio_opened) pb->output->close_audio();
   
    if(dec) {ape_decode_close(dec); free(dec);}
    if(wav) free(wav);
    if(frame_buf) free(frame_buf);
    if(ctx) {ape_read_close(ctx); free(ctx);}
    if(vfd) aud_vfs_fclose(vfd);

    AUDDBG("** demac: plugin.c: decoding loop finished\n");

    return NULL;
}

static void demac_stop(InputPlayback *pb)
{
    g_mutex_lock(demac_mutex);
    int playing = pb->playing;
    g_mutex_unlock(demac_mutex);

    if (playing) {
        g_mutex_lock(demac_mutex);
        pb->playing = 0;
        g_mutex_unlock(demac_mutex);
        AUDDBG("** demac: plugin.c: waiting for thread finished\n");
        g_thread_join(pb_thread);
        AUDDBG("** demac: plugin.c: thread finished\n");
    }
}

static void demac_pause(InputPlayback *pb, short paused) {
   pause_flag = paused;
}

static void destroy_cb(mowgli_dictionary_elem_t *delem, void *privdata) {
    g_free(delem->data);
}

Tuple *demac_probe_for_tuple (gchar *uri, VFSFile *vfd) {
    AUDDBG("** demac: plugin.c: demac_probe_for_tuple()\n");
    gchar codec_string[32];

    if (aud_vfs_is_streaming(vfd)) {
        /* This plugin does not support streams yet */
        return NULL;
    }

    APEContext *ctx = calloc(sizeof(APEContext), 1);
    aud_vfs_rewind(vfd);
    if (ape_read_header(ctx, vfd, 1) < 0) {
        free(ctx);
        aud_vfs_rewind(vfd);
        return NULL;
    }

    Tuple *tpl = aud_tuple_new_from_filename(uri);
    mowgli_dictionary_t *tag = NULL;
    gchar *item;
    if ((tag = parse_apev2_tag(vfd)) != NULL) {
        if((item = mowgli_dictionary_retrieve(tag, "Artist")) != NULL) aud_tuple_associate_string(tpl, FIELD_ARTIST, NULL, item);
        if((item = mowgli_dictionary_retrieve(tag, "Title")) != NULL) aud_tuple_associate_string(tpl, FIELD_TITLE, NULL, item);
        if((item = mowgli_dictionary_retrieve(tag, "Album")) != NULL) aud_tuple_associate_string(tpl, FIELD_ALBUM, NULL, item);
        if((item = mowgli_dictionary_retrieve(tag, "Comment")) != NULL) aud_tuple_associate_string(tpl, FIELD_COMMENT, NULL, item);
        if((item = mowgli_dictionary_retrieve(tag, "Genre")) != NULL) aud_tuple_associate_string(tpl, FIELD_GENRE, NULL, item);
        if((item = mowgli_dictionary_retrieve(tag, "Track")) != NULL) aud_tuple_associate_int(tpl, FIELD_TRACK_NUMBER, NULL, atoi(item));
        if((item = mowgli_dictionary_retrieve(tag, "Year")) != NULL) aud_tuple_associate_int(tpl, FIELD_YEAR, NULL, atoi(item));
    }

    aud_tuple_associate_int(tpl, FIELD_LENGTH, NULL, ctx->duration);
    g_snprintf(codec_string, sizeof(codec_string), "Monkey's Audio v%4.2f", (float)ctx->fileversion/1000.0);
    AUDDBG("** demac: plugin.c: Codec: %s\n", codec_string);
    aud_tuple_associate_string(tpl, FIELD_CODEC, NULL, codec_string);
    aud_tuple_associate_string(tpl, FIELD_QUALITY, NULL, "lossless");
    aud_tuple_associate_string(tpl, FIELD_MIMETYPE, NULL, "audio/x-ape");

    ape_read_close(ctx);
    free(ctx);

    if (tag) mowgli_dictionary_destroy(tag, destroy_cb, NULL);

    return tpl;
}

Tuple *demac_get_tuple(char *filename) {
  AUDDBG("** demac: plugin.c: demac_get_tuple()\n");
  VFSFile *vfd;

  if ((vfd = aud_vfs_fopen(filename, "r")) == NULL) {
    return NULL;
  }

  Tuple *tpl = demac_probe_for_tuple(filename, vfd);
  aud_vfs_fclose(vfd);
  return tpl;
}

static void demac_mseek (InputPlayback *pb, gulong millisecond) {
  seek_to_msec = millisecond;
  while (seek_to_msec != -1)
     g_usleep (50000);
}

static void demac_seek (InputPlayback *pb, gint time) {
  demac_mseek(pb, (unsigned long)time*1000);
}

static void demac_init() {
  /* Found in Freedesktop shared-mime-info */
  aud_mime_set_plugin("audio/x-ape", &demac_ip);
  demac_mutex = g_mutex_new();
}

static void demac_cleanup() {
  g_mutex_free(demac_mutex);
}

static void demac_about(void) {

    if (about_window) {
      gtk_window_present(GTK_WINDOW(about_window));
    } else {
      about_window = audacious_info_dialog(_("About Monkey's Audio Plugin"),
                                       _("Copyright (C) 2007 Eugene Zagidullin <e.asphyx@gmail.com>\n"
                                        "Based on ffape decoder, Copyright (C) 2007 Benjamin Zores\n"
					"ffape itself based on libdemac by Dave Chapman\n\n"
					"ffape is a part of FFmpeg project, http://ffmpeg.mplayerhq.hu/"),
                                       _("Ok"), FALSE, NULL, NULL);
      g_signal_connect(G_OBJECT(about_window), "destroy", G_CALLBACK(gtk_widget_destroyed), &about_window);
    }
}

static void insert_str_tuple_field_to_dictionary(Tuple *tuple, int fieldn, mowgli_dictionary_t *dict, char *key) {
    
    if(mowgli_dictionary_find(dict, key) != NULL) g_free(mowgli_dictionary_delete(dict, key));
    
    gchar *tmp = (gchar*)aud_tuple_get_string(tuple, fieldn, NULL);
    if(tmp != NULL && strlen(tmp) != 0) mowgli_dictionary_add(dict, key, g_strdup(tmp));
}

static void insert_int_tuple_field_to_dictionary(Tuple *tuple, int fieldn, mowgli_dictionary_t *dict, char *key) {
    int val;

    if(mowgli_dictionary_find(dict, key) != NULL) g_free(mowgli_dictionary_delete(dict, key));
    
    if(aud_tuple_get_value_type(tuple, fieldn, NULL) == TUPLE_INT && (val = aud_tuple_get_int(tuple, fieldn, NULL)) >= 0) {
        gchar *tmp = g_strdup_printf("%d", val);
        mowgli_dictionary_add(dict, key, tmp);
    }
}

static gboolean demac_update_song_tuple(Tuple *tuple, VFSFile *vfd) {

    mowgli_dictionary_t *tag = parse_apev2_tag(vfd);
    if (tag == NULL) tag = mowgli_dictionary_create(g_ascii_strcasecmp);

    insert_str_tuple_field_to_dictionary(tuple, FIELD_TITLE, tag, "Title");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_ARTIST, tag, "Artist");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_ALBUM, tag, "Album");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_COMMENT, tag, "Comment");
    insert_str_tuple_field_to_dictionary(tuple, FIELD_GENRE, tag, "Genre");
    
    insert_int_tuple_field_to_dictionary(tuple, FIELD_YEAR, tag, "Year");
    insert_int_tuple_field_to_dictionary(tuple, FIELD_TRACK_NUMBER, tag, "Track");

    gboolean ret = write_apev2_tag(vfd, tag);
    mowgli_dictionary_destroy(tag, destroy_cb, NULL);

    return ret;
}

static gchar *fmts[] = { "ape", NULL };

static InputPlugin demac_ip = {
    .description = "Monkey's Audio Plugin",
    .init = demac_init,
    .about = demac_about,
    .play_file = demac_play,
    .stop = demac_stop,
    .pause = demac_pause,
    .seek = demac_seek,
    .cleanup = demac_cleanup,
    .get_song_tuple = demac_get_tuple,
    .is_our_file_from_vfs = demac_probe_vfs,
    .vfs_extensions = fmts,
    .mseek = demac_mseek,
    .probe_for_tuple = demac_probe_for_tuple,
    .update_song_tuple = demac_update_song_tuple,
};

InputPlugin *demac_iplist[] = { &demac_ip, NULL };
DECLARE_PLUGIN(demac, NULL, NULL, demac_iplist, NULL, NULL, NULL, NULL, NULL);

 /* vim:foldmethod=syntax: */
