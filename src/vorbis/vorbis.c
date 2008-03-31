/*
 * Copyright (C) Tony Arcieri <bascule@inferno.tusculum.edu>
 * Copyright (C) 2001-2002  Haavard Kvaalen <havardk@xmms.org>
 * Copyright (C) 2007 William Pitcock <nenolod@sacredspiral.co.uk>
 * Copyright (C) 2008 Cristi Măgherușan <majeru@gentoo.ro>
 * Copyright (C) 2008 Eugene Zagidullin <e.asphyx@gmail.com>
 *
 * ReplayGain processing Copyright (C) 2002 Gian-Carlo Pascutto <gcp@sjeng.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

/*
 * 2002-01-11 ReplayGain processing added by Gian-Carlo Pascutto <gcp@sjeng.org>
 */

/*
 * Note that this uses vorbisfile, which is not (currently)
 * thread-safe.
 */

#include "config.h"
/*#define AUD_DEBUG
#define DEBUG*/

#define REMOVE_NONEXISTANT_TAG(x)   if (x != NULL && !*x) { x = NULL; }

#include <glib.h>
#include <gtk/gtk.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <fcntl.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/util.h>
#include <audacious/i18n.h>
#include <audacious/strings.h>

#include "vorbis.h"

extern vorbis_config_t vorbis_cfg;

static Tuple *get_song_tuple(gchar *filename);
static int vorbis_check_fd(char *filename, VFSFile *stream);
static void vorbis_play(InputPlayback *data);
static void vorbis_stop(InputPlayback *data);
static void vorbis_pause(InputPlayback *data, short p);
static void vorbis_seek(InputPlayback *data, int time);
static gchar *vorbis_generate_title(OggVorbis_File * vorbisfile, gchar * fn);
static void vorbis_aboutbox(void);
static void vorbis_init(void);
static void vorbis_cleanup(void);
static long vorbis_interleave_buffer(float **pcm, int samples, int ch,
                                     float *pcmout);
static gboolean vorbis_update_replaygain(ReplayGainInfo *rg_info);

static size_t ovcb_read(void *ptr, size_t size, size_t nmemb,
                        void *datasource);
static int ovcb_seek(void *datasource, int64_t offset, int whence);
static int ovcb_close(void *datasource);
static long ovcb_tell(void *datasource);

ov_callbacks vorbis_callbacks = {
    ovcb_read,
    ovcb_seek,
    ovcb_close,
    ovcb_tell
};

ov_callbacks vorbis_callbacks_stream = {
    ovcb_read,
    NULL,
    ovcb_close,
    NULL
};

gchar *vorbis_fmts[] = { "ogg", "ogm", NULL };

InputPlugin vorbis_ip = {
    .description = "Ogg Vorbis Audio Plugin",  /* description */
    .init = vorbis_init,                /* init */
    .about = vorbis_aboutbox,            /* aboutbox */
    .configure = vorbis_configure,           /* configure */
    .play_file = vorbis_play,
    .stop = vorbis_stop,
    .pause = vorbis_pause,
    .seek = vorbis_seek,
    .cleanup = vorbis_cleanup,
    .get_song_tuple = get_song_tuple,
    .is_our_file_from_vfs = vorbis_check_fd,
    .vfs_extensions = vorbis_fmts,
    .update_song_tuple = vorbis_update_song_tuple,
};

InputPlugin *vorbis_iplist[] = { &vorbis_ip, NULL };

DECLARE_PLUGIN(vorbis, NULL, NULL, vorbis_iplist, NULL, NULL, NULL, NULL, NULL);

static OggVorbis_File vf;

static GThread *thread;
static volatile int seekneeded = -1;
static int samplerate, channels;
GMutex *vf_mutex;

gchar **vorbis_tag_encoding_list = NULL;
static GtkWidget *about_window;

