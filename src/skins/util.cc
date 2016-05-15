/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2009  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/hook.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>
#include <libaudcore/vfs.h>

#include "util.h"

#ifdef S_IRGRP
#define DIRMODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#else
#define DIRMODE (S_IRWXU)
#endif

StringBuf find_file_case_path (const char * folder, const char * basename)
{
    static SimpleHash<String, Index<String>> cache;

    String key (folder);
    Index<String> * list = cache.lookup (key);

    if (! list)
    {
        GDir * handle = g_dir_open (folder, 0, nullptr);
        if (! handle)
            return StringBuf ();

        list = cache.add (key, Index<String> ());

        const char * name;
        while ((name = g_dir_read_name (handle)))
            list->append (name);

        g_dir_close (handle);
    }

    for (const String & entry : * list)
    {
        if (! strcmp_nocase (entry, basename))
            return filename_build ({folder, entry});
    }

    return StringBuf ();
}

VFSFile open_local_file_nocase (const char * folder, const char * basename)
{
    StringBuf path = find_file_case_path (folder, basename);
    return path ? VFSFile (filename_to_uri (path), "r") : VFSFile ();
}

StringBuf skin_pixmap_locate (const char * folder, const char * basename, const char * altname)
{
    static const char * const exts[] = {".bmp", ".png", ".xpm"};

    for (const char * ext : exts)
    {
        StringBuf name = str_concat({basename, ext});
        name.steal (find_file_case_path (folder, name));

        if (name)
            return name;
    }

    return altname ? skin_pixmap_locate (folder, altname) : StringBuf ();
}

char * text_parse_line (char * text)
{
    char * newline = strchr (text, '\n');

    if (newline == nullptr)
        return nullptr;

    * newline = 0;
    return newline + 1;
}

enum ArchiveType {
    ARCHIVE_UNKNOWN = 0,
    ARCHIVE_TAR,
    ARCHIVE_TGZ,
    ARCHIVE_ZIP,
    ARCHIVE_TBZ2
};

typedef StringBuf (* ArchiveExtractFunc) (const char *, const char *);

struct ArchiveExtensionType {
    ArchiveType type;
    const char *ext;
};

static ArchiveExtensionType archive_extensions[] = {
    {ARCHIVE_TAR, ".tar"},
    {ARCHIVE_ZIP, ".wsz"},
    {ARCHIVE_ZIP, ".zip"},
    {ARCHIVE_TGZ, ".tar.gz"},
    {ARCHIVE_TGZ, ".tgz"},
    {ARCHIVE_TBZ2, ".tar.bz2"},
    {ARCHIVE_TBZ2, ".bz2"}
};

static StringBuf archive_extract_tar (const char * archive, const char * dest);
static StringBuf archive_extract_zip (const char * archive, const char * dest);
static StringBuf archive_extract_tgz (const char * archive, const char * dest);
static StringBuf archive_extract_tbz2 (const char * archive, const char * dest);

static ArchiveExtractFunc archive_extract_funcs[] = {
    nullptr,
    archive_extract_tar,
    archive_extract_tgz,
    archive_extract_zip,
    archive_extract_tbz2
};

/* FIXME: these functions can be generalised into a function using a
 * command lookup table */

static const char * get_tar_command ()
{
    static const char * command = nullptr;

    if (! command)
    {
        if (! (command = getenv("TARCMD")))
            command = "tar";
    }

    return command;
}

static const char * get_unzip_command ()
{
    static const char * command = nullptr;

    if (! command)
    {
        if (! (command = getenv("UNZIPCMD")))
            command = "unzip";
    }

    return command;
}

static StringBuf archive_extract_tar (const char * archive, const char * dest)
{
    return str_printf ("%s >/dev/null xf \"%s\" -C %s", get_tar_command (), archive, dest);
}

static StringBuf archive_extract_zip (const char * archive, const char * dest)
{
    return str_printf ("%s >/dev/null -o -j \"%s\" -d %s", get_unzip_command (), archive, dest);
}

static StringBuf archive_extract_tgz (const char * archive, const char * dest)
{
    return str_printf ("%s >/dev/null xzf \"%s\" -C %s", get_tar_command (), archive, dest);
}

