/*
 *  XMMS Crossfade Plugin
 *  Copyright (C) 2000-2007  Peter Eisenlohr <peter@eisenlohr.org>
 *
 *  based on the original OSS Output Plugin
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */
/* indent -i8 -ts8 -hnl -bli0 -l128 -npcs -cli8 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "crossfade.h"
#include "cfgutil.h"

#include "configure.h"

#include "interface-2.0.h"
#include "support-2.0.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#undef  DEBUG_HARDCORE

/* output plugin callback prototypes */
static OutputPluginInitStatus xfade_init();
static void xfade_cleanup();  /* audacious and patched only */
static void xfade_set_volume(int l, int r);
static void xfade_get_volume(int *l, int *r);
static gint xfade_open_audio(AFormat fmt, int rate, int nch);
static void xfade_write_audio(void *ptr, int length);
static void xfade_close_audio();
static void xfade_flush(int time);
static void xfade_pause(short paused);
static gint xfade_buffer_free();
static gint xfade_buffer_playing();
static gint xfade_written_time();
static gint xfade_output_time();

/* output plugin callback table (extended, needs patched player) */
static struct
{
    OutputPlugin xfade_op;
        void (*cleanup) (void);
}
xfade_op_private =
{
    {
        .description    = "Crossfade Plugin",
        .init           = xfade_init,
        .cleanup        = xfade_cleanup,
        .get_volume     = xfade_get_volume,
        .set_volume     = xfade_set_volume,
        .open_audio     = xfade_open_audio,
        .write_audio    = xfade_write_audio,
        .close_audio    = xfade_close_audio,
        .flush          = xfade_flush,
        .pause          = xfade_pause,
        .buffer_free    = xfade_buffer_free,
        .buffer_playing = xfade_buffer_playing,
        .output_time    = xfade_output_time,
        .written_time   = xfade_written_time,
        .about          = xfade_about,
        .configure      = xfade_configure,
        .probe_priority = 0
    },
    NULL
};

static OutputPlugin *xfade_op = &xfade_op_private.xfade_op;

static OutputPlugin *xfade_oplist[] = { &xfade_op_private.xfade_op, NULL };
DECLARE_PLUGIN(crossfade, NULL, NULL, NULL, xfade_oplist, NULL, NULL, NULL);

/* internal prototypes */
static gint open_output();
static void buffer_reset(buffer_t *buf, config_t *cfg);
static void *buffer_thread_f(void *arg);
static void sync_output();
static gint calc_bitrate();

/* This function has been stolen from libxmms/util.c. */
void xfade_usleep(gint usec)
{
#if defined(HAVE_G_USLEEP)
    g_usleep(usec);
#elif defined(HAVE_NANOSLEEP)
    struct timespec req;

    req.tv_sec = usec / 1000000;
    usec -= req.tv_sec * 1000000;
    req.tv_nsec = usec * 1000;

    nanosleep(&req, NULL);
#else
    struct timeval tv;

    tv.tv_sec = usec / 1000000;
    usec -= tv.tv_sec * 1000000;
    tv.tv_usec = usec;
    select(0, NULL, NULL, NULL, &tv);
#endif
}


/* local variables */
static gboolean realtime;
static gboolean is_http;

static gint64 streampos;    /* position within current song (input bps) */
static gboolean playing;
gboolean opened;        /* TRUE between open_audio() and close_audio() */
static gboolean paused;     /* TRUE: no playback (but still filling buffer) */
static gboolean stopped;    /* TRUE: stop buffer thread ASAP */
static gboolean eop;        /* TRUE: wait until buffer is empty then sync() */

static plugin_config_t the_op_config = DEFAULT_OP_CONFIG;
       OutputPlugin          *the_op = NULL;

static gboolean input_playing = FALSE;

       gboolean output_opened     = FALSE;
static gint     output_flush_time = 0;
       gint     output_offset     = 0;
static gint64   output_written    = 0;
       gint64   output_streampos  = 0;

static gchar zero_4k[4096];

/*
 *  Available fade configs:
 *
 *  fc_start:       First song, only in_len and in_level are used
 *  fc_xfade:       Automatic crossfade at end of song
 *  fc_album:       Like xfade but for consecutive songs of the same album
 *  fc_manual:      Manual crossfade (triggered by Next or Prev)
 *  fc_stop:        Last song, only out_len and out_level are used
 *  fc_eop:         Last song, only out_len and out_level are used
 *
 *  NOTE: As of version 0.2 of xmms-crossfade,
 *        only xfade and manual are implemented.
 *
 *        With version 0.2.3, fc_album has been added.
 *
 *        With 0.2.4, all configs are implemented:
 *
 *  Available parameters:
 *
 *              | start | xfade | manual | album | stop |  eop
 *  ------------+-------+-------+--------+-------+------+------
 *  in_len      |   yes |   yes |    yes |    no |   no |   no
 *  in_volume   |   yes |   yes |    yes |    no |   no |   no
 *  offset      |    no |   yes |    yes |    no | +yes | +yes
 *  out_len     |    no |   yes |    yes |    no |  yes |  yes
 *  out_volume  |    no |   yes |    yes |    no |  yes |  yes
 *  flush (*)   |    no |    no |    yes |    no |  yes |   no
 *  ------------+-------+-------+--------+-------+------+------
 *
 *  Parameters marked with (*) are not configureable by the user
 *
 *  The offset parameters for 'stop' and 'eop' is used to store the
 *  length of the additional silence to be added. It may be >= 0 only.
 *
 */

static struct timeval last_close;
static struct timeval last_write;

static gchar *last_filename = NULL;

static format_t  in_format;

static buffer_t            the_buffer;
       buffer_t *buffer = &the_buffer;

static THREAD buffer_thread;
       MUTEX  buffer_mutex = MUTEX_INITIALIZER;

static config_t     the_config;
       config_t        *config = &the_config;
       config_t config_default = CONFIG_DEFAULT;

static fade_config_t *fade_config = NULL;

/* this is the entry point for XMMS */
OutputPlugin *
get_oplugin_info()
{
    return xfade_op;
}

OutputPlugin *
get_crossfade_oplugin_info()
{
    return xfade_op;
}

static gboolean
open_output_f(gpointer data)
{
    AUDDBG("[crossfade] open_output_f: pid=%d\n", getpid());
    open_output();
    return FALSE;       /* FALSE = 'do not call me again' */
}

void
xfade_realize_config()      /* also called by xfade_init() */
{
    /* 0.3.0: keep device opened */
    if (config->output_keep_opened && !output_opened)
    {
        AUDDBG("[crossfade] realize_config: keeping output opened...\n");

        /* 0.3.1: HACK: this will make sure that we start playing silence after startup */
        gettimeofday(&last_close, NULL);

        /* 0.3.1: HACK: Somehow, if we open output here at XMMS startup, there
           will be leftover filedescriptors later when closing output again.
           Opening output in a timeout function seems to work around this... */
        AUDDBG("[crossfade] realize_config: adding timeout (pid=%d)\n", (int) getpid());
        g_timeout_add(0, open_output_f, NULL);
    }
}

static gint
output_list_f(gconstpointer a, gconstpointer b)
{
    OutputPlugin *op = (OutputPlugin *) a;
    gchar *name = (gchar *) b;

    return strcmp(g_basename(op->filename), name);
}

static OutputPlugin *
find_output()
{
    GList *list, *element;
    OutputPlugin *op = NULL;

    /* find output plugin */
    {
        if (config->op_name && (list = xfplayer_get_output_list()))
            if ((element = g_list_find_custom(list, config->op_name, output_list_f)))
                op = element->data;

        if (op == xfade_op)
        {
            AUDDBG("[crossfade] find_output: can't use myself as output plugin!\n");
            op = NULL;
        }
        else if (!op)
        {
            AUDDBG("[crossfade] find_output: could not find output plugin \"%s\"\n",
                   config->op_name ? config->op_name : "#NULL#");
        }
        else        /* ok, we have a plugin. last, get its compatibility options */
            xfade_load_plugin_config(config->op_config_string, config->op_name, &the_op_config);
    }

    if (op != NULL && op->init () != OUTPUT_PLUGIN_INIT_FOUND_DEVICES)
    {
        fprintf (stderr, "crossfade: %s failed to initialize.\n",
         op->description);
        op = NULL;
    }

    return op;
}