static int
vorbis_check_fd(char *filename, VFSFile *stream)
{
    OggVorbis_File vfile;       /* avoid thread interaction */
    gint result;
    VFSVorbisFile *fd;

    fd = g_new0(VFSVorbisFile, 1);
    fd->fd = stream;
    fd->probe = TRUE;

    /*
     * The open function performs full stream detection and machine
     * initialization.  If it returns zero, the stream *is* Vorbis and
     * we're fully ready to decode.
     */

    /* libvorbisfile isn't thread safe... */
    memset(&vfile, 0, sizeof(vfile));
    g_mutex_lock(vf_mutex);

    result = ov_test_callbacks(fd, &vfile, NULL, 0, aud_vfs_is_streaming(stream) ? vorbis_callbacks_stream : vorbis_callbacks);

    switch (result) {
    case OV_EREAD:
#ifdef DEBUG
        g_message("** vorbis.c: Media read error: %s", filename);
#endif
        g_mutex_unlock(vf_mutex);
        return FALSE;
        break;
    case OV_ENOTVORBIS:
#ifdef DEBUG
        g_message("** vorbis.c: Not Vorbis data: %s", filename);
#endif
        g_mutex_unlock(vf_mutex);
        return FALSE;
        break;
    case OV_EVERSION:
#ifdef DEBUG
        g_message("** vorbis.c: Version mismatch: %s", filename);
#endif
        g_mutex_unlock(vf_mutex);
        return FALSE;
        break;
    case OV_EBADHEADER:
#ifdef DEBUG
        g_message("** vorbis.c: Invalid Vorbis bistream header: %s",
                  filename);
#endif
        g_mutex_unlock(vf_mutex);
        return FALSE;
        break;
    case OV_EFAULT:
#ifdef DEBUG
        g_message("** vorbis.c: Internal logic fault while reading %s",
                  filename);
#endif
        g_mutex_unlock(vf_mutex);
        return FALSE;
        break;
    case 0:
        break;
    default:
        break;
    }

    ov_clear(&vfile);           /* once the ov_open succeeds, the stream belongs to
                                   vorbisfile.a.  ov_clear will fclose it */
    g_mutex_unlock(vf_mutex);
    return TRUE;
}

static void
vorbis_jump_to_time(InputPlayback *playback, long time)
{
    g_mutex_lock(vf_mutex);

    /*
     * We need to guard against seeking to the end, or things
     * don't work right.  Instead, just seek to one second prior
     * to this
     */
    if (time == ov_time_total(&vf, -1))
        time--;

    playback->output->flush(time * 1000);
    ov_time_seek(&vf, time);

    g_mutex_unlock(vf_mutex);
}

static void
do_seek(InputPlayback *playback)
{
    if (seekneeded != -1) {
        vorbis_jump_to_time(playback, seekneeded);
        seekneeded = -1;
        playback->eof = FALSE;
    }
}

#define PCM_FRAMES 1024
#define PCM_BUFSIZE PCM_FRAMES*2

