/*
 * utils.c
 * Copyright 2010-2011 Michał Lipski
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

char *oss_format_to_text(int format)
{
    const struct
    {
        int format;
        char *format_text;
    }
    table[] =
    {
#ifdef AFMT_FLOAT
        {AFMT_FLOAT,  "AFMT_FLOAT"},
#endif
        {AFMT_S8,     "AFMT_S8"},
        {AFMT_U8,     "AFMT_U8"},
        {AFMT_S16_LE, "AFMT_S16_LE"},
        {AFMT_S16_BE, "AFMT_S16_BE"},
        {AFMT_U16_LE, "AFMT_U16_LE"},
        {AFMT_U16_BE, "AFMT_U16_BE"},
        {AFMT_S24_LE, "AFMT_S24_LE"},
        {AFMT_S24_BE, "AFMT_S24_BE"},
        {AFMT_S32_LE, "AFMT_S32_LE"},
        {AFMT_S32_BE, "AFMT_S32_BE"},
    };

    int count;

    for (count = 0; count < N_ELEMENTS(table); count++)
    {
        if (table[count].format == format)
        {
            return table[count].format_text;
        }
    }

    return "FMT_UNKNOWN";
}

int oss_convert_aud_format(int aud_format)
{
    const struct
    {
        int aud_format;
        int format;
    }
    table[] =
    {
#ifdef AFMT_FLOAT
        {FMT_FLOAT,  AFMT_FLOAT},
#endif
        {FMT_S8,     AFMT_S8},
        {FMT_U8,     AFMT_U8},
        {FMT_S16_LE, AFMT_S16_LE},
        {FMT_S16_BE, AFMT_S16_BE},
        {FMT_U16_LE, AFMT_U16_LE},
        {FMT_U16_BE, AFMT_U16_BE},
        {FMT_S24_LE, AFMT_S24_LE},
        {FMT_S24_BE, AFMT_S24_BE},
        {FMT_S32_LE, AFMT_S32_LE},
        {FMT_S32_BE, AFMT_S32_BE},
    };

    int count;

    for (count = 0; count < N_ELEMENTS(table); count++)
    {
        if (table[count].aud_format == aud_format)
        {
            return table[count].format;
        }
    }

    return -1;
}

int oss_format_to_bits(int format)
{
    char bits;

    switch (format)
    {
        case AFMT_U8:
        case AFMT_S8:
            bits = 8;
            break;
        case AFMT_S16_LE:
        case AFMT_S16_BE:
        case AFMT_U16_LE:
        case AFMT_U16_BE:
            bits = 16;
            break;
        case AFMT_S24_LE:
        case AFMT_S24_BE:
        case AFMT_S32_LE:
        case AFMT_S32_BE:
            bits = 32;
            break;
#ifdef AFMT_FLOAT
        case AFMT_FLOAT:
            bits = sizeof(float) * 8;
            break;
#endif
        default:
            bits = 8;
    }

    return bits;
}

int oss_frames_to_bytes(int frames)
{
    return frames * oss_data->bits_per_sample * oss_data->channels / 8;
}

int oss_bytes_to_frames(int bytes)
{
    return bytes * 8 / oss_data->channels / oss_data->bits_per_sample;
}

int oss_calc_bitrate(void)
{
    return (oss_data->rate * oss_data->channels * oss_data->bits_per_sample) >> 3;
}

char *oss_describe_error(void)
{
    const struct
    {
        int error;
        char *text;
    }
    table[] =
    {
        {EINVAL, "The ioctl call is not supported by current OSS version."},
        {EACCES, "You do not have permissions to access the device."},
        {EBUSY,  "The device is busy. There is some other application using it."},
        {ENXIO,  "OSS has not detected any supported sound hardware in your system."},
        {ENODEV, "The device file was found in /dev but OSS is not loaded. You need to "
                 "load it by executing the soundon command."},
        {ENOSPC, "Your system cannot allocate memory for the device buffers. Reboot your "
                 "machine and try again."},
        {ENOENT, "The device file is missing from /dev. Perhaps you have not installed "
                 "and started Open Sound System yet."},
    };

    int count;
    for (count = 0; count < N_ELEMENTS(table); count++)
    {
        if (table[count].error == errno)
            return table[count].text;
    }

    return strerror(errno);
}

int oss_probe_for_adev(oss_sysinfo *sysinfo)
{
    int num;
    if ((num = sysinfo->numaudios) < 1)
    {
        errno = ENXIO;
        return -1;
    }

    return num;
}

bool_t oss_hardware_present(void)
{
    int mixerfd;
    oss_sysinfo sysinfo;

    CHECK_NOISY(mixerfd = open, DEFAULT_MIXER, O_RDWR, 0);
    CHECK(ioctl, mixerfd, SNDCTL_SYSINFO, &sysinfo);
    CHECK_NOISY(oss_probe_for_adev, &sysinfo);

    close(mixerfd);
    return TRUE;

FAILED:
    close(mixerfd);
    return FALSE;
}
