/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
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
#include "input.h"

#include <math.h>

#include <gtk/gtk.h>
#include <audacious/util.h>
#include <audacious/configdb.h>
#include <stdarg.h>
#include <fcntl.h>
#include <audacious/vfs.h>
#include <sys/stat.h>

/*
 * Global variables
 */
struct audmad_config_t audmad_config;   /**< global configuration */
InputPlugin *mad_plugin = NULL;
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

/*
 * Function extname (filename)
 *
 *    Return pointer within filename to its extenstion, or NULL if
 *    filename has no extension.
 *
 */
static gchar *extname(const char *filename)
{
    gchar *ext = strrchr(filename, '.');

    if (ext != NULL)
        ++ext;

    return ext;
}


void audmad_config_compute(struct audmad_config_t *config)
{
    /* set some config parameters by parsing text fields 
       (RG default gain, etc..)
     */
    const gchar *text;
    gdouble x;

    text = config->pregain_db;
    x = g_strtod(text, NULL);
    config->pregain_scale = (x != 0) ? pow(10.0, x / 20) : 1;
#ifdef DEBUG
    g_message("pregain=[%s] -> %g  -> %g", text, x, config->pregain_scale);
#endif
    text = config->replaygain.default_db;
    x = g_strtod(text, NULL);
    config->replaygain.default_scale = (x != 0) ? pow(10.0, x / 20) : 1;
#ifdef DEBUG
    g_message("RG.default=[%s] -> %g  -> %g", text, x,
              config->replaygain.default_scale);
#endif
}

static void audmad_init()
{
    ConfigDb *db = NULL;

    audmad_config.fast_play_time_calc = TRUE;
    audmad_config.use_xing = TRUE;
    audmad_config.dither = TRUE;
    audmad_config.sjis = FALSE;
    audmad_config.pregain_db = "+0.00";
    audmad_config.replaygain.enable = TRUE;
    audmad_config.replaygain.track_mode = FALSE;
    audmad_config.hard_limit = FALSE;
    audmad_config.replaygain.default_db = "-9.00";

    db = bmp_cfg_db_open();
    if (db) {
        bmp_cfg_db_get_bool(db, "MAD", "fast_play_time_calc",
                            &audmad_config.fast_play_time_calc);
        bmp_cfg_db_get_bool(db, "MAD", "use_xing",
                            &audmad_config.use_xing);
        bmp_cfg_db_get_bool(db, "MAD", "dither", &audmad_config.dither);
        bmp_cfg_db_get_bool(db, "MAD", "sjis", &audmad_config.sjis);
        bmp_cfg_db_get_bool(db, "MAD", "hard_limit",
                            &audmad_config.hard_limit);
        bmp_cfg_db_get_string(db, "MAD", "pregain_db",
                              &audmad_config.pregain_db);
        bmp_cfg_db_get_bool(db, "MAD", "RG.enable",
                            &audmad_config.replaygain.enable);
        bmp_cfg_db_get_bool(db, "MAD", "RG.track_mode",
                            &audmad_config.replaygain.track_mode);
        bmp_cfg_db_get_string(db, "MAD", "RG.default_db",
                              &audmad_config.replaygain.default_db);
        bmp_cfg_db_get_bool(db, "MAD", "title_override",
                            &audmad_config.title_override);
        bmp_cfg_db_get_string(db, "MAD", "id3_format",
                              &audmad_config.id3_format);

        bmp_cfg_db_close(db);
    }

    mad_mutex = g_mutex_new();
    pb_mutex = g_mutex_new();
    mad_cond = g_cond_new();
    audmad_config_compute(&audmad_config);

}

static void audmad_cleanup()
{
}

static gboolean mp3_head_check(guint32 head)
{
    /*
     * First two bytes must be a sync header (11 bits all 1)
     * http://www.mp3-tech.org/programmer/frame_header.html
     */
    if ((head & 0xffe00000) != 0xffe00000)
        return FALSE;

    /* check if bits 18 and 19 are set */
    if (!((head >> 17) & 3))
        return FALSE;

    /* check if bits 13 - 16 are all set */
    if (((head >> 12) & 0xf) == 0xf)
        return FALSE;

    /* check if bits 13 - 16 are all not set */
    if (!((head >> 12) & 0xf))
        return FALSE;

    /* check if bit 11 and 12 are both set */
    if (((head >> 10) & 0x3) == 0x3)
        return FALSE;

    /* check if bits 17 - 20 are all set */
    if (((head >> 19) & 1) == 1 &&
        ((head >> 17) & 3) == 3 && ((head >> 16) & 1) == 1)
        return FALSE;

    /* not sure why we check this, but ok! */
    if ((head & 0xffff0000) == 0xfffe0000)
        return FALSE;

    return TRUE;
}