static gint
open_output()
{
    /* sanity check */
    if (output_opened)
        AUDDBG("[crossfade] open_output: WARNING: output_opened=TRUE!\n");

    /* reset output_* */
    output_opened = FALSE;
    output_flush_time = 0;
    output_offset     = 0;
    output_written    = 0;
    output_streampos  = 0;

    /* get output plugin (this will also init the_op_config) */
    if (!(the_op = find_output()))
    {
        fprintf (stderr, "[crossfade] open_output: could not find any output!\n");
        return -1;
    }

    /* print output plugin info */
    AUDDBG("[crossfade] open_output: using \"%s\" for output", the_op->description ? the_op->description : "#NULL#");

    if (realtime)
        AUDDBG(" (RT)");

    if (the_op_config.throttle_enable)
        AUDDBG(realtime ? " (throttled (disabled with RT))" : " (throttled)");

    if (the_op_config.max_write_enable)
        AUDDBG(" (max_write=%d)", the_op_config.max_write_len);

    AUDDBG("\n");

    /* open plugin */
    if (!the_op->open_audio(in_format.fmt, in_format.rate, in_format.nch))
    {
        fprintf (stderr, "[crossfade] open_output: open_audio() failed!\n");
        the_op->cleanup ();
        the_op = NULL;
        return -1;
    }

    /* clear buffer struct */
    memset(buffer, 0, sizeof(*buffer));

    /* calculate buffer size */
    buffer->mix_size     = MS2B(xfade_mix_size_ms(config)) & -4;
    buffer->sync_size    = MS2B(config->sync_size_ms)      & -4;
    buffer->preload_size = MS2B(config->preload_size_ms)   & -4;

    buffer->size = (buffer->mix_size +      /* mixing area */
                    buffer->sync_size +     /* additional sync */
                    buffer->preload_size);  /* preload */

    AUDDBG("[crossfade] open_output: buffer: size=%d (%d+%d+%d=%d ms) (%d Hz)\n",
           buffer->size,
           B2MS(buffer->mix_size),
           B2MS(buffer->preload_size),
           B2MS(buffer->sync_size),
           B2MS(buffer->size),
           in_format.rate);

    /* allocate buffer */
    if (!(buffer->data = g_malloc0(buffer->size)))
    {
        fprintf (stderr, "[crossfade] open_output: error allocating buffer!\n");
        the_op->close_audio();
        the_op->cleanup ();
        the_op = NULL;
        return -1;
    }

    /* reset buffer */
    buffer_reset(buffer, config);

    /* make sure stopped is TRUE -- otherwise the buffer thread would
     * stop again immediatelly after it has been started. */
    stopped = FALSE;

    /* create and run buffer thread */
    if (THREAD_CREATE(buffer_thread, buffer_thread_f))
    {
        PERROR("[crossfade] open_output: thread_create()");
        g_free(buffer->data);
        the_op->close_audio();
        the_op->cleanup ();
        the_op = NULL;
        return -1;
    }
    SCHED_YIELD;

    /* done */
    output_opened = TRUE;
    return 0;
}

static OutputPluginInitStatus
xfade_init()
{
    /* load config */
    memset(config, 0, sizeof(*config));
    *config = config_default;
    xfade_load_config();

    /* set default strings if there is no existing config */
    if (!config->op_config_string)     config->op_config_string     = g_strdup(DEFAULT_OP_CONFIG_STRING);
    if (!config->op_name)              config->op_name              = g_strdup(DEFAULT_OP_NAME);

    /* check for realtime priority, it needs some special attention */
    realtime = xfplayer_check_realtime_priority();

    /* reset */
    stopped = FALSE;

    /* find current output plugin early so that volume control works
     * even if playback has not started yet. */
    if (!(the_op = find_output()))
        AUDDBG("[crossfade] init: could not find any output!\n");

    /* realize config -- will also setup the pre-mixing effect plugin */
    xfade_realize_config();

    return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}

void
xfade_get_volume(int *l, int *r)
{
    if (config->mixer_software)
    {
        *l = config->mixer_reverse
           ? config->mixer_vol_right
           : config->mixer_vol_left;
        *r = config->mixer_reverse
           ? config->mixer_vol_left
           : config->mixer_vol_right;
    }
    else
    {
        if (the_op && the_op->get_volume)
        {
            if (config->mixer_reverse)
                the_op->get_volume(r, l);
            else
                the_op->get_volume(l, r);
        }
    }

    /* AUDDBG("[crossfade] xfade_get_volume: l=%d r=%d\n", *l, *r); */
}

void
xfade_set_volume(int l, int r)
{
    /* AUDDBG("[crossfade] xfade_set_volume: l=%d r=%d\n", l, r); */

    if (!config->enable_mixer)
        return;

    if (the_op && the_op->set_volume)
    {
        if (config->mixer_reverse)
            the_op->set_volume(r, l);
        else
            the_op->set_volume(l, r);
    }
}

/*** buffer stuff ***********************************************************/

static void
buffer_mfg_reset(buffer_t *buf, config_t *cfg)
{
    buf->mix        = 0;
    buf->fade       = 0;
    buf->gap        = (cfg->gap_lead_enable ? MS2B(cfg->gap_lead_len_ms) & -4 : 0);
    buf->gap_len    = buf->gap;
    buf->gap_level  = cfg->gap_lead_level;
    buf->gap_killed = 0;
    buf->skip       = 0;
    buf->skip_len   = 0;
}

static void
buffer_reset(buffer_t *buf, config_t *cfg)
{
    buffer_mfg_reset(buf, cfg);

    buf->rd_index = 0;
    buf->used     = 0;
    buf->preload  = buf->preload_size;

    buf->silence     =  0;
    buf->silence_len =  0;
    buf->reopen      = -1;
    buf->pause       = -1;
}

/****************************************************************************/

