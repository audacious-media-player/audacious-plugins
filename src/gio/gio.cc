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
#include <sys/stat.h>

#include <gio/gio.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

static const char gio_about[] =
 N_("GIO Plugin for Audacious\n"
    "Copyright 2009-2012 John Lindgren");

static const char * const gio_schemes[] = {"ftp", "sftp", "smb", "mtp"};

class GIOTransport : public TransportPlugin
{
public:
    static constexpr PluginInfo info = {N_("GIO Plugin"), PACKAGE, gio_about};

    constexpr GIOTransport () : TransportPlugin (info, gio_schemes) {}

    VFSImpl * fopen (const char * path, const char * mode, String & error);
    VFSFileTest test_file (const char * filename, VFSFileTest test, String & error);
    Index<String> read_folder (const char * filename, String & error);
};

EXPORT GIOTransport aud_plugin_instance;

class GIOFile : public VFSImpl
{
public:
    GIOFile (const char * filename, const char * mode);
    ~GIOFile ();

    // exception
    struct OpenError {
        String error;
    };

protected:
    int64_t fread (void * ptr, int64_t size, int64_t nmemb);
    int64_t fwrite (const void * buf, int64_t size, int64_t nitems);

    int fseek (int64_t offset, VFSSeekType whence);
    int64_t ftell ();
    bool feof ();

    int ftruncate (int64_t length);
    int64_t fsize ();

    int fflush ();

private:
    String m_filename;
    GFile * m_file = nullptr;
    GIOStream * m_iostream = nullptr;
    GInputStream * m_istream = nullptr;
    GOutputStream * m_ostream = nullptr;
    GSeekable * m_seekable = nullptr;
    bool m_eof = false;
};

#define CHECK_ERROR(op, name) do { \
    if (error) { \
        AUDERR ("Cannot %s %s: %s.\n", op, (const char *) name, error->message); \
        g_error_free (error); \
        goto FAILED; \
    } \
} while (0)

#define CHECK_AND_SAVE_ERROR(op, name) do { \
    if (error) { \
        AUDERR ("Cannot %s %s: %s.\n", op, (const char *) name, error->message); \
        errorstr = String (error->message); \
        g_error_free (error); \
        goto FAILED; \
    } \
} while (0)

GIOFile::GIOFile (const char * filename, const char * mode) :
    m_filename (filename)
{
    GError * error = nullptr;
    String errorstr;

    m_file = g_file_new_for_uri (filename);

    switch (mode[0])
    {
    case 'r':
        if (strchr (mode, '+'))
        {
            m_iostream = (GIOStream *) g_file_open_readwrite (m_file, 0, & error);
            CHECK_AND_SAVE_ERROR ("open", filename);
            m_istream = g_io_stream_get_input_stream (m_iostream);
            m_ostream = g_io_stream_get_output_stream (m_iostream);
            m_seekable = (GSeekable *) m_iostream;
        }
        else
        {
            m_istream = (GInputStream *) g_file_read (m_file, 0, & error);
            CHECK_AND_SAVE_ERROR ("open", filename);
            m_seekable = (GSeekable *) m_istream;
        }
        break;
    case 'w':
        if (strchr (mode, '+'))
        {
            m_iostream = (GIOStream *) g_file_replace_readwrite (m_file,
             0, 0, (GFileCreateFlags) 0, 0, & error);
            CHECK_AND_SAVE_ERROR ("open", filename);
            m_istream = g_io_stream_get_input_stream (m_iostream);
            m_ostream = g_io_stream_get_output_stream (m_iostream);
            m_seekable = (GSeekable *) m_iostream;
        }
        else
        {
            m_ostream = (GOutputStream *) g_file_replace (m_file, 0, 0,
             (GFileCreateFlags) 0, 0, & error);
            CHECK_AND_SAVE_ERROR ("open", filename);
            m_seekable = (GSeekable *) m_ostream;
        }
        break;
    case 'a':
        if (strchr (mode, '+'))
        {
            AUDERR ("Cannot open %s: GIO does not support read-and-append mode.\n", filename);
            errorstr = String (_("Read-and-append mode not supported"));
            goto FAILED;
        }
        else
        {
            m_ostream = (GOutputStream *) g_file_append_to (m_file,
             (GFileCreateFlags) 0, 0, & error);
            CHECK_AND_SAVE_ERROR ("open", filename);
            m_seekable = (GSeekable *) m_ostream;
        }
        break;
    default:
        AUDERR ("Cannot open %s: invalid mode.\n", filename);
        errorstr = String (_("Invalid open mode"));
        goto FAILED;
    }

    return;

FAILED:
    g_object_unref (m_file);
    throw OpenError {errorstr};
}

GIOFile::~GIOFile ()
{
    GError * error = nullptr;

    if (m_iostream)
    {
        g_io_stream_close (m_iostream, 0, & error);
        g_object_unref (m_iostream);
        CHECK_ERROR ("close", m_filename);
    }
    else if (m_istream)
    {
        g_input_stream_close (m_istream, 0, & error);
        g_object_unref (m_istream);
        CHECK_ERROR ("close", m_filename);
    }
    else if (m_ostream)
    {
        g_output_stream_close (m_ostream, 0, & error);
        g_object_unref (m_ostream);
        CHECK_ERROR ("close", m_filename);
    }

FAILED:
    g_object_unref (m_file);
}

VFSImpl * GIOTransport::fopen (const char * filename, const char * mode, String & error)
{
#if ! GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    try { return new GIOFile (filename, mode); }
    catch (GIOFile::OpenError & ex)
    {
        error = std::move (ex.error);
        return nullptr;
    }
}