static int mp3_head_convert(const guchar * hbuf)
{
    return ((unsigned long) hbuf[0] << 24) |
        ((unsigned long) hbuf[1] << 16) |
        ((unsigned long) hbuf[2] << 8) | (unsigned long) hbuf[3];
}


// audacious vfs fast version
static int audmad_is_our_fd(char *filename, VFSFile *fin)
{
    guint32 check;
    gchar *ext = extname(filename);
    gint cyc = 0;
    guchar buf[4];
    guchar tmp[4096];
    gint ret, i;

    info.remote = FALSE;

#if 1
    // XXX: temporary fix
    if (!strncasecmp("http://", filename, 7) || !strncasecmp("https://", filename, 8)) {
        g_message("audmad_is_our_fd: remote");
        info.remote = TRUE;
    }
#endif

    /* I've seen some flac files beginning with id3 frames..
       so let's exclude known non-mp3 filename extensions */
    if (!strcasecmp(".flac", ext) || !strcasecmp(".mpc", ext) ||
        !strcasecmp(".tta", ext))
        return 0;

    if (fin == NULL)
        return FALSE;

    vfs_fread(buf, 1, 4, fin);

    check = mp3_head_convert(buf);

    if (memcmp(buf, "ID3", 3) == 0)
        return 1;
    else if (memcmp(buf, "RIFF", 4) == 0)
    {
        vfs_fseek(fin, 4, SEEK_CUR);
        vfs_fread(buf, 1, 4, fin);

        if (memcmp(buf, "RMP3", 4) == 0)
            return 1;
    }

    while (!mp3_head_check(check))
    {
        ret = vfs_fread(tmp, 1, 4096, fin);
        if (ret == 0)
            return 0;

        for (i = 0; i < ret; i++)
        {
            check <<= 8;
            check |= tmp[i];

            if (mp3_head_check(check))
                return 1;
        }

        if (++cyc > 1024)
            return 0;
    }

    return 1;
}

// audacious vfs version
static int audmad_is_our_file(char *filename)
{
    VFSFile *fin = NULL;
    gint rtn;

    fin = vfs_fopen(filename, "rb");

    if (fin == NULL)
        return 0;

    rtn = audmad_is_our_fd(filename, fin);
    vfs_fclose(fin);

    return rtn;
}

static void audmad_stop(InputPlayback *playback)
{
#ifdef DEBUG
    g_message("f: audmad_stop");
#endif
    g_mutex_lock(mad_mutex);
    info.playback = playback;
    g_mutex_unlock(mad_mutex);

    if (decode_thread) {

        g_mutex_lock(mad_mutex);
        info.playback->playing = 0;
        g_mutex_unlock(mad_mutex);
        g_cond_signal(mad_cond);

#ifdef DEBUG
        g_message("waiting for thread");
#endif
        g_thread_join(decode_thread);
#ifdef DEBUG
        g_message("thread done");
#endif
        input_term(&info);
        decode_thread = NULL;

    }
#ifdef DEBUG
    g_message("e: audmad_stop");
#endif
}

static void audmad_play_file(InputPlayback *playback)
{
    gboolean rtn;
    gchar *url = playback->filename;

#ifdef DEBUG
    g_message("playing %s", url);
#endif                          /* DEBUG */

    if (input_init(&info, url) == FALSE) {
        g_message("error initialising input");
        return;
    }

    rtn = input_get_info(&info, audmad_config.fast_play_time_calc);

    if (rtn == FALSE) {
        g_message("error reading input info");
        return;
    }
    g_mutex_lock(pb_mutex);
    info.playback = playback;
    info.playback->playing = 1;
    g_mutex_unlock(pb_mutex);

    decode_thread = g_thread_create(decode_loop, (void *) &info, TRUE, NULL);
}

static void audmad_pause(InputPlayback *playback, short paused)
{
    g_mutex_lock(pb_mutex);
    info.playback = playback;
    g_mutex_unlock(pb_mutex);
    playback->output->pause(paused);
}