static gpointer
vorbis_play_loop(gpointer arg)
{
    InputPlayback *playback = arg;
    char *filename = playback->filename;
    gchar *title = NULL;
    double time;
    long timercount = 0;
    vorbis_info *vi;
    gint br;
    VFSVorbisFile *fd = NULL;

    int last_section = -1;

    VFSFile *stream = NULL;
    void *datasource = NULL;

    /*gboolean use_rg;
    /float rg_scale = 1.0;*/

    ReplayGainInfo rg_info;

    memset(&vf, 0, sizeof(vf));

    if ((stream = aud_vfs_fopen(filename, "r")) == NULL) {
        playback->eof = TRUE;
        goto play_cleanup;
    }

    fd = g_new0(VFSVorbisFile, 1);
    fd->fd = stream;
    datasource = (void *) fd;

    /*char pcmout[4096];*/
    float pcmout[PCM_BUFSIZE*sizeof(float)];
    int bytes;
    float **pcm;

    /*
     * The open function performs full stream detection and
     * machine initialization.  None of the rest of ov_xx() works
     * without it
     *
     * A vorbis physical bitstream may consist of many logical
     * sections (information for each of which may be fetched from
     * the vf structure).  This value is filled in by ov_read to
     * alert us what section we're currently decoding in case we
     * need to change playback settings at a section boundary
     */
 
   
    g_mutex_lock(vf_mutex);
    if (ov_open_callbacks(datasource, &vf, NULL, 0, aud_vfs_is_streaming(fd->fd) ? vorbis_callbacks_stream : vorbis_callbacks) < 0) {
        vorbis_callbacks.close_func(datasource);
        g_mutex_unlock(vf_mutex);
        playback->eof = TRUE;
        goto play_cleanup;
    }
    vi = ov_info(&vf, -1);

    if (aud_vfs_is_streaming(fd->fd))
        time = -1;
    else
        time = ov_time_total(&vf, -1) * 1000;

    if (vi->channels > 2) {
        playback->eof = TRUE;
        g_mutex_unlock(vf_mutex);
        goto play_cleanup;
    }

    title = vorbis_generate_title(&vf, filename);
    vorbis_update_replaygain(&rg_info);
    playback->set_replaygain_info(playback, &rg_info);
    
    vi = ov_info(&vf, -1);

    samplerate = vi->rate;
    channels = vi->channels;
    br = vi->bitrate_nominal;

    g_mutex_unlock(vf_mutex);

    playback->set_params(playback, title, time, br, samplerate, channels);
    if (!playback->output->open_audio(FMT_FLOAT, vi->rate, vi->channels)) {
        playback->error = TRUE;
        goto play_cleanup;
    }

    seekneeded = -1;

    /*
     * Note that chaining changes things here; A vorbis file may
     * be a mix of different channels, bitrates and sample rates.
     * You can fetch the information for any section of the file
     * using the ov_ interface.
     */

    while (playback->playing) {
       
        if (playback->eof) {
            g_usleep(20000);
            continue;
        }

        if (seekneeded != -1)
            do_seek(playback);

        
        int current_section = last_section;

        g_mutex_lock(vf_mutex);
        
        bytes = ov_read_float(&vf, &pcm, PCM_FRAMES, &current_section);
        
        if (bytes > 0)
            bytes = vorbis_interleave_buffer(pcm, bytes, channels, pcmout);

        /*
         * We got some sort of error. Bail.
         */
        if (bytes <= 0 && bytes != OV_HOLE) {
           /*
            * EOF
            */
            AUDDBG("EOF\n");
            playback->playing = 0;
            playback->eof = TRUE;
            current_section = last_section;
        }

        if (current_section <= last_section) {
            /*
             * The info struct is different in each section.  vf
             * holds them all for the given bitstream.  This
             * requests the current one
             */
            vorbis_info *vi = ov_info(&vf, -1);

            if (vi->channels > 2) {
                playback->eof = TRUE;
                g_mutex_unlock(vf_mutex);
                goto stop_processing;
            }


            if (vi->rate != samplerate || vi->channels != channels) {
                samplerate = vi->rate;
                channels = vi->channels;
                while(playback->output->buffer_playing()) g_usleep(50000);
                playback->output->close_audio();
                if (!playback->output->
                        open_audio(FMT_FLOAT, vi->rate, vi->channels)) {
                    playback->error = TRUE;
                    playback->eof = TRUE;
                    g_mutex_unlock(vf_mutex);
                    goto stop_processing;
                }
                playback->output->flush(ov_time_tell(&vf) * 1000);
                vorbis_update_replaygain(&rg_info);
                playback->set_replaygain_info(playback, &rg_info); /* audio reopened */
            }
        }

        g_mutex_unlock(vf_mutex);

        playback->pass_audio(playback, FMT_FLOAT, channels, bytes, pcmout, &playback->playing);

        if (!playback->playing)
            goto stop_processing;

        if (seekneeded != -1)
            do_seek(playback);

        stop_processing:
   
        if (current_section <= last_section) {
            /*
            * set total play time, bitrate, rate, and channels of
            * current section
            */
            if (title)
                g_free(title);

            g_mutex_lock(vf_mutex);
            title = vorbis_generate_title(&vf, filename);

            if (time != -1)
                time = ov_time_total(&vf, -1) * 1000;

            g_mutex_unlock(vf_mutex);
        
            playback->set_params(playback, title, time, br, samplerate, channels);

            timercount = playback->output->output_time();

            last_section = current_section;
         
        }
    } /* main loop */

    if (!playback->error) {
        /*this loop makes it not skip the last ~4 seconds, but the playback 
         * timer isn't updated in this period, so it still needs a bit of work 
         *
         * majeru
         */
        if(playback->eof) /* do it only on EOF --asphyx */
            while(playback->output->buffer_playing()) {
                AUDDBG("waiting for empty output buffer\n");
                g_usleep(50000);
            }

        playback->output->close_audio();
    }


    play_cleanup:
    g_free(title);

    /*
     * ov_clear closes the stream if its open.  Safe to call on an
     * uninitialized structure as long as we've zeroed it
     */
    g_mutex_lock(vf_mutex);
    ov_clear(&vf);
    g_mutex_unlock(vf_mutex);
    playback->playing = 0;
    return NULL;
}

