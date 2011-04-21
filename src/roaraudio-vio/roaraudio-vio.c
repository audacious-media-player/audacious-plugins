/*
 * UNIX Transport Plugin for Audacious
 * Copyright 2009 John Lindgren
 * Copyright 2011 Philipp Schafft
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

#include <roaraudio.h>

#include <glib.h>

#include <audacious/debug.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#define error(...) fprintf (stderr, "roaraudio-vio: " __VA_ARGS__)

static VFSFile * roar_fopen (const gchar * uri, const gchar * mode)
{
    struct roar_vio_defaults def;
    VFSFile * file = NULL;
    gboolean update;
    int mode_flag;

    AUDDBG ("fopen %s, mode = %s\n", uri, mode);

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

#ifdef S_IRGRP
    if ( roar_vio_dstr_init_defaults(&def, ROAR_VIO_DEF_TYPE_NONE, mode_flag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1 )
#else
    if ( roar_vio_dstr_init_defaults(&def, ROAR_VIO_DEF_TYPE_NONE, mode_flag, S_IRUSR | S_IWUSR) == -1 )
#endif
        return NULL;

    file = g_malloc (sizeof * file);
    file->handle = g_malloc (sizeof(struct roar_vio_calls));

    if ( roar_vio_open_dstr(file->handle, (char*)uri, &def, 1) == -1 )
     return NULL;

    return file;
}

static gint roar_fclose (VFSFile * file)
{
    gint ret;
    roar_vio_sync(file->handle);
    ret = roar_vio_close(file->handle);
    g_free(file->handle);
    return ret;
}

static gint64 roar_fread (void * ptr, gint64 size, gint64 nitems, VFSFile * file)
{
    gint64 goal = size * nitems;
    gint64 total = 0;

/*    AUDDBG ("[%d] fread %d x %d\n", handle, (gint) size, (gint) nitems); */

    while (total < goal)
    {
        gint64 readed = roar_vio_read (file->handle, (gchar *) ptr + total, goal - total);

        if (readed < 0)
        {
            break;
        }

        if (! readed)
            break;

        total += readed;
    }

/*    AUDDBG (" = %d\n", (gint) total); */

    return (size > 0) ? total / size : 0;
}

static gint64 roar_fwrite (const void * ptr, gint64 size, gint64 nitems,
 VFSFile * file)
{
    gint64 goal = size * nitems;
    gint64 total = 0;

    AUDDBG ("[%p] fwrite %d x %d\n", file->handle, (gint) size, (gint) nitems);

    while (total < goal)
    {
        gint64 written = roar_vio_write (file->handle, (gchar *) ptr + total, goal - total);

        if (written < 0)
        {
            break;
        }

        total += written;
    }

    AUDDBG (" = %d\n", (gint) total);

    return (size > 0) ? total / size : 0;
}

static gint roar_fseek (VFSFile * file, gint64 offset, gint whence)
{

    AUDDBG ("[%p] fseek %d, whence = %d\n", file->handle, (gint) offset, whence);

    if (roar_vio_lseek (file->handle, offset, whence) < 0)
    {
        return -1;
    }

    return 0;
}

static gint64 roar_ftell (VFSFile * file)
{
    gint64 result = roar_vio_lseek (file->handle, 0, SEEK_CUR);


    return result;
}

static gint roar_getc (VFSFile * file)
{
    guchar c;

    return (roar_fread (& c, 1, 1, file) == 1) ? c : -1;
}

static gint roar_ungetc (gint c, VFSFile * file)
{
    return (! roar_fseek (file, -1, SEEK_CUR)) ? c : -1;
}

static void roar_rewind (VFSFile * file)
{
    roar_fseek (file, 0, SEEK_SET);
}

static gboolean roar_feof (VFSFile * file)
{
    gint test = roar_getc (file);

    if (test < 0)
        return TRUE;

    roar_ungetc (test, file);
    return FALSE;
}

static gint roar_ftruncate (VFSFile * file, gint64 length)
{
    return -1;
}

static gint64 roar_fsize (VFSFile * file)
{
    return -1;
}

gchar *roar_metadata(VFSFile* file, const gchar* field) {
    char * ret = NULL;

#ifdef ROAR_VIO_CTL_GET_MIMETYPE
    if ( !strcmp(field, "content-type") ) {
        if ( roar_vio_ctl(file->handle, ROAR_VIO_CTL_GET_MIMETYPE, &ret) == -1 )
            ret = NULL;
    }
#endif

    if ( ret == NULL )
        return NULL;

    return g_strdup(ret);
}

static VFSConstructor constructor =
{
    .vfs_fopen_impl = roar_fopen,
    .vfs_fclose_impl = roar_fclose,
    .vfs_fread_impl = roar_fread,
    .vfs_fwrite_impl = roar_fwrite,
    .vfs_getc_impl = roar_getc,
    .vfs_ungetc_impl = roar_ungetc,
    .vfs_fseek_impl = roar_fseek,
    .vfs_rewind_impl = roar_rewind,
    .vfs_ftell_impl = roar_ftell,
    .vfs_feof_impl = roar_feof,
    .vfs_ftruncate_impl = roar_ftruncate,
    .vfs_fsize_impl = roar_fsize,
    .vfs_get_metadata_impl = roar_metadata,
};

static const gchar * const roar_schemes[] = {"tantalos", "gopher", NULL};

AUD_TRANSPORT_PLUGIN
(
 .name = "RoarAudio Virtual I/O (VIO/DSTR)",
 .schemes = roar_schemes,
 .vtable = & constructor
)
