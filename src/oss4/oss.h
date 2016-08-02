/*
 * oss.h
 * Copyright 2010-2012 Michał Lipski
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

#ifndef AUDACIOUS_OSS4_H
#define AUDACIOUS_OSS4_H

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#include <soundcard.h>
#endif

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#define DESCRIBE_ERROR AUDERR("%s\n", oss_describe_error())

#define CHECK(function, ...) \
do { \
    int CHECK_error = function(__VA_ARGS__); \
    if (CHECK_error < 0) { \
        DESCRIBE_ERROR; \
        goto FAILED; \
    } \
} while (0)

#define CHECK_STR(str, function, ...) \
do { \
    int CHECK_STR_error = function(__VA_ARGS__); \
    if (CHECK_STR_error < 0) { \
        str = String(str_printf("OSS error: %s\n", oss_describe_error())); \
        goto FAILED; \
    } \
} while (0)

#define DEFAULT_MIXER "/dev/mixer"
#define DEFAULT_DSP "/dev/dsp"

struct PreferencesWidget;

class OSSPlugin : public OutputPlugin
{
public:
    static const char about[];
    static const char *const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
#ifdef SNDCTL_SYSINFO
        N_("OSS4 Output"),
#else
        N_("OSS3 Output"),
#endif
        PACKAGE,
        about,
        &prefs
    };

    // OSS4 is preferred over ALSA (priority 5).
    // ALSA is preferred over OSS3.
#ifdef SNDCTL_SYSINFO
    constexpr OSSPlugin() : OutputPlugin(info, 6) {}
#else
    constexpr OSSPlugin() : OutputPlugin(info, 4) {}
#endif

    bool init();

    StereoVolume get_volume();
    void set_volume(StereoVolume v);

    bool open_audio(int aud_format, int rate, int chans, String &error);
    void close_audio();

    void period_wait();
    int write_audio(const void *data, int size);
    void drain();

    int get_delay();

    void pause(bool pause);
    void flush();

private:
    bool set_format(int format, int rate, int channels, String &error);
    bool set_buffer(String &error);

    int frames_to_bytes(int frames) const
        { return frames * (m_bytes_per_sample * m_channels); }
    int bytes_to_frames(int bytes) const
        { return bytes / (m_bytes_per_sample * m_channels); }

    int m_fd = -1;
    int m_format = 0;
    int m_rate = 0;
    int m_channels = 0;
    int m_bytes_per_sample = 0;

    bool m_ioctl_vol = false;
};

/* utils.c */
int oss_convert_aud_format(int aud_format);
const char *oss_format_to_text(int format);
int oss_format_to_bytes(int format);
const char *oss_describe_error();

#ifdef SNDCTL_SYSINFO
int oss_probe_for_adev(oss_sysinfo *sysinfo);
#endif

bool oss_hardware_present();

#endif
