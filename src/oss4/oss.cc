/*
 * oss.c
 * Copyright 2010-2012 Michał Lipski
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

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include <poll.h>

static const char * const oss_defaults[] = {
 "device", DEFAULT_DSP,
 "use_alt_device", "FALSE",
 "alt_device", DEFAULT_DSP,
 "save_volume", "TRUE",
 "volume", "12850", /* 0x3232 */
 "cookedmode", "TRUE",
 "exclusive", "FALSE",
 nullptr};

oss_data_t *oss_data;
static int64_t oss_time; /* microseconds */
static bool oss_paused;
static int oss_paused_time;
static int oss_delay; /* miliseconds */
static bool oss_ioctl_vol = false;

static int poll_pipe[2];
static pollfd poll_handles[2];

bool oss_init()
{
    AUDDBG("Init.\n");

    aud_config_set_defaults("oss4", oss_defaults);
    oss_data = (oss_data_t *) malloc(sizeof(oss_data_t));
    oss_data->fd = -1;

    return oss_hardware_present();
}

void oss_cleanup()
{
    AUDDBG("Cleanup.\n");

    free(oss_data);
}

static bool set_format(int format, int rate, int channels)
{
    int param;

    AUDDBG("Audio format: %s, sample rate: %dHz, number of channels: %d.\n", oss_format_to_text(format), rate, channels);

#ifdef SNDCTL_DSP_COOKEDMODE
    /* Enable/disable format conversions made by the OSS software */
    param = aud_get_bool("oss4", "cookedmode");
    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_COOKEDMODE, &param);
#endif

    AUDDBG("%s format conversions made by the OSS software.\n", param ? "Enabled" : "Disabled");

    param = format;
    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_SETFMT, &param);
    CHECK_VAL(param == format, ERROR_NOISY, "Selected audio format is not supported by the device.\n");

    param = rate;
    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_SPEED, &param);
    CHECK_VAL(param >= rate * 9 / 10 && param <= rate * 11 / 10, ERROR_NOISY,
     "Selected sample rate is not supported by the device.\n");

    param = channels;
    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_CHANNELS, &param);
    CHECK_VAL(param == channels, ERROR_NOISY, "Selected number of channels is not supported by the device.\n");

    oss_data->format = format;
    oss_data->rate = rate;
    oss_data->channels = channels;
    oss_data->bits_per_sample = oss_format_to_bits(oss_data->format);

    return true;

FAILED:
    return false;
}

static int open_device()
{
    int res = -1;
    int flags = O_WRONLY;
    String device = aud_get_str("oss4", "device");
    String alt_device = aud_get_str("oss4", "alt_device");

    if (aud_get_bool("oss4", "exclusive"))
    {
        AUDDBG("Enabled exclusive mode.\n");
        flags |= O_EXCL;
    }

    if (aud_get_bool("oss4", "use_alt_device") && alt_device != nullptr)
        res = open(alt_device, flags);
    else if (device != nullptr)
        res = open(device, flags);
    else
        res = open(DEFAULT_DSP, flags);

    return res;
}

static void close_device()
{
    close(oss_data->fd);
    oss_data->fd = -1;
}

static bool poll_setup()
{
    if (pipe(poll_pipe))
    {
        AUDERR("Failed to create pipe: %s.\n", strerror(errno));
        return false;
    }

    if (fcntl(poll_pipe[0], F_SETFL, O_NONBLOCK))
    {
        AUDERR("Failed to set O_NONBLOCK on pipe: %s.\n", strerror(errno));
        close(poll_pipe[0]);
        close(poll_pipe[1]);
        return false;
    }

    poll_handles[0].fd = poll_pipe[0];
    poll_handles[0].events = POLLIN;
    poll_handles[1].fd = oss_data->fd;
    poll_handles[1].events = POLLOUT;

    return true;
}

static void poll_sleep()
{
    if (poll(poll_handles, 2, -1) < 0)
    {
        AUDERR("Failed to poll: %s.\n", strerror(errno));
        return;
    }

    if (poll_handles[0].revents & POLLIN)
    {
        char c;
        while (read (poll_pipe[0], &c, 1) == 1)
            ;
    }
}

static void poll_wake()
{
    const char c = 0;
    if (write (poll_pipe[1], &c, 1) < 0)
        AUDERR("Failed to write to pipe: %s.\n", strerror(errno));
}

