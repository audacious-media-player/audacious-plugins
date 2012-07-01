/*
 * ffaudio-io.c
 * Copyright 2011 John Lindgren
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
 */

#include <glib.h>

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
