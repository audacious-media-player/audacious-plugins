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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "OSS.h"

static int fd = -1;

static int
open_mixer_device()
{
    char *name;

    if (fd != -1)
        return 0;

    if (oss_cfg.use_alt_mixer_device && oss_cfg.alt_mixer_device)
        name = g_strdup(oss_cfg.alt_mixer_device);
    else if (oss_cfg.mixer_device > 0)
        name = g_strdup_printf("%s%d", DEV_MIXER, oss_cfg.mixer_device);
    else
        name = g_strdup(DEV_MIXER);

    if ((fd = open(name, O_RDWR)) == -1) {
        g_free(name);
        return 1;
    }
    g_free(name);

    return 0;
}

void
oss_get_volume(int *l, int *r)
{
    int v, devs;
    long cmd;

    /*
     * We dont show any errors if this fails, as this is called
     * rather often
     */
    if (!open_mixer_device()) {
        ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
        if ((devs & SOUND_MASK_PCM) && (oss_cfg.use_master == 0))
            cmd = SOUND_MIXER_READ_PCM;
        else if ((devs & SOUND_MASK_VOLUME) && (oss_cfg.use_master == 1))
            cmd = SOUND_MIXER_READ_VOLUME;
        else
            return;

        ioctl(fd, cmd, &v);
        *r = (v & 0xFF00) >> 8;
        *l = (v & 0x00FF);
    }
}

void
oss_set_volume(int l, int r)
{
    int v, devs;
    long cmd;

    if (!open_mixer_device()) {
        ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
        if ((devs & SOUND_MASK_PCM) && (oss_cfg.use_master == 0))
            cmd = SOUND_MIXER_WRITE_PCM;
        else if ((devs & SOUND_MASK_VOLUME) && (oss_cfg.use_master == 1))
            cmd = SOUND_MIXER_WRITE_VOLUME;
        else {
            close(fd);
            return;
        }
        v = (r << 8) | l;
        ioctl(fd, cmd, &v);
    }
    else
        g_warning("Failed to open mixer device: %s", strerror(errno));
}

void
close_mixer_device()
{
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}