static void
xfade_apply_fade_config(fade_config_t *fc)
{
    gint out_skip, in_skip;
    gint avail, out_len, in_len, offset, preload;
    gint index, length, fade, n;
    gfloat out_scale, in_scale;
    gboolean out_skip_clipped = FALSE;
    gboolean out_len_clipped = FALSE;
    gboolean offset_clipped = FALSE;

    /* Overwrites mix and fade; may add silence */

    /*
     * Example 1: offset < 0 --> mix streams together
     * Example 2: offset > 0 --> insert pause between streams
     *
     *     |----- out_len -----|    *     |out_len|
     *     |                   |    *     |       |
     * ~~~~~-_        /T~~~~~~~T~~  * ~~~~~\      |               /T~~
     *        ~-_    / |       |    *       \     |              / |
     *           ~-_/  |       |    *        \    |             /  |
     *             /~-_|       |    *         \   |            /   |
     *            /    T-_     |    *          \  |           /    |
     *           /     |  ~-_  |    *           \ |          /     |
     * _________/______|_____~-|__  * ___________\__________/______|__
     *          |in_len|       |    *             |         |in_len|
     *          |<-- offset ---|    *             |offset-->|
     *
     *  a) avail:   max(0, used - preload)
     *  b) out_len: 0 .. avail
     *  c) in_len:  0 .. #
     *  d) offset:  -avail .. buffer->mix_size - out_size
     *  e) skip:    min(used, preload)
     *
     */

    out_scale = 1.0f - (gfloat) xfade_cfg_fadeout_volume(fc) / 100.0f;
    in_scale  = 1.0f - (gfloat) xfade_cfg_fadein_volume (fc) / 100.0f;

    /* rules (see above) */
    /* a: leave preload untouched */
    avail = buffer->used - buffer->preload_size;
    if (avail < 0)
        avail = 0;

    /* skip end of song */
    out_skip = MS2B(xfade_cfg_out_skip(fc)) & -4;
    if (out_skip > avail)
    {
        AUDDBG("[crossfade] apply_fade_config: WARNING: clipping out_skip (%d -> %d)!\n", B2MS(out_skip), B2MS(avail));
        out_skip = avail;
        out_skip_clipped = TRUE;
    }

    if (out_skip > 0)
    {
        buffer->used -= out_skip;
        avail -= out_skip;
    }

    /* b: fadeout */
    out_len = MS2B(xfade_cfg_fadeout_len(fc)) & -4;
    if (out_len > avail)
    {
        AUDDBG("[crossfade] apply_fade_config: WARNING: clipping out_len (%d -> %d)!\n", B2MS(out_len), B2MS(avail));
        out_len = avail;
        out_len_clipped = TRUE;
    }
    else if (out_len < 0)
        out_len = 0;

    /* skip beginning of song */
    in_skip = MS2B(xfade_cfg_in_skip(fc)) & -4;
    if (in_skip < 0)
        in_skip = 0;

    /* c: fadein */
    in_len = MS2B(xfade_cfg_fadein_len(fc)) & -4;
    if (in_len < 0)
        in_len = 0;

    /* d: offset (mixing point) */
    offset = MS2B(xfade_cfg_offset(fc)) & -4;
    if (offset < -avail)
    {
        AUDDBG("[crossfade] apply_fade_config: WARNING: clipping offset (%d -> %d)!\n", B2MS(offset), -B2MS(avail));
        offset = -avail;
        offset_clipped = TRUE;
    }
    if (offset > (buffer->mix_size - out_len))
        offset = buffer->mix_size - out_len;

    /* e */
    preload = buffer->preload_size;
    if (preload > buffer->used)
        preload = buffer->used;

    /* cut off rest of stream (decreases latency on manual songchange) */
    if (fc->flush)
    {
        gint cutoff = avail - MAX(out_len, -offset);    /* MAX() -> glib.h */
        if (cutoff > 0)
        {
            AUDDBG("[crossfade] apply_fade_config: %d ms flushed\n", B2MS(cutoff));
            buffer->used -= cutoff;
            avail -= cutoff;
        }

        /* make sure there is no pending silence */
        buffer->silence = 0;
        buffer->silence_len = 0;
    }

    /* begin modifying buffer at index */
    index = (buffer->rd_index + buffer->used - out_len) % buffer->size;

    /* fade out (modifies buffer directly) */
    fade = 0;
    length = out_len;
    while (length > 0)
    {
        gint16 *p = buffer->data + index;
        gint blen = buffer->size - index;
        if (blen > length) blen = length;

        for (n = blen / 4; n > 0; n--)
        {
            gfloat factor = 1.0f - (((gfloat) fade / out_len) * out_scale);
            *p = (gfloat)*p * factor; p++;
            *p = (gfloat)*p * factor; p++;
            fade += 4;
        }

        index = (index + blen) % buffer->size;
        length -= blen;
    }

    /* Initialize fadein. Note that the actual fading / mixing will be done
     * on-the-fly when audio data is received by xfade_write_audio() */

    /* start skipping */
    if (in_skip > 0)
    {
        buffer->skip = in_skip;
        buffer->skip_len = in_skip;
    }
    else
        buffer->skip = 0;

    /* start fading in */
    if (in_len > 0)
    {
        buffer->fade = in_len;
        buffer->fade_len = in_len;
        buffer->fade_scale = in_scale;
    }
    else
        buffer->fade = 0;

    /* start mixing */
    if (offset < 0)
    {
        length = -offset;
        buffer->mix = length;
        buffer->used -= length;
    }
    else
        buffer->mix = 0;

    /* start silence if applicable (will be applied in buffer_thread_f) */
    if (offset > 0)
    {
        if ((buffer->silence > 0) || (buffer->silence_len > 0))
            AUDDBG("[crossfade] apply_config: WARNING: silence in progress (%d/%d ms)\n",
                   B2MS(buffer->silence), B2MS(buffer->silence_len));

        buffer->silence = buffer->used;
        buffer->silence_len = offset;
    }

    /* done */
    if (in_skip || out_skip)
        AUDDBG("[crossfade] apply_fade_config: out_skip=%d in_skip=%d\n", B2MS(out_skip), B2MS(in_skip));
    AUDDBG("[crossfade] apply_fade_config: avail=%d out=%d in=%d offset=%d preload=%d\n",
           B2MS(avail), B2MS(out_len), B2MS(in_len), B2MS(offset), B2MS(preload));
}

static gint
extract_track(const gchar *name)
{
#if 1
    /* skip non-digits at beginning */
    while (*name && !isdigit(*name))
        name++;

    return atoi(name);
#else
    /* Remove all but numbers.
     * Will not work if a filename has number in the title, like "track-03-U2.mp3"
     * Ideally, should look into id3 track entry and fallback to filename
     * */
    gchar temp[8];
    int t = 0;

    memset(temp, 0, sizeof(temp));
    while (*name != '\0' && t < sizeof(temp))
    {
        if (strcmp(name, "mp3") == 0)
            break;

        if (isdigit(*name))
            temp[t++] = *name;

        name++;
    }
    return atoi(temp);
#endif
}

static gint
album_match(gchar *old, gchar *new)
{
    gchar *old_dir, *new_dir;
    gboolean same_dir;
    gint old_track = 0, new_track = 0;

    if (!old || !new)
        return 0;

    old_dir = g_dirname(old);
    new_dir = g_dirname(new);
    same_dir = !strcmp(old_dir, new_dir);
    g_free(old_dir);
    g_free(new_dir);

    if (!same_dir)
    {
        AUDDBG("[crossfade] album_match: no match (different dirs)\n");
        return 0;
    }

    old_track = extract_track(g_basename(old));
    new_track = extract_track(g_basename(new));

    if (new_track <= 0)
    {
        AUDDBG("[crossfade] album_match: can't parse track number:\n");
        AUDDBG("[crossfade] album_match: ... \"%s\"\n", g_basename(new));
        return 0;
    }

    if ((old_track < 0) || (old_track + 1 != new_track))
    {
        AUDDBG("[crossfade] album_match: no match (same dir, but non-successive (%d, %d))\n", old_track, new_track);
        return 0;
    }

    AUDDBG("[crossfade] album_match: match detected (same dir, successive tracks (%d, %d))\n", old_track, new_track);

    return old_track;
}

