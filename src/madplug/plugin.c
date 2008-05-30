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

/* #define AUD_DEBUG 1 */

#include "config.h"
#include "plugin.h"
#include "input.h"

#include <math.h>

#include <gtk/gtk.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "tuple.h"

/*
 * Global variables
 */
audmad_config_t *audmad_config;   /**< global configuration */
GMutex *mad_mutex;
GMutex *pb_mutex;
GCond *mad_cond;

/*
 * static variables
 */
static GThread *decode_thread; /**< the single decoder thread */
static struct mad_info_t info;   /**< info for current track */

#ifndef NOGUI
static GtkWidget *error_dialog = 0;
#endif

extern gboolean scan_file(struct mad_info_t *info, gboolean fast);

static gint mp3_bitrate_table[5][16] = {
  { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1 }, /* MPEG1 L1 */
  { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1 }, /* MPEG1 L2 */
  { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1 }, /* MPEG1 L3 */
  { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, -1 }, /* MPEG2(.5) L1 */
  { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, -1 }  /* MPEG2(.5) L2,L3 */
};

static gint mp3_samplerate_table[4][4] = {
  { 11025, 12000, 8000, -1 },   /* MPEG2.5 */
  { -1, -1, -1, -1 },           /* Reserved */
  { 22050, 24000, 16000, -1 },  /* MPEG2 */
  { 44100, 48000, 32000, -1 }   /* MPEG1 */
};

/*
 * Function extname (filename)
 *
 *    Return pointer within filename to its extenstion, or NULL if
 *    filename has no extension.
 *
 */
static gchar *
extname(const char *filename)
{
    gchar *ext = strrchr(filename, '.');

    if (ext != NULL)
        ++ext;

    return ext;
}


static void
audmad_init()
{
    mcs_handle_t *db = NULL;

    audmad_config = g_malloc0(sizeof(audmad_config_t));

    audmad_config->dither = TRUE;
    audmad_config->force_reopen_audio = TRUE;
    audmad_config->fast_play_time_calc = TRUE;
    audmad_config->use_xing = TRUE;
    audmad_config->sjis = FALSE;
    audmad_config->show_avg_vbr_bitrate = TRUE;
    audmad_config->title_override = FALSE;


    db = aud_cfg_db_open();
    if (db) {
        //audio
        aud_cfg_db_get_bool(db, "MAD", "dither", &audmad_config->dither);
        aud_cfg_db_get_bool(db, "MAD", "force_reopen_audio",
                            &audmad_config->force_reopen_audio);

        //metadata
        aud_cfg_db_get_bool(db, "MAD", "fast_play_time_calc",
                            &audmad_config->fast_play_time_calc);
        aud_cfg_db_get_bool(db, "MAD", "use_xing",
                            &audmad_config->use_xing);
        aud_cfg_db_get_bool(db, "MAD", "sjis", &audmad_config->sjis);

        //misc
        aud_cfg_db_get_bool(db, "MAD", "show_avg_vbr_bitrate",
                            &audmad_config->show_avg_vbr_bitrate);

        //text
        aud_cfg_db_get_bool(db, "MAD", "title_override",
                            &audmad_config->title_override);
        aud_cfg_db_get_string(db, "MAD", "id3_format",
                              &audmad_config->id3_format);

        aud_cfg_db_close(db);
    }

    mad_mutex = g_mutex_new();
    pb_mutex = g_mutex_new();
    mad_cond = g_cond_new();

    if (!audmad_config->id3_format)
        audmad_config->id3_format = g_strdup("(none)");

    aud_mime_set_plugin("audio/mpeg", mad_plugin);
}

static void
audmad_cleanup()
{
    g_free(audmad_config->id3_format);
    g_free(audmad_config);
    
    g_cond_free(mad_cond);
    g_mutex_free(mad_mutex);
    g_mutex_free(pb_mutex);
}

