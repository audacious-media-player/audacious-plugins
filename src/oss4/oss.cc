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

constexpr StereoVolume to_stereo_volume(int vol)
    { return {vol & 0x00ff, vol >> 8}; }
constexpr int from_stereo_volume(StereoVolume v)
    { return v.left | (v.right << 8); }

const char * const OSSPlugin::defaults[] = {
 "device", DEFAULT_DSP,
 "use_alt_device", "FALSE",
 "alt_device", DEFAULT_DSP,
 "save_volume", "TRUE",
 "volume", aud::numeric_string<0x3232>::str,
 "cookedmode", "TRUE",
 "exclusive", "FALSE",
 nullptr};

static int poll_pipe[2];
static pollfd poll_handles[2];

bool OSSPlugin::init()
{
    AUDDBG("Init.\n");

    aud_config_set_defaults("oss4", defaults);

    return oss_hardware_present();
}

bool OSSPlugin::set_format(int format, int rate, int channels, String &error)
{
    int param;

    AUDDBG("Audio format: %s, sample rate: %dHz, number of channels: %d.\n", oss_format_to_text(format), rate, channels);

#ifdef SNDCTL_DSP_COOKEDMODE
    /* Enable/disable format conversions made by the OSS software */
    param = aud_get_bool("oss4", "cookedmode");
    CHECK(ioctl, m_fd, SNDCTL_DSP_COOKEDMODE, &param);

    AUDDBG("%s format conversions made by the OSS software.\n", param ? "Enabled" : "Disabled");
#endif

    param = format;
    CHECK_STR(error, ioctl, m_fd, SNDCTL_DSP_SETFMT, &param);

    if (param != format)
    {
        error = String("Selected audio format is not supported by the device.");
        goto FAILED;
    }

    param = rate;
    CHECK_STR(error, ioctl, m_fd, SNDCTL_DSP_SPEED, &param);

    if (param < rate * 9 / 10 || param > rate * 11 / 10)
    {
        error = String("Selected sample rate is not supported by the device.");
        goto FAILED;
    }

    param = channels;
    CHECK_STR(error, ioctl, m_fd, SNDCTL_DSP_CHANNELS, &param);

    if (param != channels)
    {
        error = String("Selected number of channels is not supported by the device.");
        goto FAILED;
    }

    m_format = format;
    m_rate = rate;
    m_channels = channels;
    m_bytes_per_sample = oss_format_to_bytes(m_format);

    return true;

FAILED:
    return false;
}

static int log2(int x)
{
    int y = 0;

    while((x >>= 1))
        y++;

    return y;
}

bool OSSPlugin::set_buffer(String &error)
{
    int milliseconds = aud_get_int(nullptr, "output_buffer_size");
    int bytes = frames_to_bytes(aud::rescale(milliseconds, 1000, m_rate));
    int fragorder = aud::clamp(log2(bytes / 4), 9, 15);
    int numfrags = aud::clamp(aud::rdiv(bytes, 1 << fragorder), 4, 32767);

    int param = (numfrags << 16) | fragorder;
    CHECK_STR(error, ioctl, m_fd, SNDCTL_DSP_SETFRAGMENT, &param);

    return true;

FAILED:
    return false;
}

static int open_device()
{
    int res = -1;
    int flags = O_WRONLY | O_NONBLOCK;
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

static void close_device(int &fd)
{
    close(fd);
    fd = -1;
}

static bool poll_setup(int fd)
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
    poll_handles[1].fd = fd;
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
        while (read(poll_pipe[0], &c, 1) == 1)
            ;
    }
}

static void poll_wake()
{
    const char c = 0;
    if (write(poll_pipe[1], &c, 1) < 0)
        AUDERR("Failed to write to pipe: %s.\n", strerror(errno));
}

static void poll_cleanup()
{
    close(poll_pipe[0]);
    close(poll_pipe[1]);
}