static gint
xfade_open_audio(AFormat fmt, int rate, int nch)
{
    gint pos, force_reopen = 0;
    gchar *file, *title;

    struct timeval tv;
    glong dt;

    AUDDBG("[crossfade]\n");

    in_format.fmt = FMT_S16_NE;

    if ((in_format.rate != rate && in_format.rate > 0) || (in_format.nch != nch && in_format.nch > 0))
    {
        AUDDBG("[crossfade] open_audio: format changed, reopen device forced\n");
        force_reopen = 1;
    }

    in_format.rate = rate;
    in_format.nch = nch;
    in_format.is_8bit = (in_format.fmt == FMT_U8 || in_format.fmt == FMT_S8);
    in_format.bps = calc_bitrate(in_format.fmt, in_format.rate, in_format.nch);

    AUDDBG("[crossfade] open_audio: pid=%d\n", (int) getpid());

    /* sanity... don't do anything about it */
    if (opened)
        AUDDBG("[crossfade] open_audio: WARNING: already opened!\n");

    /* get filename */
    pos     = xfplaylist_get_position ();
    file    = xfplaylist_get_filename (pos);
    title   = xfplaylist_get_songtitle(pos);

    if (!file)
        file = g_strdup(title);

    AUDDBG("[crossfade] open_audio: bname=\"%s\"\n", g_basename(file));
    AUDDBG("[crossfade] open_audio: title=\"%s\"\n", title);

    /* is this an automatic crossfade? */
    if (last_filename && (fade_config == &config->fc[FADE_CONFIG_XFADE]))
    {
        /* check if next song is the same as the current one */
        if (config->no_xfade_if_same_file && !strcmp(last_filename, file))
        {
            AUDDBG("[crossfade] open_audio: same file, disabling crossfade\n");
            fade_config = &config->fc[FADE_CONFIG_ALBUM];
        }

        /* check if next song is the next song from the same album */
        else if (config->album_detection && album_match(last_filename, file))
        {
            gboolean use_fc_album = FALSE;

            if (xfade_cfg_gap_trail_enable(config))
            {
                AUDDBG("[crossfade] album_match: "
                       "trailing gap: length=%d/%d ms\n", B2MS(buffer->gap_killed), B2MS(buffer->gap_len));

                if (buffer->gap_killed < buffer->gap_len)
                {
                    AUDDBG("[crossfade] album_match: "
                           "trailing gap: -> no silence, probably pre-faded\n");
                    use_fc_album = TRUE;
                }
                else
                {
                    AUDDBG("[crossfade] album_match: " "trailing gap: -> silence, sticking to XFADE\n");
                }
            }
            else
            {
                AUDDBG("[crossfade] album_match: " "trailing gap killer disabled\n");
                use_fc_album = TRUE;
            }

            if (use_fc_album)
            {
                AUDDBG("[crossfade] album_match: " "-> using FADE_CONFIG_ALBUM\n");
                fade_config = &config->fc[FADE_CONFIG_ALBUM];
            }
        }
    }
    g_free(last_filename);
    last_filename = g_strdup(file);

    /* check for streaming */
    if (aud_vfs_is_remote(file))
    {
        AUDDBG("[crossfade] open_audio: HTTP underrun workaround enabled.\n");
        is_http = TRUE;
    }
    else
        is_http = FALSE;

    g_free(file); file = NULL;
    g_free(title); title = NULL;

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    /* reset writer timeout */
    gettimeofday(&last_write, NULL);

    /* calculate time since last close() (don't care about overflows at 24h) */
    if (output_opened)
    {
        gettimeofday(&tv, NULL);
        dt = (tv.tv_sec - last_close.tv_sec) * 1000 + (tv.tv_usec - last_close.tv_usec) / 1000;
    }
    else
        dt = 0;

    AUDDBG("[crossfade] open_audio: fmt=%d rate=%d nch=%d bps=%d dt=%ld ms\n", in_format.fmt, in_format.rate, in_format.nch, in_format.bps, dt);

    /* (re)open the device if necessary */
    if (!output_opened)
    {
        if (open_output())
        {
            AUDDBG("[crossfade] open_audio: error opening/configuring output!\n");
            MUTEX_UNLOCK(&buffer_mutex);
            return 0;
        }
        fade_config = &config->fc[FADE_CONFIG_START];
    }

    /* reset */
    streampos = 0;
    playing   = TRUE;
    opened    = TRUE;
    paused    = FALSE;

    /* reset mix/fade/gap */
    buffer_mfg_reset(buffer, config);

    /* enable gap killer / zero crossing only for automatic/album songchange */
    switch (fade_config->config)
    {
        case FADE_CONFIG_XFADE:
        case FADE_CONFIG_ALBUM:
            break;

        default:
            buffer->gap = GAP_SKIPPING_DONE;
    }

    /* restart realtime throttling */
    output_written = 0;

    /* start mixing */
    switch (fade_config ? fade_config->type : -1)
    {
        case FADE_TYPE_FLUSH:
            AUDDBG("[crossfade] open_audio: FLUSH:\n");

            /* flush output plugin */
            the_op->flush(0);
            output_streampos = 0;

            /* flush buffer */
            buffer_reset(buffer, config);

            /* apply fade config (pause/fadein after flush) */
            xfade_apply_fade_config(fade_config);

            /* also repopen device (if configured so in the plugin compat. options) */
            if (the_op_config.force_reopen || force_reopen)
            {
                buffer->reopen = 0;
                buffer->reopen_sync = FALSE;
            }
            break;

        case FADE_TYPE_REOPEN:
            AUDDBG("[crossfade] open_audio: REOPEN:\n");

            /* flush buffer if applicable */
            if (fade_config->flush)
                buffer_reset(buffer, config);

            if (buffer->reopen >= 0)
                AUDDBG("[crossfade] open_audio: REOPEN: WARNING: reopen in progress (%d ms)\n",
                       B2MS(buffer->reopen));

            /* start reopen countdown (will be executed in buffer_thread_f) */
            buffer->reopen = buffer->used;  /* may be 0 */
            buffer->reopen_sync = FALSE;
            break;

        case FADE_TYPE_NONE:
        case FADE_TYPE_PAUSE:
        case FADE_TYPE_SIMPLE_XF:
        case FADE_TYPE_ADVANCED_XF:
        case FADE_TYPE_FADEIN:
        case FADE_TYPE_FADEOUT:
            AUDDBG("[crossfade] open_audio: XFADE:\n");

            /* apply fade config (do fadeout, init mix/fade/gap, add silence) */
            xfade_apply_fade_config(fade_config);

            /* set reopen countdown. after buffer_thread_f has written
             * buffer->reopen bytes, it will close/reopen the output plugin. */
            if ((the_op_config.force_reopen || force_reopen) && !(fade_config->config == FADE_CONFIG_START))
            {
                if (buffer->reopen >= 0)
                    AUDDBG("[crossfade] open_audio: XFADE: WARNING: reopen in progress (%d ms)\n",
                           B2MS(buffer->reopen));
                buffer->reopen = buffer->used;
                buffer->reopen_sync = TRUE;
            }
            break;
    }

    /* calculate offset of the output plugin */
    output_offset = the_op->written_time() + B2MS(buffer->used) + B2MS(buffer->silence_len);

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);

    /* done */
    return 1;
}

void
xfade_write_audio(void *ptr, int length)
{
    gint free;
    gint ofs = 0;

#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] write_audio: ptr=0x%08lx, length=%d\n", (long) ptr, length);
#endif

    /* sanity */
    if (length <= 0)
        return;

    /* Commenting out for now
     * fix clicks while using with samplerate converter - Michal
    if (length & 3)
    {
        AUDDBG("[crossfade] write_audio: truncating %d bytes!\n", length & 3);
        length &= -4;
    }*/

    /* update input accumulator (using input format size) */
    streampos += length;

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    /* check if device has been closed, reopen if necessary */
    if (!output_opened)
    {
        if (open_output())
        {
            AUDDBG("[crossfade] write_audio: reopening failed!\n");
            MUTEX_UNLOCK(&buffer_mutex);
            return;
        }
    }

    /* reset timeout */
    gettimeofday(&last_write, NULL);

    /* calculate free buffer space, check for overflow (should never happen :) */
    free = buffer->size - buffer->used;
    if (length > free)
    {
        AUDDBG("[crossfade] write_audio: %d bytes truncated!\n", length - free);
        length = free;
    }

    /* skip beginning of song */
    if ((length > 0) && (buffer->skip > 0))
    {
        gint blen = MIN(length, buffer->skip);

        buffer->skip -= blen;
        length -= blen;
        ptr += blen;
    }

    /* kill leading gap */
    if ((length > 0) && (buffer->gap > 0))
    {
        gint blen = MIN(length, buffer->gap);
        gint16 *p = ptr;
        gint index = 0;

        gint16 left, right;
        while (index < blen)
        {
            left = *p++, right = *p++;
            if (ABS(left)  >= buffer->gap_level) break;
            if (ABS(right) >= buffer->gap_level) break;
            index += 4;
        }

        buffer->gap -= index;
        length -= index;
        ptr += index;

        if ((index < blen) || (buffer->gap <= 0))
        {
            buffer->gap_killed = buffer->gap_len - buffer->gap;
            buffer->gap = 0;

            AUDDBG("[crossfade] write_audio: leading gap size: %d/%d ms\n",
                   B2MS(buffer->gap_killed), B2MS(buffer->gap_len));

            /* fix streampos */
            streampos -= (gint64) buffer->gap_killed;
        }
    }

    /* start skipping to next crossing (if enabled) */
    if (buffer->gap == 0)
    {
        if (config->gap_crossing)
        {
            buffer->gap = GAP_SKIPPING_POSITIVE;
            buffer->gap_skipped = 0;
        }
        else
            buffer->gap = GAP_SKIPPING_DONE;
    }

    /* skip until next zero crossing (pos -> neg) */
    if ((length > 0) && (buffer->gap == GAP_SKIPPING_POSITIVE))
    {
        gint16 *p = ptr;
        gint index = 0;

        gint16 left;
        while (index < length)
        {
            left = *p++;
            p++;
            if (left < 0)
                break;
            index += 4;
        }

        buffer->gap_skipped += index;
        length -= index;
        ptr += index;

        if (index < length)
            buffer->gap = GAP_SKIPPING_NEGATIVE;
    }

    /* skip until next zero crossing (neg -> pos) */
    if ((length > 0) && (buffer->gap == GAP_SKIPPING_NEGATIVE))
    {
        gint16 *p = ptr;
        gint index = 0;

        gint16 left;
        while (index < length)
        {
            left = *p++;
            p++;
            if (left >= 0)
                break;
            index += 4;
        }

        buffer->gap_skipped += index;
        length -= index;
        ptr += index;

        if (index < length)
        {
            AUDDBG("[crossfade] write_audio: %d samples to next crossing\n", buffer->gap_skipped);
            buffer->gap = GAP_SKIPPING_DONE;
        }
    }

    /* update preload. the buffer thread will not write any
     * data to the device before preload is decreased below 1. */
    if ((length > 0) && (buffer->preload > 0))
        buffer->preload -= length;

    /* fadein -- FIXME: is modifying the input/effect buffer safe? */
    if ((length > 0) && (buffer->fade > 0))
    {
        gint16 *p = ptr;
        gint blen = MIN(length, buffer->fade);
        gint n;

        for (n = blen / 4; n > 0; n--)
        {
            gfloat factor = 1.0f - (((gfloat) buffer->fade / buffer->fade_len) * buffer->fade_scale);
            *p = (gfloat)*p * factor; p++;
            *p = (gfloat)*p * factor; p++;
            buffer->fade -= 4;
        }
    }

    /* mix */
    while ((length > 0) && (buffer->mix > 0))
    {
        gint wr_index = (buffer->rd_index + buffer->used) % buffer->size;
        gint     blen = buffer->size - wr_index;
        gint16    *p1 = buffer->data + wr_index;
        gint16    *p2 = ptr + ofs;
        gint n;

        if (blen > length)      blen = length;
        if (blen > buffer->mix) blen = buffer->mix;

        for (n = blen / 2; n > 0; n--)
        {
            gint out = (gint)*p1 + *p2++;  /* add */
            if (out > 32767)               /* clamp */
                *p1++ = 32767;
            else if (out < -32768)
                *p1++ = -32768;
            else
                *p1++ = out;
        }

        buffer->used += blen;
        buffer->mix -= blen;
        length -= blen;
        ofs += blen;
    }

    /* normal write */
    while (length > 0)
    {
        gint wr_index = (buffer->rd_index + buffer->used) % buffer->size;
        gint blen = buffer->size - wr_index;

        if (blen > length)
            blen = length;

        memcpy(buffer->data + wr_index, ptr + ofs, blen);

        buffer->used += blen;
        length -= blen;
        ofs += blen;
    }

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);
#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] write_audio: done.\n");
#endif
}