static gboolean
mp3_head_check(guint32 head, gint *frameSize)
{
    gint version, layer, bitIndex, bitRate, sampleIndex, sampleRate, padding;

    /* http://www.mp3-tech.org/programmer/frame_header.html
     * Bits 21-31 must be set (frame sync)
     */
    if ((head & 0xffe00000) != 0xffe00000)
        return FALSE;

    /* check if layer bits (17-18) are good */
    layer = (head >> 17) & 0x3;
    if (!layer)
        return FALSE; /* 00 = reserved */
    layer = 4 - layer;

    /* check if bitrate index bits (12-15) are acceptable */
    bitIndex = (head >> 12) & 0xf;

    /* 1111 and 0000 are reserved values for all layers */
    if (bitIndex == 0xf || bitIndex == 0)
        return FALSE;

    /* check samplerate index bits (10-11) */
    sampleIndex = (head >> 10) & 0x3;
    if (sampleIndex == 0x3)
        return FALSE;

    /* check version bits (19-20) and get bitRate */
    version = (head >> 19) & 0x03;
    switch (version) {
        case 0: /* 00 = MPEG Version 2.5 */
        case 2: /* 10 = MPEG Version 2 */
            if (layer == 1)
                bitRate = mp3_bitrate_table[3][bitIndex];
            else
                bitRate = mp3_bitrate_table[4][bitIndex];
            break;

        case 1: /* 01 = reserved */
            return FALSE;

        case 3: /* 11 = MPEG Version 1 */
            bitRate = mp3_bitrate_table[layer][bitIndex];
            break;

        default:
            return FALSE;
    }

    /* check layer II restrictions vs. bitrate */
    if (layer == 2) {
        gint chanMode = (head >> 6) & 0x3;

        if (chanMode == 0x3) {
            /* single channel with bitrate > 192 */
            if (bitRate > 192)
                return FALSE;
        } else {
            /* any other mode with bitrates 32-56 and 80.
             * NOTICE! this check is not entirely correct, but I think
             * it is sufficient in most cases.
             */
            if (((bitRate >= 32 && bitRate <= 56) || bitRate == 80))
                return FALSE;
        }
    }

    /* calculate approx. frame size */
    padding = (head >> 9) & 1;
    sampleRate = mp3_samplerate_table[version][sampleIndex];
    if (layer == 1)
        *frameSize = ((12 * bitRate * 1000 / sampleRate) + padding) * 4;
    else
        *frameSize = (144 * bitRate * 1000) / (sampleRate + padding);

    /* check if bits 16 - 19 are all set (MPEG 1 Layer I, not protected?) */
    if (((head >> 19) & 1) == 1 &&
        ((head >> 17) & 3) == 3 && ((head >> 16) & 1) == 1)
        return FALSE;

    /* not sure why we check this, but ok! */
    if ((head & 0xffff0000) == 0xfffe0000)
        return FALSE;

    return TRUE;
}

static int
mp3_head_convert(const guchar * hbuf)
{
    return ((unsigned long) hbuf[0] << 24) |
        ((unsigned long) hbuf[1] << 16) |
        ((unsigned long) hbuf[2] << 8) | (unsigned long) hbuf[3];
}

