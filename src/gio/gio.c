/*
 * GIO Transport Plugin for Audacious
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include <audacious/plugin.h>

typedef struct {
    GFile * file;
    GIOStream * iostream;
    GInputStream * istream;
    GOutputStream * ostream;
    GSeekable * seekable;
} FileData;

/* in gtk.c */
void gio_error (const char * format, ...);
void gio_about (void);

#define CHECK_ERROR(op, name) do { \
    if (error) { \
        gio_error ("Cannot %s %s: %s.", op, name, error->message); \
        g_error_free (error); \
        goto FAILED; \
    } \
} while (0)

static void * gio_fopen (const char * filename, const char * mode)
{
    GError * error = 0;

    FileData * data = malloc (sizeof (FileData));
    memset (data, 0, sizeof (FileData));

    data->file = g_file_new_for_uri (filename);

    switch (mode[0])
    {
    case 'r':
        if (strchr (mode, '+'))
        {
            data->iostream = (GIOStream *) g_file_open_readwrite (data->file, 0, & error);
            CHECK_ERROR ("open", filename);
            data->istream = g_io_stream_get_input_stream (data->iostream);
            data->ostream = g_io_stream_get_output_stream (data->iostream);
            data->seekable = (GSeekable *) data->iostream;
        }
        else
        {
            data->istream = (GInputStream *) g_file_read (data->file, 0, & error);
            CHECK_ERROR ("open", filename);
            data->seekable = (GSeekable *) data->istream;
        }
        break;
    case 'w':
        if (strchr (mode, '+'))
        {
            data->iostream = (GIOStream *) g_file_replace_readwrite (data->file, 0, 0, 0, 0, & error);
            CHECK_ERROR ("open", filename);
            data->istream = g_io_stream_get_input_stream (data->iostream);
            data->ostream = g_io_stream_get_output_stream (data->iostream);
            data->seekable = (GSeekable *) data->iostream;
        }
        else
        {
            data->ostream = (GOutputStream *) g_file_replace (data->file, 0, 0, 0, 0, & error);
            CHECK_ERROR ("open", filename);
            data->seekable = (GSeekable *) data->ostream;
        }
        break;
    case 'a':
        if (strchr (mode, '+'))
        {
            gio_error ("Cannot open %s: GIO does not support read-and-append mode.", filename);
            goto FAILED;
        }
        else
        {
            data->ostream = (GOutputStream *) g_file_append_to (data->file, 0, 0, & error);
            CHECK_ERROR ("open", filename);
            data->seekable = (GSeekable *) data->ostream;
        }
        break;
    default:
        gio_error ("Cannot open %s: invalid mode.", filename);
        goto FAILED;
    }

    return data;

FAILED:
    free (data);
    return 0;
}

static int gio_fclose (VFSFile * file)
{
    FileData * data = vfs_get_handle (file);
    GError * error = 0;

    if (data->iostream)
    {
        g_io_stream_close (data->iostream, 0, & error);
        g_object_unref (data->iostream);
        CHECK_ERROR ("close", vfs_get_filename (file));
    }
    else if (data->istream)
    {
        g_input_stream_close (data->istream, 0, & error);
        g_object_unref (data->istream);
        CHECK_ERROR ("close", vfs_get_filename (file));
    }
    else if (data->ostream)
    {
        g_output_stream_close (data->ostream, 0, & error);
        g_object_unref (data->ostream);
        CHECK_ERROR ("close", vfs_get_filename (file));
    }

    if (data->file)
        g_object_unref (data->file);

    return 0;

FAILED:
    if (data->file)
        g_object_unref (data->file);

    return -1;
}

static int64_t gio_fread (void * buf, int64_t size, int64_t nitems, VFSFile * file)
{
    FileData * data = vfs_get_handle (file);
    GError * error = 0;

    if (! data->istream)
    {
        gio_error ("Cannot read from %s: not open for reading.", vfs_get_filename (file));
        return 0;
    }

    int64_t readed = g_input_stream_read (data->istream, buf, size * nitems, 0, & error);
    CHECK_ERROR ("read from", vfs_get_filename (file));

    return (size > 0) ? readed / size : 0;

FAILED:
    return 0;
}