static void
vorbis_play(InputPlayback *playback)
{
    playback->playing = 1;
    playback->eof = 0;
    playback->error = FALSE;

    thread = g_thread_self();
    playback->set_pb_ready(playback);
    vorbis_play_loop(playback);
}

static void
vorbis_stop(InputPlayback *playback)
{
    if (playback->playing) {
        playback->playing = 0;
        AUDDBG("waiting for playback thread finished\n");
        g_thread_join(thread);
        AUDDBG("playback finished\n");
    }
}

static void
vorbis_pause(InputPlayback *playback, short p)
{
    playback->output->pause(p);
}

static void
vorbis_seek(InputPlayback *data, int time)
{
    seekneeded = time;

    while (seekneeded != -1)
        g_usleep(20000);
}

/* Make sure you've locked vf_mutex */

static gboolean
vorbis_update_replaygain(ReplayGainInfo *rg_info)
{
    vorbis_comment *comment;
    char *rg_gain = NULL, *rg_peak = NULL;

    if (rg_info == NULL || (comment = ov_comment(&vf, -1)) == NULL)
        return FALSE;

    rg_gain = vorbis_comment_query(comment, "replaygain_album_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_audiophile", 0);    /* Old */
    rg_info->album_gain = rg_gain != NULL ? atof(rg_gain) : 0.0;
    
    rg_gain = vorbis_comment_query(comment, "replaygain_track_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_radio", 0);    /* Old */
    rg_info->track_gain = rg_gain != NULL ? atof(rg_gain) : 0.0;
    
    rg_peak = vorbis_comment_query(comment, "replaygain_album_peak", 0);
    rg_info->album_peak = rg_peak != NULL ? atof(rg_peak) : 0.0;
    
    rg_peak = vorbis_comment_query(comment, "replaygain_track_peak", 0);
    if (!rg_peak) rg_peak = vorbis_comment_query(comment, "rg_peak", 0);  /* Old */
    rg_info->track_peak = rg_peak != NULL ? atof(rg_peak) : 0.0;

    return TRUE;
}

static long
vorbis_interleave_buffer(float **pcm, int samples, int ch, float *pcmout)
{
    int i, j;
    for (i = 0; i < samples; i++)
        for (j = 0; j < ch; j++)
            *pcmout++ = pcm[j][i];

    return ch * samples * sizeof(float);
}

static void _aud_tuple_associate_string(Tuple *tuple, const gint nfield, const gchar *field, const gchar *string)
{
    if (string) {
        gchar *str = aud_str_to_utf8(string);
        aud_tuple_associate_string(tuple, nfield, field, str);
        g_free(str);
    }
}

/*
 * Ok, nhjm449! Are you *happy* now?!  -nenolod
 */
static Tuple *
get_aud_tuple_for_vorbisfile(OggVorbis_File * vorbisfile, gchar *filename)
{
    VFSVorbisFile *vfd = (VFSVorbisFile *) vorbisfile->datasource;
    Tuple *tuple = NULL;
    gint length;
    vorbis_comment *comment = NULL;

    tuple = aud_tuple_new_from_filename(filename);

    if (aud_vfs_is_streaming(vfd->fd))
        length = -1;
    else
        length = ov_time_total(vorbisfile, -1) * 1000;

    /* associate with tuple */
    aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, length);
    /* maybe, it would be better to display nominal bitrate (like in main win), not average? --eugene */
    aud_tuple_associate_int(tuple, FIELD_BITRATE, NULL, ov_bitrate(vorbisfile, -1)/1000);

    if ((comment = ov_comment(vorbisfile, -1))) {
        gchar *tmps;
        _aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, vorbis_comment_query(comment, "title", 0));
        _aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, vorbis_comment_query(comment, "artist", 0));
        _aud_tuple_associate_string(tuple, FIELD_ALBUM, NULL, vorbis_comment_query(comment, "album", 0));
        _aud_tuple_associate_string(tuple, FIELD_DATE, NULL, vorbis_comment_query(comment, "date", 0));
        _aud_tuple_associate_string(tuple, FIELD_GENRE, NULL, vorbis_comment_query(comment, "genre", 0));
        _aud_tuple_associate_string(tuple, FIELD_COMMENT, NULL, vorbis_comment_query(comment, "comment", 0));

        if ((tmps = vorbis_comment_query(comment, "tracknumber", 0)) != NULL)
            aud_tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi(tmps));
    }

    aud_tuple_associate_string(tuple, FIELD_QUALITY, NULL, "lossy");

    if (comment && comment->vendor)
    {
        gchar *codec = g_strdup_printf("Ogg Vorbis [%s]", comment->vendor);
        aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, codec);
        g_free(codec);
    }
    else
        aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, "Ogg Vorbis");
    
    aud_tuple_associate_string(tuple, FIELD_MIMETYPE, NULL, "application/ogg");

    return tuple;
}

