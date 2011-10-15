/*
 * OSS4 Output Plugin for Audacious
 * Copyright 2010 Michał Lipski <tallica@o2.pl>
 *
 * I would like to thank people on #audacious, especially Tony Vroon and
 * John Lindgren and of course the authors of the previous OSS plugin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "oss.h"

oss_data_t *oss_data;
oss_cfg_t *oss_cfg;
gchar *oss_message = NULL;
static gint64 oss_time; /* microseconds */
static gboolean oss_paused;
static gint oss_paused_time;
static audio_buf_info oss_buffer_info;
static gint oss_delay; /* miliseconds */
static gboolean oss_ioctl_vol = FALSE;

gboolean oss_init(void)
{
    AUDDBG("Init.\n");

    oss_data = g_new0(oss_data_t, 1);
    oss_cfg = g_new0(oss_cfg_t, 1);
    oss_data->fd = -1;
    oss_cfg->device = DEFAULT_DSP;
    oss_cfg->use_alt_device = FALSE;
    oss_cfg->alt_device = DEFAULT_DSP;
    oss_cfg->save_volume = TRUE;
    oss_cfg->volume = 0x3232;
    oss_cfg->cookedmode = TRUE;

    oss_config_load();

    if (oss_hardware_present())
        return TRUE;
    else
        return FALSE;
}

void oss_cleanup(void)
{
    AUDDBG("Cleanup.\n");

    oss_config_save();

    g_free(oss_data);
    g_free(oss_cfg);
}

static gboolean set_format(gint format, gint rate, gint channels)
{
    gint param;

    AUDDBG("Audio format: %s, sample rate: %dHz, number of channels: %d.\n", oss_format_to_text(format), rate, channels);

    /* Enable/disable format conversions made by the OSS software */
    if (ioctl(oss_data->fd, SNDCTL_DSP_COOKEDMODE, &oss_cfg->cookedmode) == -1)
        DEBUG_MSG;

    param = format;

    if (ioctl(oss_data->fd, SNDCTL_DSP_SETFMT, &param) == -1)
        goto FAILED;

    if (param != format)
    {
        ERROR("Selected audio format is not supported by the device.\n");
        return FALSE;
    }

    param = rate;

    if (ioctl(oss_data->fd, SNDCTL_DSP_SPEED, &param) == -1)
        goto FAILED;

    if (param != rate)
    {
        ERROR("Selected sample rate is not supported by the device.\n");
        return FALSE;
    }

    param = channels;

    if (ioctl(oss_data->fd, SNDCTL_DSP_CHANNELS, &param) == -1)
        goto FAILED;

    if (param != channels)
    {
        ERROR("Selected number of channels is not supported by the device.\n");
        return FALSE;
    }

    oss_data->format = format;
    oss_data->rate = rate;
    oss_data->channels = channels;
    oss_data->bits_per_sample = oss_format_to_bits(oss_data->format);

    return TRUE;

    FAILED:
        ERROR_MSG;
        return FALSE;
}

gint oss_open_audio(gint aud_format, gint rate, gint channels)
{
    AUDDBG("Opening audio.\n");

    gint format;
    gint vol_left, vol_right;
    gchar *device = DEFAULT_DSP;

    if (oss_cfg->use_alt_device && oss_cfg->alt_device != NULL)
        device = oss_cfg->alt_device;
    else if (oss_cfg->device != NULL)
        device = oss_cfg->device;

    oss_data->fd = open(device, O_WRONLY);

    if (oss_data->fd == -1)
    {
        ERROR_MSG;
        goto FAILED;
    }

    format = oss_convert_aud_format(aud_format);

    if (!set_format(format, rate, channels))
        goto FAILED;

    if (ioctl(oss_data->fd, SNDCTL_DSP_GETOSPACE, &oss_buffer_info) == -1)
        DEBUG_MSG;

    AUDDBG("Buffer information, fragstotal: %d, fragsize: %d, bytes: %d.\n",
        oss_buffer_info.fragstotal,
        oss_buffer_info.fragsize,
        oss_buffer_info.bytes);

    oss_time = 0;
    oss_paused = FALSE;
    oss_paused_time = 0;
    oss_delay = oss_bytes_to_frames(oss_buffer_info.fragstotal * oss_buffer_info.fragsize) * 1000 / oss_data->rate;
    oss_ioctl_vol = TRUE;

    AUDDBG("Internal OSS buffer size: %dms.\n", oss_delay);

    if (oss_cfg->save_volume)
    {
        vol_right = (oss_cfg->volume & 0xFF00) >> 8;
        vol_left  = (oss_cfg->volume & 0x00FF);

        oss_set_volume(vol_left, vol_right);
    }

    return 1;

    FAILED:
        close(oss_data->fd);
        oss_data->fd = -1;
        return 0;
}

