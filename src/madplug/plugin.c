/*
 * mad plugin for audacious
 * Copyright (C) 2005-2009 William Pitcock, Yoshiki Yazawa, Eugene Zagidullin,
 *  John Lindgren
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
GMutex * mad_mutex, * control_mutex;
GMutex *pb_mutex;
GCond * mad_cond, * control_cond;

/*
 * static variables
 */
static struct mad_info_t info;   /**< info for current track */

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

typedef struct {
    gint version,
         layer,
         bitRate,
         sampleRate,
         size,
         lsf;
    gboolean hasCRC;
} mp3_frame_t;

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

    audmad_config->fast_play_time_calc = TRUE;
    audmad_config->use_xing = TRUE;
    audmad_config->sjis = FALSE;

    db = aud_cfg_db_open();
    if (db) {
        //metadata
        aud_cfg_db_get_bool(db, "MAD", "fast_play_time_calc",
                            &audmad_config->fast_play_time_calc);
        aud_cfg_db_get_bool(db, "MAD", "use_xing",
                            &audmad_config->use_xing);
        aud_cfg_db_get_bool(db, "MAD", "sjis", &audmad_config->sjis);

        aud_cfg_db_close(db);
    }

    mad_mutex = g_mutex_new();
    pb_mutex = g_mutex_new();
    mad_cond = g_cond_new();
    control_mutex = g_mutex_new ();
    control_cond = g_cond_new ();

    aud_mime_set_plugin("audio/mpeg", mad_plugin);
}

static void
audmad_cleanup()
{
    g_free(audmad_config);

    g_cond_free(mad_cond);
    g_mutex_free(mad_mutex);
    g_mutex_free(pb_mutex);
    g_mutex_free (control_mutex);
    g_cond_free (control_cond);
}

/* Validate a MPEG Audio header and extract some information from it.
 * References used:
 * http://www.mp3-tech.org/programmer/frame_header.html
 * http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 */
static gint
mp3_head_validate(guint32 head, mp3_frame_t *frame)
{
    gint bitIndex, sampleIndex, padding;

    /* bits 21-31 must be set (frame sync) */
    if ((head & 0xffe00000) != 0xffe00000)
        return -1;

    /* check for LSF */
    if ((head >> 20) & 1)
        frame->lsf = ((head >> 19) & 1) ? 0 : 1;
    else
        frame->lsf = 1;

    /* check if layer bits (17-18) are good */
    frame->layer = (head >> 17) & 3;
    if (frame->layer == 0)
        return -2; /* 00 = reserved */
    frame->layer = 4 - frame->layer;

    /* check CRC bit. if set, a 16-bit CRC follows header (not counted in frameSize!) */
    frame->hasCRC = (head >> 16) & 1;

    /* check if bitrate index bits (12-15) are acceptable */
    bitIndex = (head >> 12) & 0xf;

    /* 1111 and 0000 are reserved values for all layers */
    if (bitIndex == 0xf || bitIndex == 0)
        return -3;

    /* check samplerate index bits (10-11) */
    sampleIndex = (head >> 10) & 3;
    if (sampleIndex == 3)
        return -4;

    /* check version bits (19-20) and get bitRate */
    frame->version = (head >> 19) & 0x03;
    switch (frame->version) {
        case 0: /* 00 = MPEG Version 2.5 */
        case 2: /* 10 = MPEG Version 2 */
            if (frame->layer == 1)
                frame->bitRate = mp3_bitrate_table[3][bitIndex];
            else
                frame->bitRate = mp3_bitrate_table[4][bitIndex];
            break;

        case 1: /* 01 = reserved */
            return -5;

        case 3: /* 11 = MPEG Version 1 */
            frame->bitRate = mp3_bitrate_table[frame->layer - 1][bitIndex];
            break;

        default:
            return -6;
    }

    if (frame->bitRate < 0)
        return -7;

    /* check layer II restrictions vs. bitrate */
    if (frame->layer == 2) {
        gint chanMode = (head >> 6) & 0x3;

        if (chanMode == 0x3) {
            /* single channel with bitrate > 192 */
            if (frame->bitRate > 192)
                return -8;
        } else {
            /* any other mode with bitrates 32-56 and 80.
             * NOTICE! this check is not entirely correct, but I think
             * it is sufficient in most cases.
             */
            if (((frame->bitRate >= 32 && frame->bitRate <= 56) || frame->bitRate == 80))
                return -9;
        }
    }

    /* calculate approx. frame size */
    padding = (head >> 9) & 1;
    frame->sampleRate = mp3_samplerate_table[frame->version][sampleIndex];
    if (frame->sampleRate < 0)
        return -10;

    switch (frame->layer) {
        case 1:
            frame->size = ((12 * 1000 * frame->bitRate) / frame->sampleRate + padding) * 4;
            break;

        case 2:
            frame->size = (144 * 1000 * frame->bitRate) / frame->sampleRate + padding;
            break;

        case 3:
        default:
            frame->size = (144 * 1000 * frame->bitRate) / (frame->sampleRate << frame->lsf) + padding;
            break;
    }

    return 0;
}