// audacious vfs fast version
static int
audmad_is_our_fd(char *filename, VFSFile *fin)
{
    guint32 check;
    gchar *ext = extname(filename);
    guchar buf[4];

    info.remote = aud_vfs_is_remote(filename);

    /* I've seen some flac files beginning with id3 frames..
       so let's exclude known non-mp3 filename extensions */
    if ((ext != NULL) &&
        (!strcasecmp("flac", ext) || !strcasecmp("mpc", ext) ||
         !strcasecmp("tta", ext)  || !strcasecmp("ogg", ext) ||
         !strcasecmp("wma", ext) )
        )
        return 0;

    if (fin == NULL) {
        g_message("fin = NULL for %s", filename);
        return 0;
    }

    if (aud_vfs_fread(buf, 1, 4, fin) == 0) {
        g_message("aud_vfs_fread failed @1 %s", filename);
        return 0;
    }

    check = mp3_head_convert(buf);

    if (memcmp(buf, "ID3", 3) == 0)
        return 1;
    else if (memcmp(buf, "OggS", 4) == 0)
        return 0;
    else if (memcmp(buf, "RIFF", 4) == 0)
    {
        aud_vfs_fseek(fin, 4, SEEK_CUR);
        if(aud_vfs_fread(buf, 1, 4, fin) == 0) {
            g_message("aud_vfs_fread failed @2 %s", filename);
            return 0;
        }

        if (memcmp(buf, "RMP3", 4) == 0)
            return 1;
    }

    /* Check stream data for frame header(s)
     */
    guchar chkbuf[2048];
    gint chkret, i, framesize, cyc, chkcount, chksize = sizeof(chkbuf);
    
    for (cyc = chkcount = 0; cyc < 32; cyc++)
    {
        if ((chkret = aud_vfs_fread(chkbuf, 1, chksize, fin)) == 0)
            break;
        
        for (i = 0; i < chkret; i++)
        {
            check <<= 8;
            check |= chkbuf[i];

            if (mp3_head_check(check, &framesize)) {
                /* when the first matching frame header is found, we check for
                 * another frame by seeking to the approximate start of the
                 * next header ... also reduce the check size.
                 */
                if (++chkcount >= 4) return 1;
                aud_vfs_fseek(fin, framesize-8, SEEK_CUR);
                check = 0;
                chksize = 16;
            } else {
                aud_vfs_fseek(fin, chksize, SEEK_CUR);
            }
        }
    }

    g_message("Rejecting %s (not an MP3 file?)", filename);
    return 0;
}

// audacious vfs version
static int
audmad_is_our_file(char *filename)
{
    VFSFile *fin = NULL;
    gint rtn;

    fin = aud_vfs_fopen(filename, "rb");

    if (fin == NULL)
        return 0;

    rtn = audmad_is_our_fd(filename, fin);
    aud_vfs_fclose(fin);

    return rtn;
}

static void
audmad_stop(InputPlayback *playback)
{
    AUDDBG("f: audmad_stop\n");
    g_mutex_lock(mad_mutex);
    info.playback = playback;
    g_mutex_unlock(mad_mutex);

    if (decode_thread) {

        g_mutex_lock(mad_mutex);
        info.playback->playing = 0;
        g_mutex_unlock(mad_mutex);
        g_cond_signal(mad_cond);

        AUDDBG("waiting for thread\n");
        g_thread_join(decode_thread);
        AUDDBG("thread done\n");

        input_term(&info);
        decode_thread = NULL;

    }
    AUDDBG("e: audmad_stop\n");
}

static void
audmad_play_file(InputPlayback *playback)
{
    gboolean rtn;
    gchar *url = playback->filename;
    ReplayGainInfo rg_info;

#ifdef AUD_DEBUG
    {
        gchar *tmp = g_filename_to_utf8(url, -1, NULL, NULL, NULL);
        AUDDBG("playing %s\n", tmp);
        g_free(tmp);
    }
#endif                          /* DEBUG */

    if (input_init(&info, url, NULL) == FALSE) {
        g_message("error initialising input");
        return;
    }

    // remote access must use fast scan.
    rtn = input_get_info(&info, aud_vfs_is_remote(url) ? TRUE : audmad_config->fast_play_time_calc);

    if (rtn == FALSE) {
        g_message("error reading input info");
        /*
         * return;
         * commenting this return seems to be a hacky fix for the damn lastfm plugin playback
         * that used to work only for nenolod because of his fsck-ing lastfm subscription :p
        */
    }

    rg_info.track_gain = info.replaygain_track_scale;
    rg_info.track_peak = info.replaygain_track_peak;
    rg_info.album_gain = info.replaygain_album_scale;
    rg_info.album_peak = info.replaygain_album_peak;
    AUDDBG("Replay Gain info:\n");
    AUDDBG("* track gain:          %+f dB\n", rg_info.track_gain);
    AUDDBG("* track peak:          %f\n",     rg_info.track_peak);
    AUDDBG("* album gain:          %+f dB\n", rg_info.album_gain);
    AUDDBG("* album peak:          %f\n",     rg_info.album_peak);
    playback->set_replaygain_info(playback, &rg_info);

    g_mutex_lock(pb_mutex);
    info.playback = playback;
    info.playback->playing = 1;
    g_mutex_unlock(pb_mutex);

    decode_thread = g_thread_self();
    playback->set_pb_ready(playback);
    decode_loop(&info);
}

