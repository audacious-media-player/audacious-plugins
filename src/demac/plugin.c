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

#include <audacious/i18n.h> 

#include <audacious/plugin.h> 
#include <audacious/main.h> 
#include <audacious/output.h> 
#include <audacious/vfs.h> 
#include <audacious/util.h> 

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
static unsigned long seek_to_msec=(unsigned long)-1; /* -1 == not needed */

static InputPlugin demac_ip;

#ifdef DEBUG
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

static void demac_do_mseek(APEContext *ctx, unsigned long msec) {
  if(ctx->seektable) {
    unsigned int framecount = msec * ((unsigned long long)ctx->totalframes - 1L) / ctx->duration;
    ctx->currentframe = framecount;
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
    unsigned long local_seek_to;
    APEContext *ctx = NULL;
    APEDecoderContext *dec = NULL;
    int decoded_bytes;
    int pkt_size, bytes_used;
#ifdef DEBUG
    uint32_t frame_crc;
#endif
    
    if ((vfd = aud_vfs_fopen(pb->filename, "r")) == NULL) {
#ifdef DEBUG
        fprintf(stderr, "** demac: plugin.c: Error opening URI: %s\n", pb->filename);
#endif
        pb->error = TRUE;
        goto cleanup;
    }

    ctx = calloc(sizeof(APEContext), 1);
    if(ape_read_header(ctx, vfd, 0) < 0) {
        pb->error = TRUE;
#ifdef DEBUG
        fprintf(stderr, "** demac: plugin.c: Cannot parse APE header or unsupported format: %s\n", pb->filename);
#endif
        goto cleanup;
    }

    dec = calloc(sizeof(APEDecoderContext), 1);
    if(ape_decode_init(dec, ctx) < 0) {
        pb->error = TRUE;
#ifdef DEBUG
        fprintf(stderr, "** demac: plugin.c: Error initializing decoder\n");
#endif
        goto cleanup;
    }
    
    frame_buf = malloc(ctx->max_packet_size);
   
#ifdef DEBUG
    fprintf(stderr, "** demac: plugin.c: Duration: %u msec\n", ctx->duration);
#endif

    if(!pb->output->open_audio(FMT_S16_LE, ctx->samplerate, ctx->channels)) {
        pb->error = TRUE;
#ifdef DEBUG
        fprintf(stderr, "** demac: plugin.c: Cannot open audio.\n");
#endif
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
        local_seek_to = seek_to_msec;
        g_mutex_unlock(demac_mutex);
        
        /* do seeking */
        if (local_seek_to != -1) {
            demac_do_mseek(ctx, local_seek_to);
            pb->output->flush(local_seek_to);
	    
	    local_seek_to = -1;
            g_mutex_lock(demac_mutex);
	    seek_to_msec = -1;
            g_mutex_unlock(demac_mutex);

	    /* reset decoder */
	    dec->samples = 0;
	}

        ape_read_packet(ctx, vfd, frame_buf, &pkt_size);
#ifdef DEBUG
        assert(pkt_size <= ctx->max_packet_size);
	frame_crc = ape_initcrc();
#endif
	bytes_used = 0;

/*#ifdef DEBUG
        fprintf(stderr, "frame %d, %d samples, offset %d, size %d\n", ctx->currentframe-1,
	        *((uint32_t*)frame_buf), *((uint32_t*)(frame_buf+4)), pkt_size);
#endif*/

        /* Decode the frame a chunk at a time */
        while (playing && (bytes_used != pkt_size) && (local_seek_to == -1))
        {
            g_mutex_lock(demac_mutex);
            playing = pb->playing;
            local_seek_to = seek_to_msec;
            g_mutex_unlock(demac_mutex);
	    
	    decoded_bytes = wav_buffer_size;
            bytes_used = ape_decode_frame(dec, wav, &decoded_bytes, frame_buf, pkt_size);
	    if(bytes_used < 0) {
	        /* skip frame */
		dec->samples = 0;
		break;
	    }
	    
	    if(local_seek_to != -1) break;

            /* Write audio data */
            pb->pass_audio(pb, FMT_S16_LE, ctx->channels, decoded_bytes, wav, &playing);
#if DEBUG
            frame_crc = ape_updatecrc(wav, decoded_bytes, frame_crc);
#endif

        }

#if DEBUG
        frame_crc = ape_finishcrc(frame_crc);

        if (dec->CRC != frame_crc) {
            fprintf(stderr, "** demac: plugin.c: CRC error in frame %d\n", ctx->currentframe-1);
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

#ifdef DEBUG
    fprintf(stderr, "** demac: plugin.c: decoding loop finished\n");
#endif

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
#ifdef DEBUG
        fprintf(stderr, "** demac: plugin.c: waiting for thread finished\n");
#endif
        //g_thread_join(pb->thread);
	/* pb->thread is useless if input plugin initialized from **terrible** cue-sheet plugin */
        g_thread_join(pb_thread);
#ifdef DEBUG
        fprintf(stderr, "** demac: plugin.c: thread finished\n");
#endif
    }
}

static void demac_pause(InputPlayback *pb, short paused) {
    pb->output->pause(paused);
}

Tuple *demac_probe_for_tuple (gchar *uri, VFSFile *vfd) {
#ifdef DEBUG
    fprintf(stderr, "** demac: plugin.c: demac_probe_for_tuple()\n");
#endif
    Tuple *tpl = aud_tuple_new_from_filename(uri);
    gchar codec_string[32];

    GHashTable *tag = NULL;
    gchar *item;
    if ((tag = parse_apev2_tag(vfd)) != NULL) {
        if((item = g_hash_table_lookup (tag, "Artist")) != NULL) aud_tuple_associate_string(tpl, FIELD_ARTIST, NULL, item);
        if((item = g_hash_table_lookup (tag, "Title")) != NULL) aud_tuple_associate_string(tpl, FIELD_TITLE, NULL, item);
        if((item = g_hash_table_lookup (tag, "Album")) != NULL) aud_tuple_associate_string(tpl, FIELD_ALBUM, NULL, item);
        if((item = g_hash_table_lookup (tag, "Comment")) != NULL) aud_tuple_associate_string(tpl, FIELD_COMMENT, NULL, item);
        if((item = g_hash_table_lookup (tag, "Genre")) != NULL) aud_tuple_associate_string(tpl, FIELD_GENRE, NULL, item);
        if((item = g_hash_table_lookup (tag, "Track")) != NULL) aud_tuple_associate_int(tpl, FIELD_TRACK_NUMBER, NULL, atoi(item));
        if((item = g_hash_table_lookup (tag, "Year")) != NULL) aud_tuple_associate_int(tpl, FIELD_YEAR, NULL, atoi(item));
    }

    APEContext *ctx = calloc(sizeof(APEContext), 1);
    aud_vfs_rewind(vfd);
    ape_read_header(ctx, vfd, 1);
    aud_tuple_associate_int(tpl, FIELD_LENGTH, NULL, ctx->duration);
    ape_read_close(ctx);
    free(ctx);

    if (tag) g_hash_table_remove_all(tag);
    
    g_sprintf(codec_string, "Monkey's Audio v%4.2f", (float)ctx->fileversion/1000.0);
#ifdef DEBUG
    fprintf(stderr, "** demac: plugin.c: Codec: %s\n", codec_string);
#endif
    aud_tuple_associate_string(tpl, FIELD_CODEC, NULL, codec_string);
    aud_tuple_associate_string(tpl, FIELD_QUALITY, NULL, "lossless");
    return tpl;
}

Tuple *demac_get_tuple(char *filename) {
#ifdef DEBUG
  fprintf(stderr, "** demac: plugin.c: demac_get_tuple()\n");
#endif
  VFSFile *vfd;

  if ((vfd = aud_vfs_fopen(filename, "r")) == NULL) {
    return NULL;
  }

  Tuple *tpl = demac_probe_for_tuple(filename, vfd);
  aud_vfs_fclose(vfd);
  return tpl;
}

static void demac_mseek (InputPlayback *pb, gulong millisecond) {
  g_mutex_lock(demac_mutex);
  seek_to_msec = millisecond;
  g_mutex_unlock(demac_mutex);
#ifdef DEBUG
  fprintf(stderr, "** demac: plugin.c: seeking to %u msec\n", millisecond);
#endif
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
    static GtkWidget *about_window = NULL;

    if (about_window)
      gdk_window_raise(about_window->window);
    else {
      about_window = audacious_info_dialog(_("About Monkey's Audio Plugin"),
                                       _("Copyright (C) 2007 Eugene Zagidullin <e.asphyx@gmail.com>\n"
                                        "Based on ffape decoder, Copyright (C) 2007 Benjamin Zores\n"
					"ffape itself based on libdemac by Dave Chapman\n\n"
					"ffape is a part of FFmpeg project, http://ffmpeg.mplayerhq.hu/"),
                                       _("Ok"), FALSE, NULL, NULL);
      g_signal_connect(G_OBJECT(about_window), "destroy",
                       G_CALLBACK(gtk_widget_destroyed), &about_window);
    }
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
};

InputPlugin *demac_iplist[] = { &demac_ip, NULL };
DECLARE_PLUGIN(demac, NULL, NULL, demac_iplist, NULL, NULL, NULL, NULL, NULL);

 /* vim:foldmethod=syntax: */
