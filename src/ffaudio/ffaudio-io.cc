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

#define WANT_VFS_STDIO_COMPAT
#include "ffaudio-stdinc.h"

#define IOBUF 4096

static int read_cb (void * file, unsigned char * buf, int size)
{
    return ((VFSFile *) file)->fread (buf, 1, size);
}

static int64_t seek_cb (void * file, int64_t offset, int whence)
{
    if (whence == AVSEEK_SIZE)
        return ((VFSFile *) file)->fsize ();
    if (((VFSFile *) file)->fseek (offset, to_vfs_seek_type (whence & ~(int) AVSEEK_FORCE)))
        return -1;
    return ((VFSFile *) file)->ftell ();
}

AVIOContext * io_context_new (VFSFile & file)
{
    void * buf = av_malloc (IOBUF);
    return avio_alloc_context ((unsigned char *) buf, IOBUF, 0, & file, read_cb, nullptr, seek_cb);
}

void io_context_free (AVIOContext * io)
{
    av_free (io->buffer);
    av_free (io);
}