static void
audmad_pause(InputPlayback *playback, short paused)
{
    g_mutex_lock(pb_mutex);
    info.playback = playback;
    g_mutex_unlock(pb_mutex);
    playback->output->pause(paused);
}

static void
audmad_mseek(InputPlayback *playback, gulong millisecond)
{
    g_mutex_lock(pb_mutex);
    info.playback = playback;
    info.seek = millisecond;
    g_mutex_unlock(pb_mutex);
}

static void
audmad_seek(InputPlayback *playback, gint time)
{
    audmad_mseek(playback, time * 1000);
}

/**
 * Scan the given file or URL.
 * Fills in the title string and the track length in milliseconds.
 */
static void
audmad_get_song_info(char *url, char **title, int *length)
{
    struct mad_info_t myinfo;
#ifdef AUD_DEBUG
    gchar *tmp = g_filename_to_utf8(url, -1, NULL, NULL, NULL);
    AUDDBG("f: audmad_get_song_info: %s\n", tmp);
    g_free(tmp);
#endif                          /* DEBUG */

    if (input_init(&myinfo, url, NULL) == FALSE) {
        AUDDBG("error initialising input\n");
        return;
    }

    if (input_get_info(&myinfo, info.remote ? TRUE : audmad_config->fast_play_time_calc) == TRUE) {
        if(aud_tuple_get_string(myinfo.tuple, -1, "track-name"))
            *title = g_strdup(aud_tuple_get_string(myinfo.tuple, -1, "track-name"));
        else
            *title = g_strdup(url);

        *length = aud_tuple_get_int(myinfo.tuple, FIELD_LENGTH, NULL);
        if(*length == -1)
            *length = mad_timer_count(myinfo.duration, MAD_UNITS_MILLISECONDS);
    }
    else {
        *title = g_strdup(url);
        *length = -1;
    }
    input_term(&myinfo);
    AUDDBG("e: audmad_get_song_info\n");
}

static gboolean
audmad_fill_info(struct mad_info_t *info, VFSFile *fd)
{
    if (fd == NULL || info == NULL) return FALSE;
    AUDDBG("f: audmad_fill_info(): %s\n", fd->uri);

    if (input_init(info, fd->uri, fd) == FALSE) {
        AUDDBG("audmad_fill_info(): error initialising input\n");
        return FALSE;
    }
    
    info->fileinfo_request = FALSE; /* we don't need to read tuple again */
    return input_get_info(info, aud_vfs_is_remote(fd->uri) ? TRUE : audmad_config->fast_play_time_calc);
}

static void
audmad_about()
{
    static GtkWidget *aboutbox;
    gchar *scratch;

    if (aboutbox != NULL)
        return;

    scratch = g_strdup_printf(
    _("Audacious MPEG Audio Plugin\n"
    "\n"
    "Compiled against libMAD version: %d.%d.%d%s\n"
    "\n"
    "Written by:\n"
    "    William Pitcock <nenolod@sacredspiral.co.uk>\n"
    "    Yoshiki Yazawa <yaz@cc.rim.or.jp>\n"
    "\n"
    "Portions derived from XMMS-MAD by:\n"
    "    Sam Clegg\n"
    "\n"
    "ReplayGain support by:\n"
    "    Samuel Krempp"),
    MAD_VERSION_MAJOR, MAD_VERSION_MINOR, MAD_VERSION_PATCH,
    MAD_VERSION_EXTRA);

    aboutbox = audacious_info_dialog(_("About MPEG Audio Plugin"),
                                 scratch,
                                 _("Ok"), FALSE, NULL, NULL);

    g_free(scratch);

    g_signal_connect(G_OBJECT(aboutbox), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &aboutbox);
}

/**
 * Display a GTK box containing the given error message.
 * Taken from mpg123 plugin.
 */