static guint32
mp3_head_convert(const guchar * hbuf)
{
    return
        ((guint32) hbuf[0] << 24) |
        ((guint32) hbuf[1] << 16) |
        ((guint32) hbuf[2] << 8) |
        ((guint32) hbuf[3]);
}

#undef MADPROBE_DEBUG
//#define MADPROBE_DEBUG

#ifdef MADPROBE_DEBUG
static gchar *mp3_ver_table[4] = { "2.5", "INVALID", "2", "1" };
#define LOL(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define LOL(...) do { } while(0)
#endif

// audacious vfs fast version
static gint
audmad_is_our_fd(const gchar *filename, VFSFile *fin)
{
    gchar *ext = extname(filename);
    const gint max_resync_bytes = 32, max_resync_tries = 8;
    guint32 head = 0;
    guchar chkbuf[8192];
    gint state,
         tries = 0,
         chksize = 0,
         chkpos = 0,
         chkcount = 0,
         res, resync_max = -1,
         skip = 0;
    mp3_frame_t frame, prev = {0, 0, 0, 0, 0, 0, 0}; /* shut up warning */

    enum {
        STATE_HEADERS,
        STATE_VALIDATE,
        STATE_GOTO_NEXT,
        STATE_GET_NEXT,
        STATE_RESYNC,
        STATE_RESYNC_DO,
        STATE_FATAL
    };

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

    if ((chksize = aud_vfs_fread (chkbuf, 1, sizeof chkbuf, fin)) == 0)
    {
        g_message ("Rejecting %s; cannot read from file.", filename);
        return FALSE;
    }

    state = STATE_HEADERS;

    /* Check stream data for frame header(s). We employ a simple
     * state-machine approach here to find number of sequential
     * valid MPEG frame headers (with similar attributes).
     */
    do {
        switch (state) {
        case STATE_HEADERS:
            AUDDBG("check headers (size=%d, pos=%d)\n",  chksize, chkpos);
            /* Check read size */
            if (chksize - chkpos < 16) {
                AUDDBG("headers check failed, not enough data!\n");
                state = STATE_FATAL;
            } else {
                state = STATE_GET_NEXT;

                if (memcmp(&chkbuf[chkpos], "ID3", 3) == 0) {
                    /* Skip ID3 header */
                    guint tagsize = (chkbuf[chkpos+6] & 0x7f); tagsize <<= 7;
                    tagsize |= (chkbuf[chkpos+7] & 0x7f); tagsize <<= 7;
                    tagsize |= (chkbuf[chkpos+8] & 0x7f); tagsize <<= 7;
                    tagsize |= (chkbuf[chkpos+9] & 0x7f);

                    AUDDBG("ID3 size = %d\n", tagsize);
                    state = STATE_GOTO_NEXT;
                    skip = tagsize + 10;
                } else
                if (memcmp(&chkbuf[chkpos], "OggS", 4) == 0)
                    return 0;
                else
                if (memcmp(&chkbuf[chkpos], "RIFF", 4) == 0 &&
                    memcmp(&chkbuf[chkpos+8], "RMP3", 4) == 0)
                    return 1;
            }
            break;

        case STATE_VALIDATE:
            AUDDBG("validate %08x .. ", head);
            /* Check for valid header */
            if ((res = mp3_head_validate(head, &frame)) >= 0) {
                LOL("[is MPEG%s/layer %d, %dHz, %dkbps]",
                mp3_ver_table[frame.version], frame.layer, frame.sampleRate, frame.bitRate);
                state = STATE_GOTO_NEXT;
                skip = frame.size;
                chkcount++;
                if (chkcount > 1) {
                    if (frame.sampleRate != prev.sampleRate ||
                        frame.layer != prev.layer ||
                        frame.version != prev.version) {
                        /* Not similar frame... */
                        LOL(" .. but does not match (%d)!\n", chkcount);
                        state = STATE_RESYNC;
                    }
                    else if (chkcount >= 3)
                        return TRUE;
                } else {
                    /* First valid frame of sequence */
                    memcpy(&prev, &frame, sizeof(mp3_frame_t));
                    LOL(" .. first synced\n");
                }
            } else {
                /* Nope, try (re)synchronizing */
                if (chkcount > 1) {
                    LOL("no (%d), trying quick resync ..\n", res);
                    state = STATE_RESYNC_DO;
                    resync_max = max_resync_bytes;
                } else {
                    LOL("no (%d)\n", res);
                    state = STATE_RESYNC;
                }
            }
            break;

        case STATE_GOTO_NEXT:
            AUDDBG("goto next (cpos=%x, csiz=%d :: skip=%d :: fpos=%lx) ? ", chkpos, chksize, skip, aud_vfs_ftell(fin));
            /* Check if we have the next possible header in buffer? */
            gint tmppos = chkpos + skip + 16;
            if (tmppos < chksize) {
                LOL("[in buffer]\n");
                chkpos += skip;
                state = STATE_GET_NEXT;
            } else {
                g_message ("Rejecting %s: out of data.", filename);
                return FALSE;
            }
            break;

        case STATE_GET_NEXT:
            /* Get a header */
            AUDDBG("get next @ chkpos=%08x\n", chkpos);
            head = mp3_head_convert(&chkbuf[chkpos]);
            state = STATE_VALIDATE;
            break;

        case STATE_RESYNC:
            AUDDBG("resyncing try #%d ..\n", tries);
            /* Re-synchronize aka try to find a valid header */
            head = 0;
            chkcount = 0;
            resync_max = -1;
            state = STATE_RESYNC_DO;
            tries++;
            break;

        case STATE_RESYNC_DO:
            /* Scan for valid frame header */
            for (; chkpos < chksize; chkpos++) {
                head <<= 8;
                head |= chkbuf[chkpos];

                if (mp3_head_validate(head, &frame) >= 0) {
                    /* Found, exit resync */
                    chkpos -= 3;
                    AUDDBG("resync found @ %x\n", chkpos);
                    state = STATE_VALIDATE;
                    break;
                }

                /* Check for maximum bytes to search */
                if (resync_max > 0) {
                    resync_max--;
                    if (resync_max == 0) {
                        state = STATE_RESYNC;
                        break;
                    }
                }
            }

            if (state == STATE_RESYNC_DO) /* reached end of data */
                state = STATE_FATAL;

            break;
        }
    } while (state != STATE_FATAL && tries < max_resync_tries);
    /* Give up after given number of failed resync attempts or fatal error */

    return 0;
}

