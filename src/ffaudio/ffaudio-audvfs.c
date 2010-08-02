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

#include <libavutil/avstring.h>

static int audvfs_read(URLContext *h, unsigned char *buf, int size)
{
    VFSFile *file;
    file = h->priv_data;
    return vfs_fread(buf, 1, size, file);
}

static int audvfs_write(URLContext *h, const unsigned char *buf, int size)
{
    VFSFile *file;
    file = h->priv_data;
    return vfs_fwrite(buf, 1, size, file);
}

/* XXX: use llseek */
static int64_t audvfs_seek(URLContext *h, int64_t pos, int whence)
{
    int64_t res, siz;
    VFSFile *file;
    file = h->priv_data;
    siz = vfs_fsize(file);

    if (whence == AVSEEK_SIZE)
        return siz;

    if (whence == SEEK_SET && pos > siz)
        return AVERROR(EPIPE);

    if (vfs_fseek(file, pos, whence) == 0)
    {
        if (whence == SEEK_SET)
            res = pos;
        else
            res = vfs_ftell(file);
    } else
        res = AVERROR(EPIPE);

    return res;
}

static int audvfs_close(URLContext *h)
{
    VFSFile *file;
    file = h->priv_data;
    return vfs_fclose(file);
}

static int audvfsptr_open(URLContext *h, const char *filename, int flags)
{
    VFSFile *p;

    av_strstart(filename, "audvfsptr:", &filename);

    p = (VFSFile *) strtoul(filename, NULL, 16);
    h->priv_data = vfs_dup(p);
    vfs_rewind(p);

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

