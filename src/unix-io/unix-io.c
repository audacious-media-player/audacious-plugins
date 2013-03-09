/*
 * UNIX Transport Plugin for Audacious
 * Copyright 2009-2012 John Lindgren
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

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#define INT_TO_POINTER(x) ((void *) (ptrdiff_t) (x))
#define POINTER_TO_INT(x) ((int) (ptrdiff_t) (x))

#define unix_error(...) do { \
    fprintf (stderr, __VA_ARGS__); \
    fputc ('\n', stderr); \
} while (0)

static void * unix_fopen (const char * uri, const char * mode)
{
    bool_t update;
    mode_t mode_flag;
    int handle;

    update = (strchr (mode, '+') != NULL);

    switch (mode[0])
    {
      case 'r':
        mode_flag = update ? O_RDWR : O_RDONLY;
        break;
      case 'w':
        mode_flag = (update ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
        break;
      case 'a':
        mode_flag = (update ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
        break;
      default:
        return NULL;
    }

#ifdef O_CLOEXEC
    mode_flag |= O_CLOEXEC;
#endif
#ifdef _WIN32
    mode_flag |= O_BINARY;
#endif

    char * filename = uri_to_filename (uri);
    if (! filename)
        return NULL;

    if (mode_flag & O_CREAT)
#ifdef S_IRGRP
        handle = open (filename, mode_flag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#else
        handle = open (filename, mode_flag, S_IRUSR | S_IWUSR);
#endif
    else
        handle = open (filename, mode_flag);

    if (handle < 0)
    {
        unix_error ("Cannot open %s: %s.", filename, strerror (errno));
        free (filename);
        return NULL;
    }

    free (filename);
    return INT_TO_POINTER (handle);
}

static int unix_fclose (VFSFile * file)
{
    int handle = POINTER_TO_INT (vfs_get_handle (file));
    int result = 0;

    if (close (handle) < 0)
    {
        unix_error ("close failed: %s.", strerror (errno));
        result = -1;
    }

    return result;
}

static int64_t unix_fread (void * ptr, int64_t size, int64_t nitems, VFSFile * file)
{
    int handle = POINTER_TO_INT (vfs_get_handle (file));
    int64_t goal = size * nitems;
    int64_t total = 0;

    while (total < goal)
    {
        int64_t readed = read (handle, (char *) ptr + total, goal - total);

        if (readed < 0)
        {
            unix_error ("read failed: %s.", strerror (errno));
            break;
        }

        if (! readed)
            break;

        total += readed;
    }

    return (size > 0) ? total / size : 0;
}

static int64_t unix_fwrite (const void * ptr, int64_t size, int64_t nitems,
 VFSFile * file)
{
    int handle = POINTER_TO_INT (vfs_get_handle (file));
    int64_t goal = size * nitems;
    int64_t total = 0;

    while (total < goal)
    {
        int64_t written = write (handle, (char *) ptr + total, goal - total);

        if (written < 0)
        {
            unix_error ("write failed: %s.", strerror (errno));
            break;
        }

        total += written;
    }

    return (size > 0) ? total / size : 0;
}

static int unix_fseek (VFSFile * file, int64_t offset, int whence)
{
    int handle = POINTER_TO_INT (vfs_get_handle (file));

    if (lseek (handle, offset, whence) < 0)
    {
        unix_error ("lseek failed: %s.", strerror (errno));
        return -1;
    }

    return 0;
}

static int64_t unix_ftell (VFSFile * file)
{
    int handle = POINTER_TO_INT (vfs_get_handle (file));
    int64_t result = lseek (handle, 0, SEEK_CUR);

    if (result < 0)
        unix_error ("lseek failed: %s.", strerror (errno));

    return result;
}

static int unix_getc (VFSFile * file)
{
    unsigned char c;

    return (unix_fread (& c, 1, 1, file) == 1) ? c : -1;
}

static int unix_ungetc (int c, VFSFile * file)
{
    return (! unix_fseek (file, -1, SEEK_CUR)) ? c : -1;
}

static void unix_rewind (VFSFile * file)
{
    unix_fseek (file, 0, SEEK_SET);
}

static bool_t unix_feof (VFSFile * file)
{
    int test = unix_getc (file);

    if (test < 0)
        return TRUE;

    unix_ungetc (test, file);
    return FALSE;
}

static int unix_ftruncate (VFSFile * file, int64_t length)
{
    int handle = POINTER_TO_INT (vfs_get_handle (file));

    int result = ftruncate (handle, length);

    if (result < 0)
        unix_error ("ftruncate failed: %s.", strerror (errno));

    return result;
}

static int64_t unix_fsize (VFSFile * file)
{
    int64_t position, length;

    position = unix_ftell (file);

    if (position < 0)
        return -1;

    unix_fseek (file, 0, SEEK_END);
    length = unix_ftell (file);

    if (length < 0)
        return -1;

    unix_fseek (file, position, SEEK_SET);

    return length;
}

static const char unix_about[] =
 N_("File I/O Plugin for Audacious\n"
    "Copyright 2009-2012 John Lindgren\n\n"
    "THIS PLUGIN IS REQUIRED.  DO NOT DISABLE IT.");

static const char * const unix_schemes[] = {"file", NULL};

static VFSConstructor constructor = {
    .vfs_fopen_impl = unix_fopen,
    .vfs_fclose_impl = unix_fclose,
    .vfs_fread_impl = unix_fread,
    .vfs_fwrite_impl = unix_fwrite,
    .vfs_getc_impl = unix_getc,
    .vfs_ungetc_impl = unix_ungetc,
    .vfs_fseek_impl = unix_fseek,
    .vfs_rewind_impl = unix_rewind,
    .vfs_ftell_impl = unix_ftell,
    .vfs_feof_impl = unix_feof,
    .vfs_ftruncate_impl = unix_ftruncate,
    .vfs_fsize_impl = unix_fsize
};

AUD_TRANSPORT_PLUGIN
(
    .name = N_("File I/O Plugin"),
    .domain = PACKAGE,
    .about_text = unix_about,
    .schemes = unix_schemes,
    .vtable = & constructor
)