static int64_t gio_fwrite (const void * buf, int64_t size, int64_t nitems, VFSFile * file)
{
    FileData * data = vfs_get_handle (file);
    GError * error = 0;

    if (! data->ostream)
    {
        gio_error ("Cannot write to %s: not open for writing.", vfs_get_filename (file));
        return 0;
    }

    int64_t written = g_output_stream_write (data->ostream, buf, size * nitems, 0, & error);
    CHECK_ERROR ("write to", vfs_get_filename (file));

    return (size > 0) ? written / size : 0;

FAILED:
    return 0;
}

static int gio_fseek (VFSFile * file, int64_t offset, int whence)
{
    FileData * data = vfs_get_handle (file);
    GError * error = 0;
    GSeekType gwhence;

    switch (whence)
    {
    case SEEK_SET:
        gwhence = G_SEEK_SET;
        break;
    case SEEK_CUR:
        gwhence = G_SEEK_CUR;
        break;
    case SEEK_END:
        gwhence = G_SEEK_END;
        break;
    default:
        gio_error ("Cannot seek within %s: invalid whence.", vfs_get_filename (file));
        return -1;
    }

    g_seekable_seek (data->seekable, offset, gwhence, NULL, & error);
    CHECK_ERROR ("seek within", vfs_get_filename (file));

    return 0;

FAILED:
    return -1;
}

static int64_t gio_ftell (VFSFile * file)
{
    FileData * data = vfs_get_handle (file);
    return g_seekable_tell (data->seekable);
}

static int gio_getc (VFSFile * file)
{
    unsigned char c;
    return (gio_fread (& c, 1, 1, file) == 1) ? c : -1;
}

static int gio_ungetc (int c, VFSFile * file)
{
    return (! gio_fseek (file, -1, SEEK_CUR)) ? c : -1;
}

static void gio_rewind (VFSFile * file)
{
    gio_fseek (file, 0, SEEK_SET);
}

static bool_t gio_feof (VFSFile * file)
{
    int test = gio_getc (file);

    if (test < 0)
        return TRUE;

    gio_ungetc (test, file);
    return FALSE;
}

static int gio_ftruncate (VFSFile * file, int64_t length)
{
    FileData * data = vfs_get_handle (file);
    GError * error = 0;

    g_seekable_truncate (data->seekable, length, NULL, & error);
    CHECK_ERROR ("truncate", vfs_get_filename (file));

    return 0;

FAILED:
    return -1;
}

static int64_t gio_fsize (VFSFile * file)
{
    FileData * data = vfs_get_handle (file);
    GError * error = 0;

    GFileInfo * info = g_file_query_info (data->file,
     G_FILE_ATTRIBUTE_STANDARD_SIZE, 0, 0, & error);
    CHECK_ERROR ("get size of", vfs_get_filename (file));

    int64_t size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);

    g_object_unref (info);
    return size;

FAILED:
    return -1;
}

static const char * const gio_schemes[] = {"ftp", "sftp", "smb", 0};

static VFSConstructor constructor = {
 .vfs_fopen_impl = gio_fopen,
 .vfs_fclose_impl = gio_fclose,
 .vfs_fread_impl = gio_fread,
 .vfs_fwrite_impl = gio_fwrite,
 .vfs_getc_impl = gio_getc,
 .vfs_ungetc_impl = gio_ungetc,
 .vfs_fseek_impl = gio_fseek,
 .vfs_rewind_impl = gio_rewind,
 .vfs_ftell_impl = gio_ftell,
 .vfs_feof_impl = gio_feof,
 .vfs_ftruncate_impl = gio_ftruncate,
 .vfs_fsize_impl = gio_fsize
};

AUD_TRANSPORT_PLUGIN
(
 .name = "GIO Support",
 .about = gio_about,
 .schemes = gio_schemes,
 .vtable = & constructor
)