static Tuple *
get_song_tuple(gchar *filename)
{
    VFSFile *stream = NULL;
    OggVorbis_File vfile;          /* avoid thread interaction */
    Tuple *tuple = NULL;
    VFSVorbisFile *fd = NULL;

    if ((stream = aud_vfs_fopen(filename, "r")) == NULL)
        return NULL;

    fd = g_new0(VFSVorbisFile, 1);
    fd->fd = stream;

    /*
     * The open function performs full stream detection and
     * machine initialization.  If it returns zero, the stream
     * *is* Vorbis and we're fully ready to decode.
     */
    if (ov_open_callbacks(fd, &vfile, NULL, 0, aud_vfs_is_streaming(stream) ? vorbis_callbacks_stream : vorbis_callbacks) < 0) {
        aud_vfs_fclose(stream);
        return NULL;
    }

    tuple = get_aud_tuple_for_vorbisfile(&vfile, filename);

    /*
     * once the ov_open succeeds, the stream belongs to
     * vorbisfile.a.  ov_clear will fclose it
     */
    ov_clear(&vfile);

    return tuple;
}

static gchar *
vorbis_generate_title(OggVorbis_File * vorbisfile, gchar * filename)
{
    /* Caller should hold vf_mutex */
    gchar *displaytitle = NULL;
    Tuple *input;
    gchar *tmp;

    input = get_aud_tuple_for_vorbisfile(vorbisfile, filename);

    displaytitle = aud_tuple_formatter_make_title_string(input, vorbis_cfg.tag_override ?
                                                  vorbis_cfg.tag_format : aud_get_gentitle_format());

    if ((tmp = aud_vfs_get_metadata(((VFSVorbisFile *) vorbisfile->datasource)->fd, "stream-name")) != NULL)
    {
        gchar *old = displaytitle;

        aud_tuple_associate_string(input, -1, "stream", tmp);
        aud_tuple_associate_string(input, FIELD_TITLE, NULL, old);

        displaytitle = aud_tuple_formatter_process_string(input, "${?title:${title}}${?stream: (${stream})}");

	g_free(old);
	g_free(tmp);
    }

    aud_tuple_free(input);

    return displaytitle;
}

static void
vorbis_aboutbox(void)
{
    if (about_window)
        gtk_window_present(GTK_WINDOW(about_window));
    else
    {
      about_window = audacious_info_dialog(_("About Ogg Vorbis Audio Plugin"),
                                       /*
                                        * I18N: UTF-8 Translation: "Haavard Kvaalen" ->
                                        * "H\303\245vard Kv\303\245len"
                                        */
                                       _
                                       ("Ogg Vorbis Plugin by the Xiph.org Foundation\n\n"
                                        "Original code by\n"
                                        "Tony Arcieri <bascule@inferno.tusculum.edu>\n"
                                        "Contributions from\n"
                                        "Chris Montgomery <monty@xiph.org>\n"
                                        "Peter Alm <peter@xmms.org>\n"
                                        "Michael Smith <msmith@labyrinth.edu.au>\n"
                                        "Jack Moffitt <jack@icecast.org>\n"
                                        "Jorn Baayen <jorn@nl.linux.org>\n"
                                        "Haavard Kvaalen <havardk@xmms.org>\n"
                                        "Gian-Carlo Pascutto <gcp@sjeng.org>\n"
                                        "Eugene Zagidullin <e.asphyx@gmail.com>\n\n"
                                        "Visit the Xiph.org Foundation at http://www.xiph.org/\n"),
                                       _("Ok"), FALSE, NULL, NULL);
      g_signal_connect(G_OBJECT(about_window), "destroy",
                       G_CALLBACK(gtk_widget_destroyed), &about_window);
    }
}