/* sync_output: wait for output plugin to finish playback  */
/*              is only called from within buffer_thread_f */
static void
sync_output()
{
    glong dt, total;
    gint opt, opt_last;
    struct timeval tv, tv_start, tv_last_change;
    gboolean was_closed = !opened;

    /* Don't crash with a newer plugin that provides drain() instead of
     * buffer_playing().  Please update crossfade to deal with such plugins
     * properly! -jlindgren */
    if (the_op->buffer_playing == NULL)
        return;

    if (! the_op->buffer_playing ())
    {
        AUDDBG("[crossfade] sync_output: nothing to do\n");
        return;
    }

    AUDDBG("[crossfade] sync_output: waiting for plugin...\n");

    dt = 0;
    opt_last = 0;
    gettimeofday(&tv_start, NULL);
    gettimeofday(&tv_last_change, NULL);

    while ((dt < SYNC_OUTPUT_TIMEOUT)
           && !stopped && output_opened && !(was_closed && opened) && the_op && the_op->buffer_playing())
    {

        /* use output_time() to check if the output plugin is still active */
        if (the_op->output_time)
        {
            opt = the_op->output_time();
            if (opt != opt_last)
            {
                /* output_time has changed */
                opt_last = opt;
                gettimeofday(&tv_last_change, NULL);
            }
            else
            {
                /* calculate time since last change of the_op->output_time() */
                gettimeofday(&tv, NULL);
                dt = (tv.tv_sec - tv_last_change.tv_sec) * 1000 + (tv.tv_usec - tv_last_change.tv_usec) / 1000;
            }
        }

        /* yield */
        MUTEX_UNLOCK(&buffer_mutex);
        xfade_usleep(10000);
        MUTEX_LOCK(&buffer_mutex);
    }

    /* calculate total time we spent in here */
    gettimeofday(&tv, NULL);
    total = (tv.tv_sec - tv_start.tv_sec) * 1000 + (tv.tv_usec - tv_start.tv_usec) / 1000;

    /* print some debug info */
    /* *INDENT-OFF* */
    if (stopped)
        AUDDBG("[crossfade] sync_output: ... stopped\n");
    else if (was_closed && opened)
        AUDDBG("[crossfade] sync_output: ... reopened\n");
    else if (dt >= SYNC_OUTPUT_TIMEOUT)
        AUDDBG("[crossfade] sync_output: ... TIMEOUT! (%ld ms)\n", total);
    else
        AUDDBG("[crossfade] sync_output: ... done (%ld ms)\n", total);
    /* *INDENT-ON* */
}