void
audmad_error(char *error, ...)
{
#ifndef NOGUI
    if (!error_dialog) {
        va_list args;
        char string[256];
        va_start(args, error);
        vsnprintf(string, 256, error, args);
        va_end(args);
        GDK_THREADS_ENTER();
        error_dialog =
            audacious_info_dialog(_("Error"), string, _("Ok"), FALSE, 0, 0);
        gtk_signal_connect(GTK_OBJECT(error_dialog), "destroy",
                           GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                           &error_dialog);
        GDK_THREADS_LEAVE();
    }
#endif                          /* !NOGUI */
}

extern void audmad_configure();

static void
__set_and_free(Tuple *tuple, gint nfield, gchar *name, gchar *value)
{
    aud_tuple_associate_string(tuple, nfield, name, value);
    g_free(value);
}

// tuple stuff
static Tuple *
__audmad_get_song_tuple(char *filename, VFSFile *fd)
{
    Tuple *tuple = NULL;
    gchar *string = NULL;

    struct id3_file *id3file = NULL;
    struct id3_tag *tag = NULL;

    struct mad_info_t myinfo;

    gboolean local_fd = FALSE;
    int length;

#ifdef AUD_DEBUG
    string = aud_str_to_utf8(filename);
    AUDDBG("f: mad: audmad_get_song_tuple: %s\n", string);
    g_free(string);
    string = NULL;
#endif

    /* isn't is obfuscated? --eugene */

    if(info.remote && mad_timer_count(info.duration, MAD_UNITS_SECONDS) <= 0){
        if((fd && aud_vfs_is_streaming(fd)) || (info.playback && info.playback->playing)) {
            gchar *tmp = NULL;
            tuple = aud_tuple_new_from_filename(filename);

#ifdef AUD_DEBUG
            if(info.playback)
                AUDDBG("info.playback->playing = %d\n",info.playback->playing);
#endif
            tmp = aud_vfs_get_metadata(info.infile ? info.infile : fd, "track-name");
            if(tmp){
                gchar *scratch;

                scratch = aud_str_to_utf8(tmp);
                aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, scratch);
                g_free(tmp);
                g_free(scratch);

                tmp = NULL;
            }
            tmp = aud_vfs_get_metadata(info.infile ? info.infile : fd, "stream-name");
            if(tmp){
                gchar *scratch;

                scratch = aud_str_to_utf8(tmp);
                aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, scratch);
                g_free(tmp);
                g_free(scratch);

                tmp = NULL;
            }

            AUDDBG("audmad_get_song_tuple: track_name = %s\n", aud_tuple_get_string(tuple, -1, "track-name"));
            AUDDBG("audmad_get_song_tuple: stream_name = %s\n", aud_tuple_get_string(tuple, -1, "stream-name"));
            aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, -1);
            aud_tuple_associate_int(tuple, FIELD_MTIME, NULL, 0); // this indicates streaming
            AUDDBG("get_song_tuple: remote: tuple\n");
            return tuple;
        }
        AUDDBG("get_song_tuple: remote: NULL\n");
    } /* info.remote  */

    // if !fd, pre-open the file with aud_vfs_fopen() and reuse fd.
    if(!fd) {
        fd = aud_vfs_fopen(filename, "rb");
        if(!fd)
            return NULL;
        local_fd = TRUE;
    }

    if (!audmad_fill_info(&myinfo, fd)) {
        AUDDBG("get_song_tuple: error obtaining info\n");
        if (local_fd) aud_vfs_fclose(fd);
        return NULL;
    }

    tuple = aud_tuple_new();
    aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, -1);

    id3file = id3_file_vfsopen(fd, ID3_FILE_MODE_READONLY);

    if (id3file) {

        tag = id3_file_tag(id3file);
        if (tag) {
            __set_and_free(tuple, FIELD_ARTIST, NULL, input_id3_get_string(tag, ID3_FRAME_ARTIST));
            __set_and_free(tuple, FIELD_ALBUM, NULL, input_id3_get_string(tag, ID3_FRAME_ALBUM));
            __set_and_free(tuple, FIELD_TITLE, NULL, input_id3_get_string(tag, ID3_FRAME_TITLE));

            // year
            string = NULL;
            string = input_id3_get_string(tag, ID3_FRAME_YEAR); //TDRC
            if (!string)
                string = input_id3_get_string(tag, "TYER");

            if (string) {
                aud_tuple_associate_int(tuple, FIELD_YEAR, NULL, atoi(string));
                g_free(string);
                string = NULL;
            }

            __set_and_free(tuple, FIELD_FILE_NAME, NULL, aud_uri_to_display_basename(filename));
            __set_and_free(tuple, FIELD_FILE_PATH, NULL, aud_uri_to_display_dirname(filename));
            aud_tuple_associate_string(tuple, FIELD_FILE_EXT, NULL, extname(filename));

            // length
            length = mad_timer_count(myinfo.duration, MAD_UNITS_MILLISECONDS);
            aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, length);

            // track number
            string = input_id3_get_string(tag, ID3_FRAME_TRACK);
            if (string) {
                aud_tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi(string));
                g_free(string);
                string = NULL;
            }
            // genre
            __set_and_free(tuple, FIELD_GENRE, NULL, input_id3_get_string(tag, ID3_FRAME_GENRE));
            __set_and_free(tuple, FIELD_COMMENT, NULL, input_id3_get_string(tag, ID3_FRAME_COMMENT));
            AUDDBG("genre = %s\n", aud_tuple_get_string(tuple, FIELD_GENRE, NULL));
        }
        id3_file_close(id3file);
    } // id3file
    else { // no id3tag
        __set_and_free(tuple, FIELD_FILE_NAME, NULL, aud_uri_to_display_basename(filename));
        __set_and_free(tuple, FIELD_FILE_PATH, NULL, aud_uri_to_display_dirname(filename));
        aud_tuple_associate_string(tuple, FIELD_FILE_EXT, NULL, extname(filename));
        // length
        length = mad_timer_count(myinfo.duration, MAD_UNITS_MILLISECONDS);
        aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, length);
    }

    aud_tuple_associate_string(tuple, FIELD_QUALITY, NULL, "lossy");
    aud_tuple_associate_int(tuple, FIELD_BITRATE, NULL, myinfo.bitrate / 1000);

    string = g_strdup_printf("MPEG-1 Audio Layer %d", myinfo.mpeg_layer);
    aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, string);
    g_free(string);

    aud_tuple_associate_string(tuple, FIELD_MIMETYPE, NULL, "audio/mpeg");
    
    input_term(&myinfo);

    if(local_fd)
        aud_vfs_fclose(fd);

    AUDDBG("e: mad: audmad_get_song_tuple\n");
    return tuple;
}