static void audmad_seek(InputPlayback *playback, int time)
{
    g_mutex_lock(pb_mutex);
    info.playback = playback;
    /* xmms gives us the desired seek time in seconds */
    info.seek = time;
    g_mutex_unlock(pb_mutex);

}

/**
 * Scan the given file or URL.
 * Fills in the title string and the track length in milliseconds.
 */
static void
audmad_get_song_info(char *url, char **title, int *length)
{
    struct mad_info_t myinfo;
#ifdef DEBUG
    g_message("f: audmad_get_song_info: %s", url);
#endif                          /* DEBUG */

    if (input_init(&myinfo, url) == FALSE) {
        g_message("error initialising input");
        return;
    }

    // don't try to get from stopped stream.
    if(myinfo.remote && (!myinfo.playback || !myinfo.playback->playing)){
        g_print("get_song_info: remote!\n");
        return;
    }

    if (input_get_info(&myinfo, info.remote ? TRUE : audmad_config.fast_play_time_calc) == TRUE) {
        if(myinfo.tuple->track_name)
            *title = strdup(myinfo.tuple->track_name);
        else
            *title = strdup(url);
        *length = mad_timer_count(myinfo.duration, MAD_UNITS_MILLISECONDS);
    }
    else {
        *title = strdup(url);
        *length = -1;
    }
    input_term(&myinfo);
#ifdef DEBUG
    g_message("e: audmad_get_song_info");
#endif                          /* DEBUG */
}

static void audmad_about()
{
    static GtkWidget *aboutbox;
    gchar *scratch;

    if (aboutbox != NULL)
        return;

    scratch = g_strdup_printf(
	"Audacious MPEG Audio Plugin\n"
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
	"    Samuel Krempp",
	MAD_VERSION_MAJOR, MAD_VERSION_MINOR, MAD_VERSION_PATCH,
	MAD_VERSION_EXTRA);

    aboutbox = xmms_show_message("About MPEG Audio Plugin",
                                 scratch,
                                 "Ok", FALSE, NULL, NULL);

    g_free(scratch);

    g_signal_connect(G_OBJECT(aboutbox), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &aboutbox);
}

/**
 * Display a GTK box containing the given error message.
 * Taken from mpg123 plugin.
 */
void audmad_error(char *error, ...)
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
            xmms_show_message("Error", string, "Ok", FALSE, 0, 0);
        gtk_signal_connect(GTK_OBJECT(error_dialog), "destroy",
                           GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                           &error_dialog);
        GDK_THREADS_LEAVE();
    }
#endif                          /* !NOGUI */
}

extern void audmad_get_file_info(char *filename);
extern void audmad_configure();