int64_t GIOFile::fread (void * buf, int64_t size, int64_t nitems)
{
    GError * error = nullptr;

    if (! m_istream)
    {
        AUDERR ("Cannot read from %s: not open for reading.\n", (const char *) m_filename);
        return 0;
    }

    int64_t total = 0;
    int64_t remain = size * nitems;

    while (remain > 0)
    {
        int64_t part = g_input_stream_read (m_istream, buf, remain, 0, & error);
        CHECK_ERROR ("read from", m_filename);

        m_eof = (part == 0);

        if (part <= 0)
            break;

        buf = (char *) buf + part;
        total += part;
        remain -= part;
    }

FAILED:
    return (size > 0) ? total / size : 0;
}

int64_t GIOFile::fwrite (const void * buf, int64_t size, int64_t nitems)
{
    GError * error = nullptr;

    if (! m_ostream)
    {
        AUDERR ("Cannot write to %s: not open for writing.\n", (const char *) m_filename);
        return 0;
    }

    int64_t total = 0;
    int64_t remain = size * nitems;

    while (remain > 0)
    {
        int64_t part = g_output_stream_write (m_ostream, buf, remain, 0, & error);
        CHECK_ERROR ("write to", m_filename);

        if (part <= 0)
            break;

        buf = (const char *) buf + part;
        total += part;
        remain -= part;
    }

FAILED:
    return (size > 0) ? total / size : 0;
}

int GIOFile::fseek (int64_t offset, VFSSeekType whence)
{
    GError * error = nullptr;
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
        AUDERR ("Cannot seek within %s: invalid whence.\n", (const char *) m_filename);
        return -1;
    }

    g_seekable_seek (m_seekable, offset, gwhence, nullptr, & error);
    CHECK_ERROR ("seek within", m_filename);

    m_eof = (whence == VFS_SEEK_END && offset == 0);

    return 0;

FAILED:
    return -1;
}

int64_t GIOFile::ftell ()
{
    return g_seekable_tell (m_seekable);
}

bool GIOFile::feof ()
{
    return m_eof;
}

int GIOFile::ftruncate (int64_t length)
{
    GError * error = nullptr;

    g_seekable_truncate (m_seekable, length, nullptr, & error);
    CHECK_ERROR ("truncate", m_filename);

    m_eof = (g_seekable_tell (m_seekable) >= length);

    return 0;

FAILED:
    return -1;
}

int64_t GIOFile::fsize ()
{
    if (! g_seekable_can_seek (m_seekable))
        return -1;

    GError * error = nullptr;
    int64_t saved_pos = g_seekable_tell (m_seekable);
    int64_t size = -1;

    g_seekable_seek (m_seekable, 0, G_SEEK_END, nullptr, & error);
    CHECK_ERROR ("seek within", m_filename);

    size = g_seekable_tell (m_seekable);

    g_seekable_seek (m_seekable, saved_pos, G_SEEK_SET, nullptr, & error);
    CHECK_ERROR ("seek within", m_filename);

    m_eof = (saved_pos >= size);

FAILED:
    return size;
}

int GIOFile::fflush ()
{
    if (! m_ostream)
        return 0;  /* no-op */

    GError * error = nullptr;

    g_output_stream_flush (m_ostream, nullptr, & error);
    CHECK_ERROR ("flush", m_filename);

    return 0;

FAILED:
    return -1;
}

VFSFileTest GIOTransport::test_file (const char * filename, VFSFileTest test, String & error)
{
    GFile * file = g_file_new_for_uri (filename);
    Index<String> attrs;
    int passed = 0;

    if (test & (VFS_IS_REGULAR | VFS_IS_DIR))
        attrs.append (G_FILE_ATTRIBUTE_STANDARD_TYPE);
    if (test & VFS_IS_SYMLINK)
        attrs.append (G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);
    if (test & VFS_IS_EXECUTABLE)
        attrs.append (G_FILE_ATTRIBUTE_UNIX_MODE);

    GError * gerr = nullptr;
    GFileInfo * info = g_file_query_info (file, index_to_str_list (attrs, ","),
     G_FILE_QUERY_INFO_NONE, nullptr, & gerr);

    if (! info)
    {
        passed |= VFS_NO_ACCESS;
        error = String (gerr->message);
        g_error_free (gerr);
    }
    else
    {
        passed |= VFS_EXISTS;

        switch (g_file_info_get_file_type (info))
        {
            case G_FILE_TYPE_REGULAR: passed |= VFS_IS_REGULAR; break;
            case G_FILE_TYPE_DIRECTORY: passed |= VFS_IS_DIR; break;
            default: break;
        };

        if (g_file_info_get_is_symlink (info))
            passed |= VFS_IS_SYMLINK;
        if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE) & S_IXUSR)
            passed |= VFS_IS_EXECUTABLE;

        g_object_unref (info);
    }

    g_object_unref (file);
    return VFSFileTest (test & passed);
}

Index<String> GIOTransport::read_folder (const char * filename, String & error)
{
    GFile * file = g_file_new_for_uri (filename);
    Index<String> files;

    GError * gerr = nullptr;
    GFileEnumerator * dir = g_file_enumerate_children (file,
     G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, nullptr, & gerr);

    if (! dir)
    {
        error = String (gerr->message);
        g_error_free (gerr);
    }
    else
    {
        GFileInfo * info;
        while ((info = g_file_enumerator_next_file (dir, nullptr, nullptr)))
        {
            StringBuf enc = str_encode_percent (g_file_info_get_name (info));
            files.append (String (str_concat ({filename, "/", enc})));
            g_object_unref (info);
        }

        g_object_unref (dir);
    }

    g_object_unref (file);
    return files;
}