void *
buffer_thread_f(void *arg)
{
    gpointer data;
    gint sync;
    gint op_free;
    gint length_bak, length, blen;
    glong timeout, dt;
    gboolean stopping;

    struct timeval tv;
    struct timeval mark;

    AUDDBG("[crossfade] buffer_thread_f: thread started (pid=%d)\n", (int) getpid());

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    while (!stopped)
    {
        /* yield */
#ifdef DEBUG_HARDCORE
        AUDDBG("[crossfade] buffer_thread_f: yielding...\n");
#endif
        MUTEX_UNLOCK(&buffer_mutex);
        xfade_usleep(10000);
        MUTEX_LOCK(&buffer_mutex);

        /* --------------------------------------------------------------------- */

        stopping = FALSE;

        /* V0.3.0: New timeout detection */
        if (!opened)
        {
            gboolean current = xfplayer_input_playing();

            /* also see fini() */
            if (last_close.tv_sec || last_close.tv_usec)
            {
                gettimeofday(&tv, NULL);
                timeout = (tv.tv_sec  - last_close.tv_sec)  * 1000
                        + (tv.tv_usec - last_close.tv_usec) / 1000;
            }
            else
                timeout = -1;

            if (current != input_playing)
            {
                input_playing = current;

                if (current)
                    AUDDBG("[crossfade] buffer_thread_f: input restarted after %ld ms\n", timeout);
                else
                    AUDDBG("[crossfade] buffer_thread_f: input stopped after + %ld ms\n", timeout);
            }

            /* 0.3.0: HACK: output_keep_opened: play silence during prebuffering */
            if (input_playing && config->output_keep_opened && (buffer->used == 0))
            {
                buffer->silence = 0;
                buffer->silence_len = MS2B(100);
            }

            if (!input_playing && config->fc[FADE_CONFIG_STOP].type == FADE_TYPE_NONE)
            {
                stopping = TRUE;
                AUDDBG("[crossfade] buffer_thread_f: input stopped after %ld ms\n", timeout);
            }
            else if (((timeout < 0) || (timeout >= config->songchange_timeout)) && !input_playing)
            {
                if (playing)
                    AUDDBG("[crossfade] buffer_thread_f: timeout:"
                           " input did not restart after %ld ms\n", timeout);
                stopping = TRUE;
            }
        }

        /* V0.2.4: Moved the timeout checks in front of the buffer_free() check
         *         below. Before, buffer_thread_f could (theoretically) loop
         *         endlessly if buffer_free() returned 0 all the time. */

        /* calculate time since last write to the buffer (ignore overflows) */
        gettimeofday(&tv, NULL);
        timeout = (tv.tv_sec  - last_write.tv_sec)  * 1000
                + (tv.tv_usec - last_write.tv_usec) / 1000;

        /* check for timeout/eop (note this is the only way out of this loop) */
        if (stopping)
        {
            if (playing)
            {
                AUDDBG("[crossfade] buffer_thread_f: timeout: manual stop\n");

                /* if CONFIG_STOP is of TYPE_NONE, immediatelly close the device... */
                if ((config->fc[FADE_CONFIG_STOP].type == FADE_TYPE_NONE) && !config->output_keep_opened)
                    break;

                /* special handling for pause */
                if (paused)
                {
                    AUDDBG("[crossfade] buffer_thread_f: timeout: paused, closing now...\n");
                    paused = FALSE;
                    if (config->output_keep_opened)
                        the_op->pause(0);
                    else
                        break;
                }
                else if (buffer->pause >= 0)
                {
                    AUDDBG("[crossfade] buffer_thread_f: timeout: cancelling pause countdown\n");
                    buffer->pause = -1;
                }

                /* ...otherwise, do the fadeout first */
                xfade_apply_fade_config(&config->fc[FADE_CONFIG_STOP]);

                /* force CONFIG_START in case the user restarts playback during fadeout */
                fade_config = &config->fc[FADE_CONFIG_START];
                playing = FALSE;
                eop = TRUE;
            }
            else
            {
                if (!eop)
                {
                    AUDDBG("[crossfade] buffer_thread_f: timeout: end of playback\n");

                    /* 0.3.3: undo trailing gap killer at end of playlist */
                    if (buffer->gap_killed)
                    {
                        buffer->used += buffer->gap_killed;
                        AUDDBG("[crossfade] buffer_thread_f: timeout:"
                               " undoing trailing gap (%d ms)\n", B2MS(buffer->gap_killed));
                    }

                    /* do the fadeout if applicable */
                    if (config->fc[FADE_CONFIG_EOP].type != FADE_TYPE_NONE)
                        xfade_apply_fade_config(&config->fc[FADE_CONFIG_EOP]);

                    fade_config = &config->fc[FADE_CONFIG_START];   /* see above */
                    eop = TRUE;
                }

                if (buffer->used == 0)
                {
                    if (config->output_keep_opened)
                    {
                        /* 0.3.0: play silence while keeping the output opened */
                        buffer->silence = 0;
                        buffer->silence_len = MS2B(100);
                    }
                    else if (buffer->silence_len <= 0)
                    {
                        sync_output();
                        if (opened)
                        {
                            AUDDBG("[crossfade] buffer_thread_f: timeout, eop: device has been reopened\n");
                            AUDDBG("[crossfade] buffer_thread_f: timeout, eop: -> continuing playback\n");
                            eop = FALSE;
                        }
                        else
                        {
                            AUDDBG("[crossfade] buffer_thread_f: timeout, eop: closing output...\n");
                            break;
                        }
                    }
                }
            }
        }
        else
            eop = FALSE;

        /* --------------------------------------------------------------------- */

        /* get free space in device output buffer
         * NOTE: disk_writer always returns <big int> here */
        if (the_op->buffer_free != NULL)
            op_free = the_op->buffer_free() & -4;
        else
            /* Don't crash with a newer plugin that does not provide
             * buffer_free().  Please update crossfade to deal with such plugins
             * properly! -jlindgren */
            op_free = G_MAXINT;

        /* continue waiting if there is no room in the device buffer */
        if (op_free == 0)
            continue;

        /* --- Limit OP buffer use (decreases latency) ------------------------- */

        /* HACK: limit output plugin buffer usage to decrease latency */
        if (config->enable_op_max_used)
        {
            gint output_time = the_op->output_time();
            gint output_used = the_op->written_time() - output_time;
            gint output_limit = MS2B(config->op_max_used_ms - MIN(output_used, config->op_max_used_ms));

            if (output_flush_time != output_time)
            {
                /* slow down output, but always write _some_ data */
                if (output_limit < in_format.bps / 100 / 2)
                    output_limit = in_format.bps / 100 / 2;

                if (op_free > output_limit)
                    op_free = output_limit;
            }
        }

        /* --- write silence --------------------------------------------------- */

        if (!paused && (buffer->silence <= 0) && (buffer->silence_len >= 4))
        {
            /* write as much silence as a) there is left and b) the device can take */
            length = buffer->silence_len;
            if (length > op_free)
                length = op_free;

            /* make sure we always operate on stereo sample boundary */
            length &= -4;

            /* HACK: don't stay in here too long when in realtime mode (see below) */
            if (realtime)
                gettimeofday(&mark, NULL);

            /* write length bytes to the device */
            length_bak = length;
            while (length > 0)
            {
                data = zero_4k;
                blen = sizeof(zero_4k);
                if (blen > length)
                    blen = length;

                /* make sure zero_4k is cleared. The effect plugin within
                 * the output plugin may have modified this buffer! (0.2.8) */
                memset(zero_4k, 0, blen);

                /* HACK: the original OSS plugin hangs when writing large
                 * blocks (greater than device buffer size) in realtime mode */
                if (the_op_config.max_write_enable && (blen > the_op_config.max_write_len))
                    blen = the_op_config.max_write_len;

                /* finally, write data */
                the_op->write_audio(data, blen);
                length -= blen;

                /* HACK: don't stay in here too long (force yielding every 10 ms) */
                if (realtime)
                {
                    gettimeofday(&tv, NULL);
                    dt = (tv.tv_sec  - mark.tv_sec)  * 1000
                       + (tv.tv_usec - mark.tv_usec) / 1000;
                    if (dt >= 10)
                        break;
                }
            }

            /* calculate how many bytes actually have been written */
            length = length_bak - length;
        }

        /* --- write data ------------------------------------------------- */

        else if (!paused && (buffer->preload <= 0)  && (buffer->used >= 4))
        {
            /* write as much data as a) is available and b) the device can take */
            length = buffer->used;
            if (length > op_free)
                length = op_free;

            /* HACK: throttle output (used with fast output plugins) */
            if (the_op_config.throttle_enable && !realtime && opened)
            {
                sync = buffer->sync_size - (buffer->size - buffer->used);
                if      (sync < 0)      length = 0;
                else if (sync < length) length = sync;
            }

            /* clip length to silence countdown (if applicable) */
            if ((buffer->silence >= 4) && (length > buffer->silence))
                length = buffer->silence;

            /* clip length to reopen countdown (if applicable) */
            if ((buffer->reopen >= 0) && (length > buffer->reopen))
                length = buffer->reopen;

            /* clip length to pause countdown (if applicable) */
            if ((buffer->pause >= 0) && (length > buffer->pause))
                length = buffer->pause;

            /* make sure we always operate on stereo sample boundary */
            length &= -4;

            /* HACK: don't stay in here too long when in realtime mode (see below) */
            if (realtime)
                gettimeofday(&mark, NULL);

            /* write length bytes to the device */
            length_bak = length;
            while (length > 0)
            {
                data = buffer->data + buffer->rd_index;
                blen = buffer->size - buffer->rd_index;
                if (blen > length)
                    blen = length;

                /* HACK: the original OSS plugin hangs when writing large
                 * blocks (greater than device buffer size) in realtime mode */
                if (the_op_config.max_write_enable && (blen > the_op_config.max_write_len))
                    blen = the_op_config.max_write_len;

                /* finally, write data */
                the_op->write_audio(data, blen);

                buffer->rd_index = (buffer->rd_index + blen) % buffer->size;
                buffer->used -= blen;
                length -= blen;

                /* HACK: don't stay in here too long (force yielding every 10 ms) */
                if (realtime)
                {
                    gettimeofday(&tv, NULL);
                    dt = (tv.tv_sec - mark.tv_sec) * 1000 + (tv.tv_usec - mark.tv_usec) / 1000;
                    if (dt >= 10)
                        break;
                }
            }

            /* calculate how many bytes actually have been written */
            length = length_bak - length;
        }
        else
            length = 0;

        /* update realtime throttling */
        output_written   += length;
        output_streampos += length;

        /* --- check countdowns ------------------------------------------------ */

        if (buffer->silence > 0)
        {
            buffer->silence -= length;
            if (buffer->silence < 0)
                AUDDBG("[crossfade] buffer_thread_f: WARNING: silence overrun: %d\n", buffer->silence);
        }
        else if (buffer->silence_len > 0)
        {
            buffer->silence_len -= length;
            if (buffer->silence_len <= 0)
            {
                if (buffer->silence_len < 0)
                    AUDDBG("[crossfade] buffer_thread_f: WARNING: silence_len overrun: %d\n",
                           buffer->silence_len);
            }
        }

        if ((buffer->reopen >= 0) && !((buffer->silence <= 0) && (buffer->silence_len > 0)))
        {
            buffer->reopen -= length;
            if (buffer->reopen <= 0)
            {
                if (buffer->reopen < 0)
                    AUDDBG("[crossfade] buffer_thread_f: WARNING: reopen overrun: %d\n", buffer->reopen);

                AUDDBG("[crossfade] buffer_thread_f: closing/reopening device\n");
                if (buffer->reopen_sync)
                    sync_output();

                if (the_op->close_audio)
                    the_op->close_audio();

                if (!the_op->open_audio(in_format.fmt, in_format.rate, in_format.nch))
                {
                    AUDDBG("[crossfade] buffer_thread_f: reopening output plugin failed!\n");
                    g_free(buffer->data);
                    output_opened = FALSE;
                    MUTEX_UNLOCK(&buffer_mutex);
                    THREAD_EXIT(0);
                    return NULL;
                }

                output_flush_time = 0;
                output_written    = 0;
                output_streampos  = 0;

                /* We need to take the leading gap killer into account here:
                 * It will fix streampos only after gapkilling has finished.
                 * So, if gapkilling is still in progress at this point, we
                 * have to fix it ourselves. */
                output_offset = buffer->used;
                if ((buffer->gap_len > 0) && (buffer->gap > 0))
                    output_offset += buffer->gap_len - buffer->gap;
                output_offset = B2MS(output_offset) - xfade_written_time();

                /* make sure reopen is not 0 */
                buffer->reopen = -1;
            }
        }

        if (buffer->pause >= 0)
        {
            buffer->pause -= length;
            if (buffer->pause <= 0)
            {
                if (buffer->pause < 0)
                    AUDDBG("[crossfade] buffer_thread_f: WARNING: pause overrun: %d\n", buffer->pause);

                AUDDBG("[crossfade] buffer_thread_f: pausing output\n");

                paused = TRUE;
                sync_output();

                if (paused)
                    the_op->pause(1);
                else
                    AUDDBG("[crossfade] buffer_thread_f: unpause during sync\n"); buffer->pause = -1;
            }
        }
    }

    /* ----------------------------------------------------------------------- */

    /* cleanup: close output */
    if (output_opened)
    {
        AUDDBG("[crossfade] buffer_thread_f: closing output...\n");

        if (the_op->close_audio)
            the_op->close_audio();

        AUDDBG("[crossfade] buffer_thread_f: closing output... done\n");

        g_free(buffer->data);
        output_opened = FALSE;
    }
    else
        AUDDBG("[crossfade] buffer_thread_f: output already closed!\n");

    /* ----------------------------------------------------------------------- */

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);

    /* done */
    AUDDBG("[crossfade] buffer_thread_f: thread finished\n");
    THREAD_EXIT(0);
    return NULL;
}

