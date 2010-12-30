/*
 * WinINet Transport Plugin for Audacious
 * Copyright 2010 John Lindgren
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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <wininet.h>

#include <audacious/debug.h>
#include <audacious/plugin.h>

#define MAGIC ('I' | ('N' << 8) | ('E' << 16) | ('T' << 24))
#define NO_SIZE_YET (-2)
#define BUFSIZE 16384

#define error(...) printf ("inet: " __VA_ARGS__)

typedef struct {
    VFSFile v;
    int magic;
    HINTERNET h;
    int64_t size, pos;
    int ungotten;
    unsigned char buf[BUFSIZE];
    int bufstart, buflen;
} InetFile;

static HINTERNET sess = NULL;

static VFSFile * inet_fopen (const char * uri, const char * mode)
{
    if (! sess)
    {
        AUDDBG ("Connecting ...\n");
        sess = InternetOpen ("", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (! sess)
        {
            error ("Failed to connect (code %d).\n", (int) GetLastError ());
            return NULL;
        }
    }

    AUDDBG ("fopen %s, mode = %s\n", uri, mode);

    if (mode[0] != 'r' || strchr (mode, '+'))
    {
        error ("Unsupported file mode: %s\n", mode);
        return NULL;
    }

    InetFile * file = g_malloc0 (sizeof (InetFile));
    file->magic = MAGIC;

    file->h = InternetOpenUrl (sess, uri, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (! file->h)
    {
        error ("Failed to open %s (code %d).\n", uri, (int) GetLastError ());
        free (file);
        return NULL;
    }

    file->size = NO_SIZE_YET;
    file->pos = 0;
    file->ungotten = EOF;
    file->bufstart = 0;
    file->buflen = 0;

    AUDDBG (" = %p\n", file);
    return (VFSFile *) file;
}

static int inet_fclose (VFSFile * _file)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

    AUDDBG ("[%p] fclose\n", file);
    InternetCloseHandle (file->h);

    memset ((unsigned char *) file + sizeof (VFSFile), 0,
     sizeof (InetFile) - sizeof (VFSFile));
    return 0;
}

static char fill_buf (InetFile * file)
{
    if (file->buflen > 0)
        return 1;

    file->bufstart = 0;

    DWORD readed = 0;
    if (! InternetReadFile (file->h, file->buf, BUFSIZE, & readed))
    {
        error ("Read error (code %d).\n", (int) GetLastError ());
        return 0;
    }
    if (! readed)
    {
        AUDDBG ("[%p] End of file reached.\n", file);
        return 0;
    }

    file->buflen = readed;
    return 1;
}

static int64_t inet_fread (void * _buf, int64_t size, int64_t items,
 VFSFile * _file)
{
    unsigned char * buf = _buf;
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

/*    AUDDBG ("[%p] fread %d\n", file, (int) (size * items)); */
    int64_t goal = size * items;
    int64_t total = 0;

    while (total < goal)
    {
        if (file->ungotten != EOF)
        {
            buf[total ++] = file->ungotten;
            file->ungotten = EOF;
        }
        else if (file->buflen > goal - total)
        {
            memcpy (buf + total, file->buf + file->bufstart, goal - total);
            file->bufstart += goal - total;
            file->buflen -= goal - total;
            total = goal;
        }
        else if (file->buflen > 0)
        {
            memcpy (buf + total, file->buf + file->bufstart, file->buflen);
            total += file->buflen;
            file->buflen = 0;
        }
        else
        {
            if (! fill_buf (file))
                break;
        }
    }

/*    AUDDBG (" = %d\n", (int) total); */
    file->pos += total;
    return (size > 0) ? total / size : 0;
}

static int64_t inet_fwrite (const void * data, int64_t size, int64_t items,
 VFSFile * _file)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

    error ("Writing is not supported.\n");
    return 0;
}

