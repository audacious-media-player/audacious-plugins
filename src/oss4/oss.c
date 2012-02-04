/*
 * oss.c
 * Copyright 2010-2011 Michał Lipski <tallica@o2.pl>
 *
 * I would like to thank people on #audacious, especially Tony Vroon and
 * John Lindgren and of course the authors of the previous OSS plugin.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include "oss.h"

static const gchar * const oss_defaults[] = {
 "device", DEFAULT_DSP,
 "use_alt_device", "FALSE",
 "alt_device", DEFAULT_DSP,
 "save_volume", "TRUE",
 "volume", "12850", /* 0x3232 */
 "cookedmode", "TRUE",
 "exclusive", "FALSE",
 NULL};

oss_data_t *oss_data;
static gint64 oss_time; /* microseconds */
static gboolean oss_paused;
static gint oss_paused_time;
static gint oss_delay; /* miliseconds */
static gboolean oss_ioctl_vol = FALSE;

gboolean oss_init(void)
{
    AUDDBG("Init.\n");

    aud_config_set_defaults("oss4", oss_defaults);
    oss_data = g_new0(oss_data_t, 1);
    oss_data->fd = -1;

    return oss_hardware_present();
}

void oss_cleanup(void)
{
    AUDDBG("Cleanup.\n");

    g_free(oss_data);
}

static gboolean set_format(gint format, gint rate, gint channels)
{
    gint param;

    AUDDBG("Audio format: %s, sample rate: %dHz, number of channels: %d.\n", oss_format_to_text(format), rate, channels);

    /* Enable/disable format conversions made by the OSS software */
    param = aud_get_bool("oss4", "cookedmode");
    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_COOKEDMODE, &param);

    AUDDBG("%s format conversions made by the OSS software.\n", param ? "Enabled" : "Disabled");

    param = format;
    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_SETFMT, &param);
    CHECK_VAL(param == format, ERROR_NOISY, "Selected audio format is not supported by the device.\n");

    param = rate;
    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_SPEED, &param);
    CHECK_VAL(param == rate, ERROR_NOISY, "Selected sample rate is not supported by the device.\n");

    param = channels;
    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_CHANNELS, &param);
    CHECK_VAL(param == channels, ERROR_NOISY, "Selected number of channels is not supported by the device.\n");

    oss_data->format = format;
    oss_data->rate = rate;
    oss_data->channels = channels;
    oss_data->bits_per_sample = oss_format_to_bits(oss_data->format);

    return TRUE;

FAILED:
    return FALSE;
}

static gint open_device(void)
{
    gint res = -1;
    gint flags = O_WRONLY;
    gchar *device = aud_get_string("oss4", "device");
    gchar *alt_device = aud_get_string("oss4", "alt_device");

    if (aud_get_bool("oss4", "exclusive"))
    {
        AUDDBG("Enabled exclusive mode.\n");
        flags |= O_EXCL;
    }

    if (aud_get_bool("oss4", "use_alt_device") && alt_device != NULL)
        res = open(alt_device, flags);
    else if (device != NULL)
        res = open(device, flags);
    else
        res = open(DEFAULT_DSP, flags);

    g_free(device);
    g_free(alt_device);

    return res;
}

static void close_device(void)
{
    close(oss_data->fd);
    oss_data->fd = -1;
}

gint oss_open_audio(gint aud_format, gint rate, gint channels)
{
    AUDDBG("Opening audio.\n");

    gint format;
    gint vol_left, vol_right;
    audio_buf_info buf_info;

    CHECK_NOISY(oss_data->fd = open_device);

    format = oss_convert_aud_format(aud_format);

    if (!set_format(format, rate, channels))
        goto FAILED;

    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_GETOSPACE, &buf_info);

    AUDDBG("Buffer information, fragstotal: %d, fragsize: %d, bytes: %d.\n",
        buf_info.fragstotal,
        buf_info.fragsize,
        buf_info.bytes);

    oss_time = 0;
    oss_paused = FALSE;
    oss_paused_time = 0;
    oss_delay = oss_bytes_to_frames(buf_info.fragstotal * buf_info.fragsize) * 1000 / oss_data->rate;
    oss_ioctl_vol = TRUE;

    AUDDBG("Internal OSS buffer size: %dms.\n", oss_delay);

    if (aud_get_bool("oss4", "save_volume"))
    {
        vol_right = (aud_get_int("oss4", "volume") & 0xFF00) >> 8;
        vol_left  = (aud_get_int("oss4", "volume") & 0x00FF);
        oss_set_volume(vol_left, vol_right);
    }

    return 1;

FAILED:
    close_device();
    return 0;
}

void oss_close_audio(void)
{
    AUDDBG ("Closing audio.\n");

    close_device();
}

void oss_write_audio(void *data, gint length)
{
    gint written = 0, start = 0;

    while (length > 0)
    {
        written = write(oss_data->fd, (char *) data + start, length);

        if (written < 0)
        {
            DESCRIBE_ERROR;
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
        DESCRIBE_ERROR;
}

gint oss_buffer_free(void)
{
    audio_buf_info buf_info;

    if (oss_paused)
        return 0;

    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_GETOSPACE, &buf_info);

    return MAX(0, buf_info.fragments - 1) * buf_info.fragsize;

FAILED:
    return 0;
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

    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_RESET, NULL);

FAILED:
    oss_time = (gint64) time * 1000;
    oss_paused_time = time;
}

void oss_pause(gboolean pause)
{
    AUDDBG("%sause.\n", pause ? "P" : "Unp");

    if (pause)
    {
        oss_paused_time = real_output_time();
        CHECK(ioctl, oss_data->fd, SNDCTL_DSP_SILENCE, NULL);
    }
    else
        CHECK(ioctl, oss_data->fd, SNDCTL_DSP_SKIP, NULL);

FAILED:
    oss_paused = pause;
}

void oss_get_volume(gint *left, gint *right)
{
    *left = *right = 0;

    gint vol;

    if (oss_data->fd == -1 || !oss_ioctl_vol)
    {
        if (aud_get_int("oss4", "save_volume"))
        {
            *right = (aud_get_int("oss4", "volume") & 0xFF00) >> 8;
            *left  = (aud_get_int("oss4", "volume") & 0x00FF);
        }
        return;
    }

    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_GETPLAYVOL, &vol);

    *right = (vol & 0xFF00) >> 8;
    *left  = (vol & 0x00FF);
    aud_set_int("oss4", "volume", vol);

    return;

FAILED:
    if (errno == EINVAL)
        oss_ioctl_vol = FALSE;
}

void oss_set_volume(gint left, gint right)
{
    gint vol = (right << 8) | left;

    if (aud_get_int("oss4", "save_volume"))
        aud_set_int("oss4", "volume", vol);

    if (oss_data->fd == -1 || !oss_ioctl_vol)
        return;

    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_SETPLAYVOL, &vol);

    return;

FAILED:
    if (errno == EINVAL)
        oss_ioctl_vol = FALSE;
}