static StringBuf archive_extract_tbz2 (const char * archive, const char * dest)
{
    return str_printf ("bzip2 -dc \"%s\" | %s >/dev/null xf - -C %s", archive,
     get_tar_command (), dest);
}

static ArchiveType archive_get_type (const char * filename)
{
    for (auto & ext : archive_extensions)
    {
        if (str_has_suffix_nocase (filename, ext.ext))
            return ext.type;
    }

    return ARCHIVE_UNKNOWN;
}

bool file_is_archive (const char * filename)
{
    return (archive_get_type (filename) != ARCHIVE_UNKNOWN);
}

StringBuf archive_basename (const char * str)
{
    for (auto & ext : archive_extensions)
    {
        if (str_has_suffix_nocase (str, ext.ext))
            return str_copy (str, strlen (str) - strlen (ext.ext));
    }

    return StringBuf ();
}

/**
 * Escapes characters that are special to the shell inside double quotes.
 *
 * @param string String to be escaped.
 * @return Given string with special characters escaped.
 */
static StringBuf escape_shell_chars (const char * string)
{
    const char *special = "$`\"\\";    /* Characters to escape */

    int num = 0;
    for (const char * in = string; * in; in ++)
    {
        if (strchr (special, * in))
            num ++;
    }

    StringBuf escaped (strlen (string) + num);

    char * out = escaped;
    for (const char * in = string; * in; in ++)
    {
        if (strchr (special, * in))
            * out ++ = '\\';
        * out ++ = * in;
    }

    return escaped;
}

/**
 * Decompresses the archive "filename" to a temporary directory,
 * returns the path to the temp dir, or nullptr if failed
 */
StringBuf archive_decompress (const char * filename)
{
    ArchiveType type = archive_get_type (filename);
    if (type == ARCHIVE_UNKNOWN)
        return StringBuf ();

    StringBuf tmpdir = filename_build ({g_get_tmp_dir (), "audacious.XXXXXX"});
    if (! g_mkdtemp (tmpdir))
    {
        AUDWARN ("Error creating %s: %s\n", (const char *) tmpdir, strerror (errno));
        return StringBuf ();
    }

    StringBuf escaped_filename = escape_shell_chars (filename);
    StringBuf cmd = archive_extract_funcs[type] (escaped_filename, tmpdir);

    AUDDBG ("Executing \"%s\"\n", (const char *) cmd);
    int ret = system (cmd);
    if (ret != 0)
    {
        AUDDBG ("Command \"%s\" returned error %d\n", (const char *) cmd, ret);
        return StringBuf ();
    }

    return tmpdir;
}

static void del_directory_func (const char * path, const char *)
{
    if (g_file_test (path, G_FILE_TEST_IS_DIR))
        del_directory (path);
    else
        g_unlink (path);
}

void del_directory (const char * path)
{
    dir_foreach (path, del_directory_func);
    g_rmdir (path);
}

Index<int> string_to_int_array (const char * str)
{
    Index<int> array;
    int temp;
    const char * ptr = str;
    char * endptr;

    for (;;)
    {
        temp = strtol (ptr, &endptr, 10);
        if (ptr == endptr)
            break;
        array.append (temp);
        ptr = endptr;
        while (! g_ascii_isdigit ((int) * ptr) && (* ptr) != '\0')
            ptr ++;
        if (* ptr == '\0')
            break;
    }

    return array;
}

bool dir_foreach (const char * path, DirForeachFunc func)
{
    GError * error = nullptr;
    GDir * dir = g_dir_open (path, 0, & error);
    if (! dir)
    {
        AUDWARN ("Error reading %s: %s\n", path, error->message);
        g_error_free (error);
        return false;
    }

    const char * entry;
    while ((entry = g_dir_read_name (dir)))
        func (filename_build ({path, entry}), entry);

    g_dir_close (dir);
    return true;
}

void make_directory (const char * path)
{
    if (g_mkdir_with_parents (path, DIRMODE) != 0)
        AUDWARN ("Error creating %s: %s\n", path, strerror (errno));
}