static Tuple *
audmad_get_song_tuple(char *filename)
{
    return __audmad_get_song_tuple(filename, NULL);
}

static Tuple *
audmad_probe_for_tuple(char *filename, VFSFile *fd)
{
    if (!audmad_is_our_fd(filename, fd))
        return NULL;

    aud_vfs_rewind(fd);

    return __audmad_get_song_tuple(filename, fd);
}

static gchar *fmts[] = { "mp3", "mp2", "mpg", "bmu", NULL };

InputPlugin mad_ip = {
    .description = "MPEG Audio Plugin",
    .init = audmad_init,
    .about = audmad_about,
    .configure = audmad_configure,
    .is_our_file = audmad_is_our_file,
    .play_file = audmad_play_file,
    .stop = audmad_stop,
    .pause = audmad_pause,
    .seek = audmad_seek,
    .cleanup = audmad_cleanup,
    .get_song_info = audmad_get_song_info,
    .get_song_tuple = audmad_get_song_tuple,
    .is_our_file_from_vfs = audmad_is_our_fd,
    .vfs_extensions = fmts,
    .mseek = audmad_mseek,
    .probe_for_tuple = audmad_probe_for_tuple,
    .update_song_tuple = audmad_update_song_tuple,
};

InputPlugin *madplug_iplist[] = { &mad_ip, NULL };

SIMPLE_INPUT_PLUGIN(madplug, madplug_iplist);

InputPlugin *mad_plugin = &mad_ip;