static void
vorbis_init(void)
{
    ConfigDb *db;
    gchar *tmp = NULL;

    memset(&vorbis_cfg, 0, sizeof(vorbis_config_t));
    vorbis_cfg.http_buffer_size = 128;
    vorbis_cfg.http_prebuffer = 25;
    vorbis_cfg.proxy_port = 8080;
    vorbis_cfg.proxy_use_auth = FALSE;
    vorbis_cfg.proxy_user = NULL;
    vorbis_cfg.proxy_pass = NULL;
    vorbis_cfg.tag_override = FALSE;
    vorbis_cfg.tag_format = NULL;

    db = aud_cfg_db_open();
    aud_cfg_db_get_int(db, "vorbis", "http_buffer_size",
                       &vorbis_cfg.http_buffer_size);
    aud_cfg_db_get_int(db, "vorbis", "http_prebuffer",
                       &vorbis_cfg.http_prebuffer);
    aud_cfg_db_get_bool(db, "vorbis", "save_http_stream",
                        &vorbis_cfg.save_http_stream);
    if (!aud_cfg_db_get_string(db, "vorbis", "save_http_path",
                               &vorbis_cfg.save_http_path))
        vorbis_cfg.save_http_path = g_strdup(g_get_home_dir());

    aud_cfg_db_get_bool(db, "vorbis", "tag_override",
                        &vorbis_cfg.tag_override);
    if (!aud_cfg_db_get_string(db, "vorbis", "tag_format",
                               &vorbis_cfg.tag_format))
        vorbis_cfg.tag_format = g_strdup("%p - %t");

    aud_cfg_db_get_bool(db, NULL, "use_proxy", &vorbis_cfg.use_proxy);
    aud_cfg_db_get_string(db, NULL, "proxy_host", &vorbis_cfg.proxy_host);
    aud_cfg_db_get_string(db, NULL, "proxy_port", &tmp);

    if (tmp != NULL)
	vorbis_cfg.proxy_port = atoi(tmp);

    aud_cfg_db_get_bool(db, NULL, "proxy_use_auth", &vorbis_cfg.proxy_use_auth);
    aud_cfg_db_get_string(db, NULL, "proxy_user", &vorbis_cfg.proxy_user);
    aud_cfg_db_get_string(db, NULL, "proxy_pass", &vorbis_cfg.proxy_pass);

    aud_cfg_db_close(db);

    vf_mutex = g_mutex_new();

    aud_mime_set_plugin("application/ogg", &vorbis_ip);
}

static void
vorbis_cleanup(void)
{
    if (vorbis_cfg.save_http_path) {
        free(vorbis_cfg.save_http_path);
        vorbis_cfg.save_http_path = NULL;
    }

    if (vorbis_cfg.proxy_host) {
        free(vorbis_cfg.proxy_host);
        vorbis_cfg.proxy_host = NULL;
    }

    if (vorbis_cfg.proxy_user) {
        free(vorbis_cfg.proxy_user);
        vorbis_cfg.proxy_user = NULL;
    }

    if (vorbis_cfg.proxy_pass) {
        free(vorbis_cfg.proxy_pass);
        vorbis_cfg.proxy_pass = NULL;
    }

    if (vorbis_cfg.tag_format) {
        free(vorbis_cfg.tag_format);
        vorbis_cfg.tag_format = NULL;
    }

    if (vorbis_cfg.title_encoding) {
        free(vorbis_cfg.title_encoding);
        vorbis_cfg.title_encoding = NULL;
    }

    g_strfreev(vorbis_tag_encoding_list);
    g_mutex_free(vf_mutex);
}

static size_t
ovcb_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    VFSVorbisFile *handle = (VFSVorbisFile *) datasource;

    return aud_vfs_fread(ptr, size, nmemb, handle->fd);
}

static int
ovcb_seek(void *datasource, int64_t offset, int whence)
{
    VFSVorbisFile *handle = (VFSVorbisFile *) datasource;

    return aud_vfs_fseek(handle->fd, offset, whence);
}

static int
ovcb_close(void *datasource)
{
    VFSVorbisFile *handle = (VFSVorbisFile *) datasource;

    gint ret = 0;

    if (handle->probe == FALSE)
    {
        ret = aud_vfs_fclose(handle->fd);
/*        g_free(handle);  it causes double free. i'm not really sure that commenting out at here is correct. --yaz*/
    }

    return ret;
}

static long
ovcb_tell(void *datasource)
{
    VFSVorbisFile *handle = (VFSVorbisFile *) datasource;

    return aud_vfs_ftell(handle->fd);
}