static void audmad_stop (InputPlayback * playback)
{
    g_mutex_lock (control_mutex);
    info.playback->playing = FALSE;
    g_cond_signal (control_cond);
    g_mutex_unlock (control_mutex);
    g_thread_join (playback->thread);
    playback->thread = NULL;

    input_term (& info);
}

static void
audmad_play_file(InputPlayback *playback)
{
    gchar *url = playback->filename;

#ifdef DEBUG
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

    if (! input_get_info (& info))
    {
        g_warning ("Unable to get info for %s.", playback->filename);
        input_term (& info);
        return;
    }

    mowgli_object_ref (info.tuple);
    playback->set_tuple (playback, info.tuple);
    playback->set_params (playback, NULL, 0, info.bitrate, info.freq,
     info.channels);

    info.seek = -1;
    info.pause = FALSE;

    g_mutex_lock(pb_mutex);
    info.playback = playback;
    info.playback->playing = TRUE;
    g_mutex_unlock(pb_mutex);

    playback->set_pb_ready(playback);
    decode_loop(&info);
    input_term (& info);
}

static void audmad_pause (InputPlayback * playback, short paused)
{
    g_mutex_lock (control_mutex);

    if (playback->playing)
    {
        info.pause = paused;
        g_cond_signal (control_cond);
        g_cond_wait (control_cond, control_mutex);
    }

    g_mutex_unlock (control_mutex);
}

