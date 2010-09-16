/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

extern void close_mixer_device();

#include <glib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <audacious/audconfig.h>

#include "OSS.h"


#define NFRAGS		32

static gint fd = 0;
static char *buffer;
static gboolean going, prebuffer, paused, unpause, do_pause, remove_prebuffer;
static gint device_buffer_used, buffer_size, prebuffer_size, blk_size;
static gint rd_index = 0, wr_index = 0;
static gint output_time_offset = 0;
static guint64 written = 0, output_bytes = 0;
static gint flush;
static gint fragsize, device_buffer_size;
static gchar *device_name;
static GThread *buffer_thread;
static gboolean select_works;

struct format_info {
    union {
        gint xmms;
        int oss;
    } format;
    int frequency;
    int channels;
    int bps;
};


/*
 * The format of the data from the input plugin
 * This will never change during a song.
 */
struct format_info input;

/*
 * The format of the data we actually send to the soundcard.
 * This might be different from effect if we need to resample or do
 * some other format conversion.
 */
struct format_info output;


static void
oss_calc_device_buffer_used(void)
{
    audio_buf_info buf_info;
    if (paused)
        device_buffer_used = 0;
    else if (!ioctl(fd, SNDCTL_DSP_GETOSPACE, &buf_info))
        device_buffer_used =
            (buf_info.fragstotal * buf_info.fragsize) - buf_info.bytes;
}


static int
oss_calc_bitrate(int oss_fmt, int rate, int channels)
{
    int bitrate = rate * channels;

    if (oss_fmt == AFMT_U16_BE || oss_fmt == AFMT_U16_LE ||
        oss_fmt == AFMT_S16_BE || oss_fmt == AFMT_S16_LE)
        bitrate *= 2;

    return bitrate;
}

static int
oss_get_format(gint fmt)
{
    int format = 0;

    switch (fmt) {
    case FMT_U8:
        format = AFMT_U8;
        break;
    case FMT_S8:
        format = AFMT_S8;
        break;
    case FMT_U16_LE:
        format = AFMT_U16_LE;
        break;
    case FMT_U16_BE:
        format = AFMT_U16_BE;
        break;
    case FMT_S16_LE:
        format = AFMT_S16_LE;
        break;
    case FMT_S16_BE:
        format = AFMT_S16_BE;
        break;
    default:
        format = -1;
    }

    return format;
}

static void
oss_setup_format(gint fmt, int rate, int nch)
{
    output.bps = oss_calc_bitrate(oss_get_format(fmt), rate, nch);
    output.format.oss = oss_get_format(fmt);
    output.frequency = rate;
    output.channels = nch;


    fragsize = 0;
    while ((1L << fragsize) < output.bps / 25)
        fragsize++;
    fragsize--;

    device_buffer_size = ((1L << fragsize) * (NFRAGS + 1));

    oss_set_audio_params();

    output.bps = oss_calc_bitrate(output.format.oss, output.frequency,
                                  output.channels);
}


gint
oss_get_written_time(void)
{
    if (!going)
        return 0;
    return (written * 1000) / output.bps;
}

gint
oss_get_output_time(void)
{
    guint64 bytes;

    if (!fd || !going)
        return 0;

    bytes = output_bytes < device_buffer_used ?
        0 : output_bytes - device_buffer_used;

    return output_time_offset + ((bytes * 1000) / output.bps);
}

static int
oss_used(void)
{
    if (wr_index >= rd_index)
        return wr_index - rd_index;

    return buffer_size - (rd_index - wr_index);
}

gint
oss_playing(void)
{
    if (!going)
        return 0;
    if (!oss_used() && (device_buffer_used - (3 * blk_size)) <= 0)
        return FALSE;

    return TRUE;
}

gint
oss_free(void)
{
    if (remove_prebuffer && prebuffer) {
        prebuffer = FALSE;
        remove_prebuffer = FALSE;
    }

    if (prebuffer)
        remove_prebuffer = TRUE;

    if (rd_index > wr_index)
        return (rd_index - wr_index) - device_buffer_size - 1;

    return (buffer_size - (wr_index - rd_index)) - device_buffer_size - 1;
}

static void
oss_write_audio(gpointer data, size_t length)
{
    size_t done = 0;
    do {
        ssize_t n = write(fd, (gchar *) data + done, length - done);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        }
        done += n;
    } while (length > done);

    output_bytes += done;
}

void
oss_write(gpointer ptr, int length)
{
    int cnt, off = 0;

    remove_prebuffer = FALSE;

    written += length;
    while (length > 0) {
        cnt = MIN(length, buffer_size - wr_index);
        memcpy(buffer + wr_index, (char *) ptr + off, cnt);
        wr_index = (wr_index + cnt) % buffer_size;
        length -= cnt;
        off += cnt;
    }
}

void
oss_close(void)
{
    if (!going)
        return;
    going = 0;

    g_thread_join(buffer_thread);

    g_free(device_name);
    wr_index = 0;
    rd_index = 0;

    close_mixer_device();
}

void
oss_flush(gint time)
{
    flush = time;
    while (flush != -1)
        g_usleep(10000);
}

void
oss_pause(short p)
{
    if (p == TRUE)
        do_pause = TRUE;
    else
        unpause = TRUE;
}