// tuple stuff
static TitleInput *audmad_get_song_tuple(char *filename)
{
    TitleInput *tuple = NULL;
    gchar *string = NULL;
    VFSFile *file;

    struct id3_file *id3file = NULL;
    struct id3_tag *tag = NULL;

#ifdef DEBUG
    string = str_to_utf8(filename);
    g_message("f: mad: audmad_get_song_tuple: %s", string);
    g_free(string);
    string = NULL;
#endif

    if(info.remote){
        if(info.playback && info.playback->playing) {
            tuple = bmp_title_input_new();
#ifdef DEBUG
            printf("info.playback->playing = %d\n",info.playback->playing);
#endif
            tuple->track_name = vfs_get_metadata(info.infile, "track-name");
            tuple->album_name = vfs_get_metadata(info.infile, "stream-name");
#ifdef DEBUG
            printf("audmad_get_song_tuple: track_name = %s\n", tuple->track_name);
            printf("audmad_get_song_tuple: stream_name = %s\n", tuple->album_name);
#endif
            tuple->file_name = g_path_get_basename(filename);
            tuple->file_path = g_path_get_dirname(filename);
            tuple->file_ext = extname(filename);
            tuple->length = -1;
            tuple->mtime = 0; // this indicates streaming
#ifdef DEBUG
            printf("get_song_tuple remote: tuple\n");
#endif
            return tuple;
        }
#ifdef DEBUG
        printf("get_song_tuple: remote: NULL\n");
#endif
        return NULL;
    }

    if ((file = vfs_fopen(filename, "rb")) != NULL) {

        tuple = bmp_title_input_new();
        id3file = id3_file_open(filename, ID3_FILE_MODE_READONLY);

        if (id3file) {
            tag = id3_file_tag(id3file);

            if (tag) {
                tuple->performer =
                    input_id3_get_string(tag, ID3_FRAME_ARTIST);
                tuple->album_name =
                    input_id3_get_string(tag, ID3_FRAME_ALBUM);
                tuple->track_name =
                    input_id3_get_string(tag, ID3_FRAME_TITLE);

                // year
                string = NULL;
                string = input_id3_get_string(tag, ID3_FRAME_YEAR); //TDRC
                if (!string)
                    string = input_id3_get_string(tag, "TYER");

                if (string) {
                    tuple->year = atoi(string);
                    g_free(string);
                    string = NULL;
                }

                tuple->file_name = g_path_get_basename(filename);
                tuple->file_path = g_path_get_dirname(filename);
                tuple->file_ext = extname(filename);

                // length
                {
                    char *dummy = NULL;
                    int length = 0;
                    audmad_get_song_info(filename, &dummy, &length);
                    tuple->length = length;
                    g_free(dummy);
                }

                // track number
                string = input_id3_get_string(tag, ID3_FRAME_TRACK);
                if (string) {
                    tuple->track_number = atoi(string);
                    g_free(string);
                    string = NULL;
                }
                // genre
                tuple->genre = input_id3_get_string(tag, ID3_FRAME_GENRE);
#ifdef DEBUG
                g_message("genre = %s", tuple->genre);
#endif
                // comment
                tuple->comment =
                    input_id3_get_string(tag, ID3_FRAME_COMMENT);

            }
            id3_file_close(id3file);
        }
        else {
            tuple->file_name = g_path_get_basename(filename);
            tuple->file_path = g_path_get_dirname(filename);
            tuple->file_ext = extname(filename);
            // length
            {
                char *dummy = NULL;
                int length = 0;
                audmad_get_song_info(filename, &dummy, &length);
                tuple->length = length;
                g_free(dummy);
            }
        }
        vfs_fclose(file);
    }
#ifdef DEBUG
    g_message("e: mad: audmad_get_song_tuple");
#endif
    return tuple;

}


/**
 * Retrieve meta-information about URL.
 * For local files this means ID3 tag etc.
 */
gboolean mad_get_info(struct mad_info_t * info, gboolean fast_scan)
{
    TitleInput *tuple = NULL;

#ifdef DEBUG
    g_message("f: mad_get_info: %s", info->filename);
#endif
    if (info->remote) {
        return TRUE;
    }

    tuple = audmad_get_song_tuple(info->filename);
    info->title = xmms_get_titlestring(audmad_config.title_override == TRUE ?
	audmad_config.id3_format : xmms_get_gentitle_format(), tuple);

    /* scan mp3 file, decoding headers unless fast_scan is set */
    if (scan_file(info, fast_scan) == FALSE)
        return FALSE;

    /* reset the input file to the start */
    vfs_rewind(info->infile);
    info->offset = 0;

    /* use the filename for the title as a last resort */
    if (!info->title)
    {
        char *pos = strrchr(info->filename, '/');
        if (pos)
            info->title = g_strdup(pos + 1);
        else
            info->title = g_strdup(info->filename);
     }

#ifdef DEBUG
    g_message("e: mad_get_info");
#endif
    return TRUE;
}

static gchar *fmts[] = { "mp3", "mp2", "mpg", NULL };

InputPlugin *get_iplugin_info(void)
{
    if (mad_plugin != NULL)
        return mad_plugin;

    mad_plugin = g_new0(InputPlugin, 1);
    mad_plugin->description = g_strdup(_("MPEG Audio Plugin"));
    mad_plugin->init = audmad_init;
    mad_plugin->about = audmad_about;
    mad_plugin->configure = audmad_configure;
    mad_plugin->is_our_file = audmad_is_our_file;
    mad_plugin->play_file = audmad_play_file;
    mad_plugin->stop = audmad_stop;
    mad_plugin->pause = audmad_pause;
    mad_plugin->seek = audmad_seek;
    mad_plugin->cleanup = audmad_cleanup;
    mad_plugin->get_song_info = audmad_get_song_info;
    mad_plugin->file_info_box = audmad_get_file_info;
    mad_plugin->get_song_tuple = audmad_get_song_tuple;
    mad_plugin->is_our_file_from_vfs = audmad_is_our_fd;
    mad_plugin->vfs_extensions = fmts;

    return mad_plugin;
}