void oss_close_audio(void)
{
    AUDDBG ("Closing audio.\n");

    if (ioctl(oss_data->fd, SNDCTL_DSP_HALT_OUTPUT, NULL) == -1)
        DEBUG_MSG;

    if (oss_data->fd != -1)
        close(oss_data->fd);
    oss_data->fd = -1;
}

void oss_write_audio(void *data, gint length)
{
    gint written = 0, start = 0;

    while (length > 0)
    {
        written = write(oss_data->fd, data + start, length);

        if (written < 0)
        {
            ERROR_MSG;

            if (ioctl(oss_data->fd, SNDCTL_DSP_HALT_OUTPUT, NULL) == -1)
                DEBUG_MSG;

            return;
        }

        length -= written;
        start += written;
        oss_time += (gint64) oss_bytes_to_frames(written) * 1000000 / oss_data->rate;
    }
}

void oss_drain(void)
{
    AUDDBG("Drain.\n");

    if (ioctl(oss_data->fd, SNDCTL_DSP_SYNC, NULL) == -1)
        DEBUG_MSG;
}

gint oss_buffer_free(void)
{
    if (oss_paused)
        return 0;

    ioctl(oss_data->fd, SNDCTL_DSP_GETOSPACE, &oss_buffer_info);
    return oss_buffer_info.bytes;
}

void oss_set_written_time(gint time)
{
    oss_time = 1000 * (gint64) time;
}

gint oss_written_time(void)
{
    gint time;

    time = oss_time / 1000;
    return time;
}

static gint real_output_time()
{
    gint  time = 0;

    time = (oss_time - (gint64) (oss_delay * 1000)) / 1000;
    return time;
}

gint oss_output_time(void)
{
    gint time = 0;

    if (oss_paused)
        time = oss_paused_time;
    else
        time = real_output_time();
    return time;
}

void oss_flush(gint time)
{
    AUDDBG("Flush.\n");

    if (ioctl(oss_data->fd, SNDCTL_DSP_HALT_OUTPUT, NULL) == -1)
        DEBUG_MSG;

    if (ioctl(oss_data->fd, SNDCTL_DSP_SKIP, NULL) == -1)
        DEBUG_MSG;

    oss_time = (gint64) time * 1000;
    oss_paused_time = time;
}

void oss_pause(gboolean pause)
{
    AUDDBG("%sause.\n", pause ? "P" : "Unp");

    if (pause)
    {
        oss_paused = TRUE;
        oss_paused_time = real_output_time();

        if (ioctl(oss_data->fd, SNDCTL_DSP_SILENCE, NULL) == -1)
            DEBUG_MSG;
    }
    else
    {
        if (ioctl(oss_data->fd, SNDCTL_DSP_SKIP, NULL) == -1)
            DEBUG_MSG;

        oss_paused = FALSE;
    }
}

void oss_get_volume(gint *left, gint *right)
{
    gint vol;

    if (oss_data->fd == -1 || !oss_ioctl_vol)
    {
        if (oss_cfg->save_volume)
        {
            *right = (oss_cfg->volume & 0xFF00) >> 8;
            *left  = (oss_cfg->volume & 0x00FF);
        }
        return;
    }

    if (ioctl(oss_data->fd, SNDCTL_DSP_GETPLAYVOL, &vol) == -1)
    {
        if (errno == EINVAL)
            oss_ioctl_vol = FALSE;

        DEBUG_MSG;
    }
    else
    {
        *right = (vol & 0xFF00) >> 8;
        *left  = (vol & 0x00FF);
        oss_cfg->volume = vol;
    }
}

void oss_set_volume(gint left, gint right)
{
    gint vol;

    vol = (right << 8) | left;

    if (oss_cfg->save_volume)
        oss_cfg->volume = vol;

    if (oss_data->fd == -1 || !oss_ioctl_vol)
        return;

    if (ioctl(oss_data->fd, SNDCTL_DSP_SETPLAYVOL, &vol) == -1)
    {
        if (errno == EINVAL)
            oss_ioctl_vol = FALSE;

        DEBUG_MSG;
    }
}
