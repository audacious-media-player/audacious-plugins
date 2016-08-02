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

#include <libaudcore/audstrings.h>

const char *oss_format_to_text(int format)
{
    static const struct
    {
        int format;
        const char *format_text;
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
#ifdef AFMT_S24_LE
        {AFMT_S24_LE, "AFMT_S24_LE"},
        {AFMT_S24_BE, "AFMT_S24_BE"},
#endif
#ifdef AFMT_S32_LE
        {AFMT_S32_LE, "AFMT_S32_LE"},
        {AFMT_S32_BE, "AFMT_S32_BE"},
#endif
    };

    for (auto & conv : table)
    {
        if (conv.format == format)
            return conv.format_text;
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
#ifdef AFMT_S24_LE
        {FMT_S24_LE, AFMT_S24_LE},
        {FMT_S24_BE, AFMT_S24_BE},
#endif
#ifdef AFMT_S32_LE
        {FMT_S32_LE, AFMT_S32_LE},
        {FMT_S32_BE, AFMT_S32_BE},
#endif
    };

    int count;

    for (count = 0; count < aud::n_elems(table); count++)
    {
        if (table[count].aud_format == aud_format)
        {
            return table[count].format;
        }
    }

    return -1;
}

int oss_format_to_bytes(int format)
{
    int bytes = 1;

    switch (format)
    {
        case AFMT_U8:
        case AFMT_S8:
            bytes = 1;
            break;
        case AFMT_S16_LE:
        case AFMT_S16_BE:
        case AFMT_U16_LE:
        case AFMT_U16_BE:
            bytes = 2;
            break;
#ifdef AFMT_S24_LE
        case AFMT_S24_LE:
        case AFMT_S24_BE:
            bytes = 4;
            break;
#endif
#ifdef AFMT_S32_LE
        case AFMT_S32_LE:
        case AFMT_S32_BE:
            bytes = 4;
            break;
#endif
#ifdef AFMT_FLOAT
        case AFMT_FLOAT:
            bytes = sizeof(float);
            break;
#endif
    }

    return bytes;
}

const char *oss_describe_error()
{
    const struct
    {
        int error;
        const char *text;
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
    for (count = 0; count < aud::n_elems(table); count++)
    {
        if (table[count].error == errno)
            return table[count].text;
    }

    return strerror(errno);
}

#ifdef SNDCTL_SYSINFO
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
#endif

bool oss_hardware_present()
{
    int mixerfd;

    CHECK(mixerfd = open, DEFAULT_MIXER, O_RDWR, 0);

#ifdef SNDCTL_SYSINFO
    oss_sysinfo sysinfo;
    memset(&sysinfo, 0, sizeof sysinfo);
    CHECK(ioctl, mixerfd, SNDCTL_SYSINFO, &sysinfo);
    CHECK(oss_probe_for_adev, &sysinfo);
#endif

    close(mixerfd);
    return true;

FAILED:
    if (mixerfd >= 0)
        close(mixerfd);
    return false;
}
