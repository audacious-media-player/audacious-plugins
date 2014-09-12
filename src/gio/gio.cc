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
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

class GIOFile : public VFSFile {
public:
    GIOFile (const char * filename, const char * mode);
    ~GIOFile ();

protected:
    int64_t fread_impl (void * ptr, int64_t size, int64_t nmemb);
    int64_t fwrite_impl (const void * buf, int64_t size, int64_t nitems);

    int fseek_impl (int64_t offset, VFSSeekType whence);
    int64_t ftell_impl ();

    int getc_impl ();
    int ungetc_impl (int c);

    bool feof_impl ();

    int ftruncate_impl (int64_t length);
    int64_t fsize_impl ();

    int fflush_impl ();

private:
    GFile * m_file;
    GIOStream * m_iostream;
    GInputStream * m_istream;
    GOutputStream * m_ostream;
    GSeekable * m_seekable;
};

#define CHECK_ERROR(op, name) do { \
    if (error) { \
        AUDERR ("Cannot %s %s: %s.\n", op, name, error->message); \
        g_error_free (error); \
        goto FAILED; \
    } \
} while (0)

GIOFile::GIOFile (const char * url, const char * mode) :
    VFSFile (url)
{
    GError * error = nullptr;

    m_file = g_file_new_for_uri (filename ());

    switch (mode[0])
    {
    case 'r':
        if (strchr (mode, '+'))
        {
            m_iostream = (GIOStream *) g_file_open_readwrite (m_file, 0, & error);
            CHECK_ERROR ("open", filename ());
            m_istream = g_io_stream_get_input_stream (m_iostream);
            m_ostream = g_io_stream_get_output_stream (m_iostream);
            m_seekable = (GSeekable *) m_iostream;
        }
        else
        {
            m_istream = (GInputStream *) g_file_read (m_file, 0, & error);
            CHECK_ERROR ("open", filename ());
            m_seekable = (GSeekable *) m_istream;
        }
        break;
    case 'w':
        if (strchr (mode, '+'))
        {
            m_iostream = (GIOStream *) g_file_replace_readwrite (m_file,
             0, 0, (GFileCreateFlags) 0, 0, & error);
            CHECK_ERROR ("open", filename ());
            m_istream = g_io_stream_get_input_stream (m_iostream);
            m_ostream = g_io_stream_get_output_stream (m_iostream);
            m_seekable = (GSeekable *) m_iostream;
        }
        else
        {
            m_ostream = (GOutputStream *) g_file_replace (m_file, 0, 0,
             (GFileCreateFlags) 0, 0, & error);
            CHECK_ERROR ("open", filename ());
            m_seekable = (GSeekable *) m_ostream;
        }
        break;
    case 'a':
        if (strchr (mode, '+'))
        {
            AUDERR ("Cannot open %s: GIO does not support read-and-append mode.\n", filename ());
            goto FAILED;
        }
        else
        {
            m_ostream = (GOutputStream *) g_file_append_to (m_file,
             (GFileCreateFlags) 0, 0, & error);
            CHECK_ERROR ("open", filename ());
            m_seekable = (GSeekable *) m_ostream;
        }
        break;
    default:
        AUDERR ("Cannot open %s: invalid mode.\n", filename ());
        break;
    }

FAILED:
    return;
}

GIOFile::~GIOFile ()
{
    GError * error = 0;

    if (m_iostream)
    {
        g_io_stream_close (m_iostream, 0, & error);
        g_object_unref (m_iostream);
        CHECK_ERROR ("close", filename ());
    }
    else if (m_istream)
    {
        g_input_stream_close (m_istream, 0, & error);
        g_object_unref (m_istream);
        CHECK_ERROR ("close", filename ());
    }
    else if (m_ostream)
    {
        g_output_stream_close (m_ostream, 0, & error);
        g_object_unref (m_ostream);
        CHECK_ERROR ("close", filename ());
    }

FAILED:
    if (m_file)
        g_object_unref (m_file);
}

static VFSFile * gio_fopen_impl (const char * filename, const char * mode)
{
#if ! GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    return new GIOFile (filename, mode);
}

