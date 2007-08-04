/*
 * Buffered file io for ffmpeg system
 * Copyright (c) 2001 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "avformat.h"
#include "cutils.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "audacious/vfs.h"

/* standard file protocol */

static int file_open(URLContext *h, const char *filename, int flags)
{
    VFSFile *file;

    if (flags & URL_WRONLY) {
	file = vfs_fopen(filename, "wb");
    } else {
	file = vfs_fopen(filename, "rb");
    }
    
    if (file == NULL)
        return -ENOENT;
    h->priv_data = file;
    return 0;
}

static int file_read(URLContext *h, unsigned char *buf, int size)
{
    VFSFile *file;
    file = h->priv_data;
    return vfs_fread(buf, 1, size, file);
}

static int file_write(URLContext *h, unsigned char *buf, int size)
{
    VFSFile *file;
    file = h->priv_data;
    return vfs_fwrite(buf, 1, size, file);
}

/* XXX: use llseek */
static offset_t file_seek(URLContext *h, offset_t pos, int whence)
{
    int result = 0;
    VFSFile *file;
    file = h->priv_data;
    result = vfs_fseek(file, pos, whence);
    if (result == 0)
	result = vfs_ftell(file);
    else
        result = -1;
    return result;
}

static int file_close(URLContext *h)
{
    VFSFile *file;
    file = h->priv_data;
    return vfs_fclose(file);
}

URLProtocol file_protocol = {
    "file",
    file_open,
    file_read,
    file_write,
    file_seek,
    file_close,
    NULL
};

