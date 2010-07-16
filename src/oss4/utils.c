/*
 * OSS4 Output Plugin for Audacious
 * Copyright 2010 Michał Lipski <tallica@o2.pl>
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
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

gchar *oss_format_to_text(gint format)
{
    const struct
    {
        gint format;
        gchar *format_text;
    }
    table[] =
    {
        {AFMT_FLOAT,  "AFMT_FLOAT"},
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

    gint count;

    for (count = 0; count < G_N_ELEMENTS(table); count++)
    {
        if (table[count].format == format)
        {
            return table[count].format_text;
        }
    }

    return "FMT_UNKNOWN";
}

gint oss_convert_aud_format(gint aud_format)
{
    const struct
    {
        gint aud_format;
        gint format;
    }
    table[] =
    {
        {FMT_FLOAT,  AFMT_FLOAT},
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

    gint count;

    for (count = 0; count < G_N_ELEMENTS(table); count++)
    {
        if (table[count].aud_format == aud_format)
        {
            return table[count].format;
        }
    }

    return -1;
}

gint oss_format_to_bits(gint format)
{
    gchar bits;

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
        case AFMT_FLOAT:
            bits = sizeof(float) * 8;
            break;
        default:
            bits = 8;
    }

    return bits;
}

gint oss_frames_to_bytes(gint frames)
{
    return frames * oss_data->bits_per_sample * oss_data->channels / 8;
}

gint oss_bytes_to_frames(gint bytes)
{
	return bytes * 8 / oss_data->channels / oss_data->bits_per_sample;
}

gint oss_calc_bitrate(void)
{
    return (oss_data->rate * oss_data->channels * oss_data->bits_per_sample) >> 3;
}

gchar *oss_describe_error(void)
{
    const struct
    {
        gint error;
        gchar *text;
    }
    table[] =
    {
        {EINVAL, "The ioctl call is not supported by current OSS version.\n"},
        {EACCES, "You do not have permissions to access the device.\n"},
        {EBUSY,  "The device is busy. There is some other application using it.\n"},
        {ENXIO,  "OSS has not detected any supported sound hardware in your system.\n"},
        {ENODEV, "The device file was found in /dev but OSS is not loaded. You need to "
                 "load it by executing the soundon command.\n"},
        {ENOSPC, "Your system cannot allocate memory for the device buffers. Reboot your "
                 "machine and try again.\n"},
        {ENOENT, "The device file is missing from /dev. Perhaps you have not installed "
                 "and started Open Sound System yet.\n"},
    };

    gint count;
    for (count = 0; count < G_N_ELEMENTS(table); count++)
    {
        if (table[count].error == errno)
            return g_strdup(table[count].text);
    }

    return g_strdup_printf("Unknown OSS error (%d)\n", errno);
}

gboolean oss_hardware_present(void)
{
    gint mixerfd;
    oss_sysinfo sysinfo;

    if ((mixerfd = open(DEFAULT_MIXER, O_RDWR, 0)) == -1)
        goto FAILED;

    if (ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo) == -1)
        goto FAILED;

    if (sysinfo.numaudios > 0)
    {
        close(mixerfd);
        return TRUE;
    }
    else
    {
        errno = ENXIO;
        goto FAILED;
    }

    FAILED:
        SHOW_ERROR_MSG;
        close(mixerfd);
        return FALSE;
}

void oss_show_error(const gchar *message)
{
    static GtkWidget *dialog = NULL;

    audgui_simple_message (&dialog, GTK_MESSAGE_ERROR, _("OSS4 error"), message);
}