void
xfade_close_audio()
{
    AUDDBG("[crossfade] close:\n");
    AUDDBG("[crossfade] close: playing=%d filename=%s\n",
           xfplayer_input_playing(), xfplaylist_get_filename(xfplaylist_get_position()));

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    /* sanity... the vorbis plugin likes to call close_audio() twice */
    if (!opened)
    {
        AUDDBG("[crossfade] close: WARNING: not opened!\n");
        MUTEX_UNLOCK(&buffer_mutex);
        return;
    }

    /* HACK: to distinguish between STOP and EOP, check Audacious'
       input_playing() variable. It seems to be TRUE at this point
       only when the end of the playlist is reached.

       Normally, 'playing' is constantly being updated in the
       xfade_buffer_playing() callback, but Audacious does not seem
       to use it. Therefore, we can set 'playing' to FALSE here,
       which later 'buffer_thread' will interpret as EOP (see above).
    */
    if (xfplayer_input_playing())
        playing = FALSE;

    if (playing)
    {
        /* immediatelly close output when paused */
        if (paused)
        {
            buffer->pause = -1;
            paused = FALSE;
            if (config->output_keep_opened)
            {
                buffer->used = 0;
                the_op->flush(0);
                the_op->pause(0);
            }
            else
                stopped = TRUE;
        }

            AUDDBG("[crossfade] close: stop\n");

        fade_config = &config->fc[FADE_CONFIG_MANUAL];
    }
    else
    {
        /* gint x = *((gint *)0); */  /* force SEGFAULT for debugging */
        AUDDBG("[crossfade] close: songchange/eop\n");

        /* kill trailing gap (does not use buffer->gap_*) */
        if (output_opened && xfade_cfg_gap_trail_enable(config))
        {
            gint gap_len   = MS2B(xfade_cfg_gap_trail_len(config)) & -4;
            gint gap_level = xfade_cfg_gap_trail_level(config);
            gint length    = MIN(gap_len, buffer->used);

            /* AUDDBG("[crossfade] close: len=%d level=%d length=%d\n", gap_len, gap_level, length); */

            buffer->gap_killed = 0;
            while (length > 0)
            {
                gint wr_xedni = (buffer->rd_index + buffer->used - 1) % buffer->size + 1;
                gint     blen = MIN(length, wr_xedni);
                gint16     *p = buffer->data + wr_xedni, left, right;
                gint    index = 0;

                while (index < blen)
                {
                    right = *--p;
                    left  = *--p;
                    if (ABS(left)  >= gap_level) break;
                    if (ABS(right) >= gap_level) break;
                    index += 4;
                }

                buffer->used -= index;
                buffer->gap_killed += index;

                if (index < blen)
                    break;
                length -= blen;
            }

            AUDDBG("[crossfade] close: trailing gap size: %d/%d ms\n", B2MS(buffer->gap_killed), B2MS(gap_len));
        }

        /* skip to previous zero crossing */
        if (output_opened && config->gap_crossing)
        {
            int crossing;

            buffer->gap_skipped = 0;
            for (crossing = 0; crossing < 4; crossing++)
            {
                while (buffer->used > 0)
                {
                    gint wr_xedni = (buffer->rd_index + buffer->used - 1) % buffer->size + 1;
                    gint     blen = MIN(buffer->used, wr_xedni);
                    gint16     *p = buffer->data + wr_xedni, left;
                    gint    index = 0;

                    while (index < blen)
                    {
                        left = (--p, *--p);
                        if ((crossing & 1) ^ (left > 0))
                            break;
                        index += 4;
                    }

                    buffer->used -= index;
                    buffer->gap_skipped += index;

                    if (index < blen)
                        break;
                }
            }
            AUDDBG("[crossfade] close: skipped %d bytes to previous zero crossing\n", buffer->gap_skipped);

            /* update gap_killed (for undoing gap_killer in case of EOP) */
            buffer->gap_killed += buffer->gap_skipped;
        }

        fade_config = &config->fc[FADE_CONFIG_XFADE];
    }

    /* XMMS has left the building */
    opened = FALSE;

    /* update last_close */
    gettimeofday(&last_close, NULL);
    input_playing = FALSE;

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);
}

void
xfade_flush(gint time)
{
    gint pos;
    gchar *file;

    AUDDBG("[crossfade] flush: time=%d\n", time);

    /* get filename */
    pos  = xfplaylist_get_position();
    file = xfplaylist_get_filename(pos);

    if (!file)
        file = g_strdup(xfplaylist_get_songtitle(pos));

    /* HACK: special handling for audacious, which just calls flush(0) on a songchange */
    if (file && last_filename && strcmp(file, last_filename) != 0)
    {
        AUDDBG("[crossfade] flush: filename changed, forcing close/reopen...\n");
        xfade_close_audio();
        /* 0.3.14: xfade_close_audio sets fade_config to FADE_CONFIG_MANUAL,
         *         but this is an automatic songchange */
        fade_config = &config->fc[FADE_CONFIG_XFADE];
        xfade_open_audio(in_format.fmt, in_format.rate, in_format.nch);
        AUDDBG("[crossfade] flush: filename changed, forcing close/reopen... done\n");
        return;
    }

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    /* update streampos with new stream position (input format size) */
    streampos = ((gint64) time * in_format.bps / 1000) & -4;

    /* flush output device / apply seek crossfade */
    if (config->fc[FADE_CONFIG_SEEK].type == FADE_TYPE_FLUSH)
    {
        /* flush output plugin */
        the_op->flush(time);
        output_flush_time = time;
        output_streampos = MS2B(time);

        /* flush buffer, disable leading gap killing */
        buffer_reset(buffer, config);
    }
    else if (paused)
    {
        fade_config_t fc;

        /* clear buffer */
        buffer->used = 0;

        /* apply only the fade_in part of FADE_CONFIG_PAUSE */
        memcpy(&fc, &config->fc[FADE_CONFIG_PAUSE], sizeof(fc));
        fc.out_len_ms    = 0;
        fc.ofs_custom_ms = 0;
        xfade_apply_fade_config(&fc);
    }
    else
        xfade_apply_fade_config(&config->fc[FADE_CONFIG_SEEK]);

    /* restart realtime throttling (should find another name for that var) */
    output_written = 0;

    /* make sure that the gapkiller is disabled */
    buffer->gap = 0;

    /* update output offset */
    output_offset = the_op->written_time() - time + B2MS(buffer->used) + B2MS(buffer->silence_len);

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);