bool OSSPlugin::open_audio(int aud_format, int rate, int channels, String &error)
{
    AUDDBG("Opening audio.\n");

    int format;
    audio_buf_info buf_info;
    bool poll_was_setup = false;

    CHECK_STR(error, m_fd = open_device);

    if (poll_setup(m_fd))
        poll_was_setup = true;
    else
        goto FAILED;

    if ((format = oss_convert_aud_format(aud_format)) < 0)
    {
        error = String("Unsupported audio format");
        goto FAILED;
    }

    if (!set_format(format, rate, channels, error))
        goto FAILED;

    if (!set_buffer(error))
        goto FAILED;

    memset(&buf_info, 0, sizeof buf_info);
    CHECK_STR(error, ioctl, m_fd, SNDCTL_DSP_GETOSPACE, &buf_info);

    AUDINFO("Buffer information, fragstotal: %d, fragsize: %d, bytes: %d.\n",
        buf_info.fragstotal,
        buf_info.fragsize,
        buf_info.bytes);

    m_ioctl_vol = true;

    if (aud_get_bool("oss4", "save_volume"))
        set_volume(to_stereo_volume(aud_get_int("oss4", "volume")));

    return true;

FAILED:
    if (poll_was_setup)
        poll_cleanup();
    if (m_fd >= 0)
        close_device(m_fd);
    return false;
}

void OSSPlugin::close_audio()
{
    AUDDBG("Closing audio.\n");

    poll_cleanup();
    close_device(m_fd);
}

int OSSPlugin::write_audio(const void *data, int length)
{
    int written = write(m_fd, data, length);

    if (written < 0)
    {
        if (errno != EAGAIN)
            DESCRIBE_ERROR;

        return 0;
    }

    return written;
}

void OSSPlugin::period_wait()
{
    poll_sleep();
}

void OSSPlugin::drain()
{
    AUDDBG("Drain.\n");

    if (ioctl(m_fd, SNDCTL_DSP_SYNC, nullptr) == -1)
        DESCRIBE_ERROR;
}

int OSSPlugin::get_delay()
{
    int delay_bytes = 0;
    CHECK(ioctl, m_fd, SNDCTL_DSP_GETODELAY, &delay_bytes);

FAILED:
    return aud::rescale<int64_t>(bytes_to_frames(delay_bytes), m_rate, 1000);
}

void OSSPlugin::flush()
{
    AUDDBG("Flush.\n");

    CHECK(ioctl, m_fd, SNDCTL_DSP_RESET, nullptr);

FAILED:
    poll_wake();
}

void OSSPlugin::pause(bool pause)
{
    AUDDBG("%sause.\n", pause ? "P" : "Unp");

#ifdef SNDCTL_DSP_SILENCE
    if (pause)
        CHECK(ioctl, m_fd, SNDCTL_DSP_SILENCE, nullptr);
    else
        CHECK(ioctl, m_fd, SNDCTL_DSP_SKIP, nullptr);

FAILED:
    return;
#endif
}

StereoVolume OSSPlugin::get_volume()
{
#ifdef SNDCTL_DSP_GETPLAYVOL
    int vol = 0;

    if (m_fd == -1 || !m_ioctl_vol)
    {
        if (aud_get_bool("oss4", "save_volume"))
            return to_stereo_volume(aud_get_int("oss4", "volume"));

        goto FAILED;
    }

    CHECK(ioctl, m_fd, SNDCTL_DSP_GETPLAYVOL, &vol);

    aud_set_int("oss4", "volume", vol);

    return to_stereo_volume(vol);

FAILED:
    if (errno == EINVAL)
        m_ioctl_vol = false;
#endif

    return to_stereo_volume(0);
}

void OSSPlugin::set_volume(StereoVolume v)
{
#ifdef SNDCTL_DSP_SETPLAYVOL
    int vol = from_stereo_volume(v);

    if (aud_get_bool("oss4", "save_volume"))
        aud_set_int("oss4", "volume", vol);

    if (m_fd == -1 || !m_ioctl_vol)
        return;

    CHECK(ioctl, m_fd, SNDCTL_DSP_SETPLAYVOL, &vol);

    return;

FAILED:
    if (errno == EINVAL)
        m_ioctl_vol = false;
#endif
}
