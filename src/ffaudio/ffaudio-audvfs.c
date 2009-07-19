/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ffaudio-stdinc.h"

int av_strstart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && *pfx == *str) {
        pfx++;
        str++;
    }
    if (!*pfx && ptr)
        *ptr = str;
    return !*pfx;
}

static int audvfs_open(URLContext *h, const char *filename, int flags)
{
    VFSFile *file;

    av_strstart(filename, "audvfs:", &filename);

    if (flags & URL_WRONLY) {
	file = aud_vfs_fopen(filename, "wb");
    } else {
	file = aud_vfs_fopen(filename, "rb");
    }
    
    if (file == NULL)
        return -ENOENT;
    h->priv_data = file;
    return 0;
}

static int audvfs_read(URLContext *h, unsigned char *buf, int size)
{
    VFSFile *file;
    file = h->priv_data;
    return aud_vfs_fread(buf, 1, size, file);
}

static int audvfs_write(URLContext *h, unsigned char *buf, int size)
{
    VFSFile *file;
    file = h->priv_data;
    return aud_vfs_fwrite(buf, 1, size, file);
}

/* XXX: use llseek */
static int64_t audvfs_seek(URLContext *h, int64_t pos, int whence)
{
    VFSFile *file;
    file = h->priv_data;
    if (whence == AVSEEK_SIZE)
        return aud_vfs_fsize(file);
    return aud_vfs_fseek(file, pos, whence);
}

static int audvfs_close(URLContext *h)
{
    VFSFile *file;
    file = h->priv_data;
    return aud_vfs_fclose(file);
}

URLProtocol audvfs_protocol = {
    .name = "audvfs",
    .url_open = audvfs_open,
    .url_read = audvfs_read,
    .url_write = audvfs_write,
    .url_seek = audvfs_seek,
    .url_close = audvfs_close,
};

static int audvfsptr_open(URLContext *h, const char *filename, int flags)
{
    VFSFile *p;

    av_strstart(filename, "audvfsptr:", &filename);

    p = (VFSFile *) strtoul(filename, NULL, 16);
    h->priv_data = aud_vfs_dup(p);
    aud_vfs_rewind(p);

    return 0;
}

URLProtocol audvfsptr_protocol = {
    .name = "audvfsptr",
    .url_open = audvfsptr_open,
    .url_read = audvfs_read,
    .url_write = audvfs_write,
    .url_seek = audvfs_seek,
    .url_close = audvfs_close,
};