int64_t GIOFile::fread_impl (void * buf, int64_t size, int64_t nitems)
{
    GError * error = 0;

    if (! m_istream)
    {
        AUDERR ("Cannot read from %s: not open for reading.\n", filename ());
        return 0;
    }

    int64_t readed = g_input_stream_read (m_istream, buf, size * nitems, 0, & error);
    CHECK_ERROR ("read from", filename ());

    return (size > 0) ? readed / size : 0;

FAILED:
    return 0;
}

int64_t GIOFile::fwrite_impl (const void * buf, int64_t size, int64_t nitems)
{
    GError * error = 0;

    if (! m_ostream)
    {
        AUDERR ("Cannot write to %s: not open for writing.\n", filename ());
        return 0;
    }

    int64_t written = g_output_stream_write (m_ostream, buf, size * nitems, 0, & error);
    CHECK_ERROR ("write to", filename ());

    return (size > 0) ? written / size : 0;

FAILED:
    return 0;
}

int GIOFile::fseek_impl (int64_t offset, VFSSeekType whence)
{
    GError * error = 0;
    GSeekType gwhence;

    switch (whence)
    {
    case VFS_SEEK_SET:
        gwhence = G_SEEK_SET;
        break;
    case VFS_SEEK_CUR:
        gwhence = G_SEEK_CUR;
        break;
    case VFS_SEEK_END:
        gwhence = G_SEEK_END;
        break;
    default:
        AUDERR ("Cannot seek within %s: invalid whence.\n", filename ());
        return -1;
    }

    g_seekable_seek (m_seekable, offset, gwhence, nullptr, & error);
    CHECK_ERROR ("seek within", filename ());

    return 0;

FAILED:
    return -1;
}

int64_t GIOFile::ftell_impl ()
{
    return g_seekable_tell (m_seekable);
}

int GIOFile::getc_impl ()
{
    unsigned char c;
    return (fread_impl (& c, 1, 1) == 1) ? c : -1;
}

int GIOFile::ungetc_impl (int c)
{
    return (! fseek_impl (-1, VFS_SEEK_CUR)) ? c : -1;
}

bool GIOFile::feof_impl ()
{
    int test = getc_impl ();

    if (test < 0)
        return TRUE;

    ungetc_impl (test);
    return FALSE;
}

int GIOFile::ftruncate_impl (int64_t length)
{
    GError * error = 0;

    g_seekable_truncate (m_seekable, length, nullptr, & error);
    CHECK_ERROR ("truncate", filename ());

    return 0;

FAILED:
    return -1;
}

int64_t GIOFile::fsize_impl ()
{
    GError * error = 0;
    int64_t size;

    /* Audacious core expects one of two cases:
     *  1) File size is known and file is seekable.
     *  2) File size is unknown and file is not seekable.
     * Therefore, we return -1 for size if file is not seekable. */
    if (! g_seekable_can_seek (m_seekable))
        return -1;

    GFileInfo * info = g_file_query_info (m_file,
     G_FILE_ATTRIBUTE_STANDARD_SIZE, (GFileQueryInfoFlags) 0, 0, & error);
    CHECK_ERROR ("get size of", filename ());

    size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);

    g_object_unref (info);
    return size;

FAILED:
    return -1;
}

int GIOFile::fflush_impl ()
{
    int result;
    GError * error = nullptr;

    if (! m_ostream)
    {
        AUDERR ("Cannot flush %s: not open for writing.\n", filename ());
        return -1;
    }

    result = g_output_stream_flush (m_ostream, nullptr, & error);
    CHECK_ERROR ("flush", filename ());

    return result;

FAILED:
    return -1;
}

static const char gio_about[] =
 N_("GIO Plugin for Audacious\n"
    "Copyright 2009-2012 John Lindgren");

static const char * const gio_schemes[] = {"ftp", "sftp", "smb", 0};

#define AUD_PLUGIN_NAME        N_("GIO Plugin")
#define AUD_PLUGIN_ABOUT       gio_about
#define AUD_TRANSPORT_SCHEMES  gio_schemes
#define AUD_TRANSPORT_FOPEN    gio_fopen_impl

#define AUD_DECLARE_TRANSPORT
#include <libaudcore/plugin-declare.h>
