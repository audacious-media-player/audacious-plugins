/*
 * ffaudio-io.c
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include "ffaudio-stdinc.h"

#define IOBUF 4096

static gint read_cb (void * file, guchar * buf, gint size)
{
    return vfs_fread (buf, 1, size, file);
}

static gint64 seek_cb (void * file, gint64 offset, gint whence)
{
    if (whence == AVSEEK_SIZE)
        return vfs_fsize (file);
    if (vfs_fseek (file, offset, whence & ~(gint) AVSEEK_FORCE))
        return -1;
    return vfs_ftell (file);
}

AVIOContext * io_context_new (VFSFile * file)
{
    guchar * buf = av_malloc (IOBUF);
    return avio_alloc_context (buf, IOBUF, 0, file, read_cb, NULL, seek_cb);
}

void io_context_free (AVIOContext * io)
{
    av_free (io->buffer);
    av_free (io);
}