static void audmad_mseek (InputPlayback * playback, gulong millisecond)
{
    g_mutex_lock (control_mutex);

    if (playback->playing)
    {
        info.seek = millisecond;
        g_cond_signal (control_cond);
        g_cond_wait (control_cond, control_mutex);
    }

    g_mutex_unlock (control_mutex);
}

static void audmad_seek (InputPlayback * playback, gint time)
{
    audmad_mseek (playback, time * 1000);
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
 * Direct interface to show given error message.
 */
void
audmad_error(gchar *format, ...)
{
    va_list args;
    gchar *msg = NULL;

    va_start (args, format);
    msg = g_markup_vprintf_escaped (format, args);
    va_end (args);

    aud_event_queue_with_data_free("interface show error", msg);
}

static Tuple * audmad_probe_for_tuple (const gchar * filename, VFSFile * handle)
{
    struct mad_info_t info;
    Tuple * tuple;

    aud_vfs_fseek (handle, 0, SEEK_SET);

    if (! input_init (& info, filename, handle))
        return NULL;

    if (! input_get_info (& info))
    {
        input_term (& info);
        return NULL;
    }

    tuple = info.tuple;
    mowgli_object_ref (tuple);
    input_term (& info);
    return tuple;
}

static Tuple * audmad_get_song_tuple (const gchar * filename)
{
    VFSFile * handle;
    Tuple * tuple;

    if ((handle = aud_vfs_fopen (filename, "r")) == NULL)
    {
        g_warning ("Cannot open %s.\n", filename);
        return NULL;
    }

    tuple = audmad_probe_for_tuple (filename, handle);
    aud_vfs_fclose (handle);
    return tuple;
}

static const gchar *fmts[] = { "mp3", "mp2", "mpg", "bmu", NULL };

extern PluginPreferences preferences;

InputPlugin mad_ip = {
    .description = "MPEG Audio Plugin",
    .init = audmad_init,
    .about = audmad_about,
    .settings = &preferences,
    .play_file = audmad_play_file,
    .stop = audmad_stop,
    .pause = audmad_pause,
    .seek = audmad_seek,
    .cleanup = audmad_cleanup,
    .get_song_tuple = audmad_get_song_tuple,
    .is_our_file_from_vfs = audmad_is_our_fd,
    .vfs_extensions = (gchar**)fmts,
    .mseek = audmad_mseek,
    .probe_for_tuple = audmad_probe_for_tuple,
    .update_song_tuple = audmad_update_song_tuple,
};

InputPlugin *madplug_iplist[] = { &mad_ip, NULL };

SIMPLE_INPUT_PLUGIN (madplug, madplug_iplist)

InputPlugin *mad_plugin = &mad_ip;