#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] flush: time=%d: done.\n", time);
#endif
}

void
xfade_pause(short p)
{
    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    if (p)
    {
        fade_config_t *fc = &config->fc[FADE_CONFIG_PAUSE];
        if (fc->type == FADE_TYPE_PAUSE_ADV)
        {
            int fade, length, n;
            int index = buffer->rd_index;
            int out_len     = MS2B(xfade_cfg_fadeout_len(fc)) & -4;
            int in_len      = MS2B(xfade_cfg_fadein_len(fc))  & -4;
            int silence_len = MS2B(xfade_cfg_offset(fc))      & -4;

            /* limit fadeout/fadein len to available data in buffer */
            if ((out_len + in_len) > buffer->used)
            {
                out_len = (buffer->used / 2) & -4;
                in_len = out_len;
            }

            AUDDBG("[crossfade] pause: paused=1 out=%d in=%d silence=%d\n",
                   B2MS(out_len), B2MS(in_len), B2MS(silence_len));

            /* fade out (modifies buffer directly) */
            fade = 0;
            length = out_len;
            while (length > 0)
            {
                gint16 *p = buffer->data + index;
                gint blen = buffer->size - index;
                if (blen > length)
                    blen = length;

                for (n = blen / 4; n > 0; n--)
                {
                    gfloat factor = 1.0f - ((gfloat) fade / out_len);
                    *p = (gfloat)*p * factor; p++;
                    *p = (gfloat)*p * factor; p++;
                    fade += 4;
                }

                index = (index + blen) % buffer->size;
                length -= blen;
            }

            /* fade in (modifies buffer directly) */
            fade = 0;
            length = in_len;
            while (length > 0)
            {
                gint16 *p = buffer->data + index;
                gint blen = buffer->size - index;
                if (blen > length)
                    blen = length;

                for (n = blen / 4; n > 0; n--)
                {
                    gfloat factor = (gfloat) fade / in_len;
                    *p = (gfloat)*p * factor; p++;
                    *p = (gfloat)*p * factor; p++;
                    fade += 4;
                }

                index = (index + blen) % buffer->size;
                length -= blen;
            }

            /* start silence and pause countdowns */
            buffer->silence = out_len;
            buffer->silence_len = silence_len;
            buffer->pause = out_len + silence_len;
            paused = FALSE;  /* (!) will be set to TRUE in buffer_thread_f */
        }
        else
        {
            the_op->pause(1);
            paused = TRUE;
            AUDDBG("[crossfade] pause: paused=1\n");
        }
    }
    else
    {
        the_op->pause(0);
        buffer->pause = -1;
        paused = FALSE;
        AUDDBG("[crossfade] pause: paused=0\n");
    }

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);
}

gint
xfade_buffer_free()
{
    gint size, free;

#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] buffer_free:\n");
#endif

    /* sanity check */
    if (!output_opened)
    {
        AUDDBG("[crossfade] buffer_free: WARNING: output closed!\n");
        return buffer->sync_size;
    }

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    size = buffer->size;

    /* When running in realtime mode, we need to take special care here:
     * While XMMS is writing data to the output plugin, it will not yield
     * ANY processing time to the buffer thread. It will only stop when
     * xfade_buffer_free() no longer reports free buffer space. */
    if (realtime)
    {
        gint64 wanted = output_written + buffer->preload_size;

        /* Fix for XMMS misbehaviour (possibly a bug): If the free space as
         * returned by xfade_buffer_free() is below a certain minimum block size
         * (tests showed 2304 bytes), XMMS will not send more data until there
         * is enough room for one of those blocks.
         *
         * This breaks preloading in realtime mode. To make sure that the pre-
         * load buffer gets filled we request additional sync_size bytes. */
        wanted += buffer->sync_size;

        if (wanted <= size)
            size = wanted;
    }

    free = size - buffer->used;
    if (free < 0)
        free = 0;

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);

    /* Convert to input format size. For input rates > output rate this will
     * return less free space than actually is available, but we don't care. */
    free /= 2;
    if (in_format.is_8bit)
        free /= 2;
    if (in_format.nch == 1)
        free /= 2;

#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] buffer_free: %d\n", free);
#endif
    return free;
}

gint
xfade_buffer_playing()
{
    /*  always return FALSE here (if not in pause) so XMMS immediatelly
     *  starts playback of the next song */

    /*
     *  NOTE: this causes trouble when playing HTTP audio streams.
     *
     *  mpg123.lib will start prebuffering (and thus stalling output) when both
     *  1) it's internal buffer is emptied (does happen all the time)
     *  2) the output plugin's buffer_playing() (this function) returns FALSE
     */

    fade_config_t *fc;
    gint out_len, in_len, offset, length;

    /* Don't crash with a newer plugin that provides drain() instead of
     * buffer_playing().  Please update crossfade to deal with such plugins
     * properly! -jlindgren */
    if (the_op->buffer_playing == NULL)
        return FALSE;

    fc = &config->fc[FADE_CONFIG_XFADE];

    if (paused)
        playing = FALSE;
    else
    {
        playing =
            (is_http && (buffer->used > 0) && the_op->buffer_playing())
            || the_op->buffer_playing()
            || (buffer->reopen >= 0)
            || (buffer->silence > 0)
            || (buffer->silence_len > 0);

        if (playing && fc->type != FADE_TYPE_NONE)
        {
            out_len = xfade_cfg_fadeout_len(fc);
            in_len  = xfade_cfg_fadein_len(fc);
            offset  = xfade_cfg_offset(fc);

            switch (fc->type)
            {
                case FADE_TYPE_SIMPLE_XF:
                    length = out_len;
                    break;

                case FADE_TYPE_ADVANCED_XF:
                    if (in_len > out_len)
                        length = in_len;
                    else
                        length = out_len;

                    if (offset < 0 && -offset > length)
                        length = -offset;
                    else if (offset > length)
                        length = offset;
                    break;
                default:
                    length = 0;
            }

            if (audacious_drct_get_time() + length >= audacious_drct_get_length())
                return FALSE;
        }
    }

#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] buffer_playing: %d\n", playing);
#endif
    return playing;
}

gint
xfade_written_time()
{
    if (!output_opened)
        return 0;
    return (gint) (streampos * 1000 / in_format.bps);
}

gint
xfade_output_time()
{
    gint time;

#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] output_time:\n");
#endif

    /* sanity check (note: this one _does_ happen all the time) */
    if (!output_opened)
        return 0;

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    time = the_op->output_time() - output_offset;
    if (time < 0)
        time = 0;

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);

#ifdef DEBUG_HARDCORE
    AUDDBG("[crossfade] output_time: time=%d\n", time);
#endif
    return time;
}

void
xfade_cleanup()
{
    AUDDBG("[crossfade] cleanup:\n");

    /* lock buffer */
    MUTEX_LOCK(&buffer_mutex);

    /* check if buffer thread is still running */
    if (output_opened)
    {
        AUDDBG("[crossfade] cleanup: closing output\n");

        stopped = TRUE;

        /* wait for buffer thread to clean up and terminate */
        MUTEX_UNLOCK(&buffer_mutex);
        if (THREAD_JOIN(buffer_thread))
            PERROR("[crossfade] close: thread_join()");
        MUTEX_LOCK(&buffer_mutex);
    }

    /* unlock buffer */
    MUTEX_UNLOCK(&buffer_mutex);

    AUDDBG("[crossfade] cleanup: done\n");
}

static gint
calc_bitrate(gint fmt, gint rate, gint nch)
{
    gint bitrate = rate * nch;

    if (!in_format.is_8bit)
        bitrate *= 2;

    return bitrate;
}