static void poll_cleanup()
{
    close (poll_pipe[0]);
    close (poll_pipe[1]);
}

bool oss_open_audio(int aud_format, int rate, int channels)
{
    AUDDBG("Opening audio.\n");

    int format;
    int vol_left, vol_right;
    audio_buf_info buf_info;

    CHECK_NOISY(oss_data->fd = open_device);

    if (!poll_setup())
        goto CLOSE;

    format = oss_convert_aud_format(aud_format);

    if (!set_format(format, rate, channels))
        goto FAILED;

    memset(&buf_info, 0, sizeof buf_info);
    CHECK_NOISY(ioctl, oss_data->fd, SNDCTL_DSP_GETOSPACE, &buf_info);

    AUDDBG("Buffer information, fragstotal: %d, fragsize: %d, bytes: %d.\n",
        buf_info.fragstotal,
        buf_info.fragsize,
        buf_info.bytes);

    oss_time = 0;
    oss_paused = false;
    oss_paused_time = 0;
    oss_delay = oss_bytes_to_frames(buf_info.fragstotal * buf_info.fragsize) * 1000 / oss_data->rate;
    oss_ioctl_vol = true;

    AUDDBG("Internal OSS buffer size: %dms.\n", oss_delay);

    if (aud_get_bool("oss4", "save_volume"))
    {
        vol_right = (aud_get_int("oss4", "volume") & 0xFF00) >> 8;
        vol_left  = (aud_get_int("oss4", "volume") & 0x00FF);
        oss_set_volume(vol_left, vol_right);
    }

    return true;

FAILED:
    poll_cleanup();
CLOSE:
    close_device();
    return false;
}

void oss_close_audio()
{
    AUDDBG ("Closing audio.\n");

    poll_cleanup();
    close_device();
}

void oss_write_audio(void *data, int length)
{
    int written = 0, start = 0;

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
        oss_time += (int64_t) oss_bytes_to_frames(written) * 1000000 / oss_data->rate;
    }
}

void oss_period_wait()
{
    poll_sleep();
}

void oss_drain()
{
    AUDDBG("Drain.\n");

    if (ioctl(oss_data->fd, SNDCTL_DSP_SYNC, nullptr) == -1)
        DESCRIBE_ERROR;
}

int oss_buffer_free()
{
    audio_buf_info buf_info;

    if (oss_paused)
        return 0;

    memset(&buf_info, 0, sizeof buf_info);
    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_GETOSPACE, &buf_info);

    return buf_info.bytes;

FAILED:
    return 0;
}

static int real_output_time()
{
    int  time = 0;

    time = (oss_time - (int64_t) (oss_delay * 1000)) / 1000;
    return time;
}

int oss_output_time()
{
    int time = 0;

    if (oss_paused)
        time = oss_paused_time;
    else
        time = real_output_time();
    return time;
}

void oss_flush(int time)
{
    AUDDBG("Flush.\n");

    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_RESET, nullptr);

FAILED:
    oss_time = (int64_t) time * 1000;
    oss_paused_time = time;

    poll_wake();
}

void oss_pause(bool pause)
{
    AUDDBG("%sause.\n", pause ? "P" : "Unp");

#ifdef SNDCTL_DSP_SILENCE
    if (pause)
    {
        oss_paused_time = real_output_time();
        CHECK(ioctl, oss_data->fd, SNDCTL_DSP_SILENCE, nullptr);
    }
    else
        CHECK(ioctl, oss_data->fd, SNDCTL_DSP_SKIP, nullptr);

FAILED:
#endif
    oss_paused = pause;
}

void oss_get_volume(int *left, int *right)
{
    *left = *right = 0;

#ifdef SNDCTL_DSP_GETPLAYVOL
    int vol = 0;

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
        oss_ioctl_vol = false;
#endif
}

void oss_set_volume(int left, int right)
{
#ifdef SNDCTL_DSP_SETPLAYVOL
    int vol = (right << 8) | left;

    if (aud_get_int("oss4", "save_volume"))
        aud_set_int("oss4", "volume", vol);

    if (oss_data->fd == -1 || !oss_ioctl_vol)
        return;

    CHECK(ioctl, oss_data->fd, SNDCTL_DSP_SETPLAYVOL, &vol);

    return;

FAILED:
    if (errno == EINVAL)
        oss_ioctl_vol = false;
#endif
}