gpointer
oss_loop(gpointer arg)
{
    gint length, cnt;
    fd_set set;
    struct timeval tv;

    while (going) {
        if (oss_used() > prebuffer_size)
            prebuffer = FALSE;
        if (oss_used() > 0 && !paused && !prebuffer) {
            tv.tv_sec = 0;
            tv.tv_usec = 10000;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            if (!select_works || (select(fd + 1, NULL, &set, NULL, &tv) > 0)) {
                length = MIN(blk_size, oss_used());
                while (length > 0) {
                    cnt = MIN(length, buffer_size - rd_index);
                    oss_write_audio(buffer + rd_index, cnt);
                    rd_index = (rd_index + cnt) % buffer_size;
                    length -= cnt;
                }
                if (!oss_used())
                    ioctl(fd, SNDCTL_DSP_POST, 0);
            }
        }
        else
            g_usleep(10000);
        oss_calc_device_buffer_used();
        if (do_pause && !paused) {
            do_pause = FALSE;
            paused = TRUE;

            ioctl(fd, SNDCTL_DSP_SYNC, 0);
        }
        else if (unpause && paused) {
            unpause = FALSE;
            close(fd);
            fd = open(device_name, O_WRONLY);
            oss_set_audio_params();
            paused = FALSE;
        }

        if (flush != -1) {
            /*
             * This close and open is a work around of a
             * bug that exists in some drivers which cause
             * the driver to get fucked up by a reset
             */

            ioctl(fd, SNDCTL_DSP_SYNC, 0);
            close(fd);
            fd = open(device_name, O_WRONLY);
            oss_set_audio_params();
            output_time_offset = flush;
            written = ((guint64) flush * input.bps) / 1000;
            rd_index = wr_index = output_bytes = 0;
            flush = -1;
            prebuffer = TRUE;
        }

    }

    ioctl(fd, SNDCTL_DSP_SYNC, 0);
    close(fd);
    g_free(buffer);
    return NULL;
}

void
oss_set_audio_params(void)
{
    int frag, stereo, ret;
    struct timeval tv;
    fd_set set;

    ioctl(fd, SNDCTL_DSP_SYNC, 0);
    frag = (NFRAGS << 16) | fragsize;
    ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);
    /*
     * Set the stream format.  This ioctl() might fail, but should
     * return a format that works if it does.
     */
    ioctl(fd, SNDCTL_DSP_SETFMT, &output.format.oss);
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &output.format.oss) == -1)
        g_warning("SNDCTL_DSP_SETFMT ioctl failed: %s", strerror(errno));

    stereo = output.channels - 1;
    ioctl(fd, SNDCTL_DSP_STEREO, &stereo);
    output.channels = stereo + 1;

    if (ioctl(fd, SNDCTL_DSP_SPEED, &output.frequency) == -1)
        g_warning("SNDCTL_DSP_SPEED ioctl failed: %s", strerror(errno));

    if (ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &blk_size) == -1)
        blk_size = 1L << fragsize;

    /*
     * Stupid hack to find out if the driver support selects, some
     * drivers won't work properly without a select and some won't
     * work with a select :/
     */

    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    ret = select(fd + 1, NULL, &set, NULL, &tv);
    if (ret > 0)
        select_works = TRUE;
    else
        select_works = FALSE;
}

gint
oss_open(gint fmt, gint rate, gint nch)
{

    if (oss_cfg.use_alt_audio_device && oss_cfg.alt_audio_device)
        device_name = g_strdup(oss_cfg.alt_audio_device);
    else {
        if (oss_cfg.audio_device > 0)
            device_name =
                g_strdup_printf("%s%d", DEV_DSP, oss_cfg.audio_device);
        else
            device_name = g_strdup(DEV_DSP);
    }

    fd = open(device_name, O_WRONLY);

    if (fd == -1) {
        g_warning("oss_open(): Failed to open audio device (%s): %s",
                  device_name, strerror(errno));
        g_free(device_name);
        return 0;
    }

    input.format.xmms = fmt;
    input.frequency = rate;
    input.channels = nch;
    input.bps = oss_calc_bitrate(oss_get_format(fmt), rate, nch);

    oss_setup_format(fmt, rate, nch);

    buffer_size = aud_cfg->output_buffer_size * input.bps / 1000;

    if (buffer_size < 8192)
        buffer_size = 8192;

    prebuffer_size = (buffer_size * oss_cfg.prebuffer) / 100;
    if (buffer_size - prebuffer_size < 4096)
        prebuffer_size = buffer_size - 4096;

    buffer_size += device_buffer_size;
    buffer = g_malloc0(buffer_size);

    flush = -1;
    prebuffer = TRUE;
    wr_index = rd_index = output_time_offset = written = output_bytes = 0;
    paused = FALSE;
    do_pause = FALSE;
    unpause = FALSE;
    remove_prebuffer = FALSE;

    going = 1;

    buffer_thread = g_thread_create(oss_loop, NULL, TRUE, NULL);

    return 1;
}

void oss_tell(gint * fmt, gint * rate, gint * nch)
{
	(*fmt) = input.format.xmms;
	(*rate) = input.frequency;
	(*nch) = input.channels;
}