static void get_content_length (InetFile * file)
{
    if (file->size != NO_SIZE_YET)
        return;

    char buf[32];
    DWORD buflen = sizeof buf - 1;
    DWORD index = 0;
    if (! HttpQueryInfo (file->h, HTTP_QUERY_CONTENT_LENGTH, buf, & buflen,
     & index))
    {
        int error = GetLastError ();
        if (error == ERROR_HTTP_HEADER_NOT_FOUND)
            AUDDBG ("[%p] Unknown content length.\n", file);
        else
            error ("HTTP_QUERY_CONTENT_LENGTH failed (code %d).\n", error);

        file->size = -1;
        return;
    }

    if (sscanf (buf, "%I64d", & file->size) != 1)
    {
        error ("Invalid content length.\n");
        file->size = -1;
        return;
    }
}

static int inet_fseek (VFSFile * _file, int64_t offset, int whence)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

    AUDDBG ("[%p] fseek %d, whence = %d\n", file, (int) offset, whence);

    if (file->size == NO_SIZE_YET)
        get_content_length (file);

    if (file->size < 0)
    {
        error ("Attempt to seek in content of unknown length.\n");
        return -1;
    }

    if (whence == SEEK_SET)
        whence = FILE_BEGIN;
    else if (whence == SEEK_CUR)
        whence = FILE_CURRENT;
    else if (whence == SEEK_END)
        whence = FILE_END;
    else
    {
        error ("Invalid whence: %d.\n", whence);
        return -1;
    }

    file->ungotten = EOF;
    file->bufstart = 0;
    file->buflen = 0;

    long low = offset & 0xffffffff;
    long high = (offset >> 32) & 0xffffffff;
    low = InternetSetFilePointer (file->h, low, & high, whence, 0);

    if (low == INVALID_SET_FILE_POINTER) /* may be a valid return value! */
    {
        int error = GetLastError ();
        if (error != NO_ERROR)
        {
            error ("Seek error (code %d).\n", error);
            return -1;
        }
    }

    file->pos = (((int64_t) high & 0xffffffff) << 32) |
     ((int64_t) low & 0xffffffff);
    return 0;
}

static int64_t inet_ftell (VFSFile * _file)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);
    return file->pos;
}

static int inet_getc (VFSFile * file)
{
    unsigned char c;
    return (inet_fread (& c, 1, 1, file) == 1) ? c : EOF;
}

static int inet_ungetc (int c, VFSFile * _file)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

    if (file->ungotten != EOF)
    {
        error ("Too many ungetc's.\n");
        return EOF;
    }

    file->ungotten = c;
    return c;
}

static void inet_rewind (VFSFile * file)
{
    inet_fseek (file, 0, SEEK_SET);
}

static int inet_feof (VFSFile * _file)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

    if (file->ungotten != EOF || file->buflen > 0)
        return 0;

    return ! fill_buf (file);
}

static int inet_ftruncate (VFSFile * _file, int64_t length)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

    error ("Writing is not supported.\n");
    return 0;
}

static int64_t inet_fsize (VFSFile * _file)
{
    InetFile * file = (InetFile *) _file;
    assert (file->magic == MAGIC);

    AUDDBG ("[%p] fsize\n", file);

    if (file->size == NO_SIZE_YET)
        get_content_length (file);

    AUDDBG (" = %d\n", (int) file->size);
    return file->size;
}

void inet_cleanup (void)
{
    if (sess)
    {
        InternetCloseHandle (sess);
        sess = NULL;
    }
}

static const char * const inet_schemes[] = {"http", "https", NULL};

static VFSConstructor constructor = {
 .vfs_fopen_impl = inet_fopen,
 .vfs_fclose_impl = inet_fclose,
 .vfs_fread_impl = inet_fread,
 .vfs_fwrite_impl = inet_fwrite,
 .vfs_getc_impl = inet_getc,
 .vfs_ungetc_impl = inet_ungetc,
 .vfs_fseek_impl = inet_fseek,
 .vfs_rewind_impl = inet_rewind,
 .vfs_ftell_impl = inet_ftell,
 .vfs_feof_impl = inet_feof,
 .vfs_ftruncate_impl = inet_ftruncate,
 .vfs_fsize_impl = inet_fsize
};

static TransportPlugin inet_plugin = {
 .description = "WinINet-based HTTP/HTTPS support",
 .cleanup = inet_cleanup,
 .schemes = inet_schemes,
 .vtable = & constructor,
};

static TransportPlugin * const inet_plugins[] = {& inet_plugin, NULL};

SIMPLE_TRANSPORT_PLUGIN (inet, inet_plugins)
