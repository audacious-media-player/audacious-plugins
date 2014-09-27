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

#include <libaudcore/runtime.h>
#include <libaudcore/i18n.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/vfs.h>

#include "util.h"

typedef struct
{
    const char *to_match;
    char *match;
    gboolean found;
} FindFileContext;

#ifndef HAVE_MKDTEMP
static void make_directory(const char *path, mode_t mode);
#endif

char * find_file_case (const char * folder, const char * basename)
{
    static GHashTable * cache = nullptr;
    GList * list = nullptr;
    void * vlist;

    if (cache == nullptr)
        cache = g_hash_table_new ((GHashFunc) str_calc_hash, g_str_equal);

    if (g_hash_table_lookup_extended (cache, folder, nullptr, & vlist))
        list = (GList *) vlist;
    else
    {
        GDir * handle = g_dir_open (folder, 0, nullptr);
        if (! handle)
            return nullptr;

        const char * name;
        while ((name = g_dir_read_name (handle)))
            list = g_list_prepend (list, g_strdup (name));

        g_hash_table_insert (cache, g_strdup (folder), list);
        g_dir_close (handle);
    }

    for (; list != nullptr; list = list->next)
    {
        if (! g_ascii_strcasecmp ((char *) list->data, basename))
            return g_strdup ((char *) list->data);
    }

    return nullptr;
}

char * find_file_case_path (const char * folder, const char * basename)
{
    char * found, * path;

    if ((found = find_file_case (folder, basename)) == nullptr)
        return nullptr;

    path = g_strdup_printf ("%s/%s", folder, found);
    g_free (found);
    return path;
}

VFSFile open_local_file_nocase (const char * folder, const char * basename)
{
    char * path = find_file_case_path (folder, basename);
    if (! path)
        return VFSFile ();

    StringBuf uri = filename_to_uri (path);
    g_free (path);

    if (! uri)
        return VFSFile ();

    return VFSFile (uri, "r");
}

char * text_parse_line (char * text)
{
    char * newline = strchr (text, '\n');

    if (newline == nullptr)
        return nullptr;

    * newline = 0;
    return newline + 1;
}

typedef enum
{
    ARCHIVE_UNKNOWN = 0,
    ARCHIVE_DIR,
    ARCHIVE_TAR,
    ARCHIVE_TGZ,
    ARCHIVE_ZIP,
    ARCHIVE_TBZ2
} ArchiveType;

typedef char *(*ArchiveExtractFunc) (const char *, const char *);

typedef struct
{
    ArchiveType type;
    const char *ext;
} ArchiveExtensionType;

static ArchiveExtensionType archive_extensions[] = {
    {ARCHIVE_TAR, ".tar"},
    {ARCHIVE_ZIP, ".wsz"},
    {ARCHIVE_ZIP, ".zip"},
    {ARCHIVE_TGZ, ".tar.gz"},
    {ARCHIVE_TGZ, ".tgz"},
    {ARCHIVE_TBZ2, ".tar.bz2"},
    {ARCHIVE_TBZ2, ".bz2"},
    {ARCHIVE_UNKNOWN, nullptr}
};

static char *archive_extract_tar(const char *archive, const char *dest);
static char *archive_extract_zip(const char *archive, const char *dest);
static char *archive_extract_tgz(const char *archive, const char *dest);
static char *archive_extract_tbz2(const char *archive, const char *dest);

static ArchiveExtractFunc archive_extract_funcs[] = {
    nullptr,
    nullptr,
    archive_extract_tar,
    archive_extract_tgz,
    archive_extract_zip,
    archive_extract_tbz2
};


/* FIXME: these functions can be generalised into a function using a
 * command lookup table */

static const char *get_tar_command(void)
{
    static const char *command = nullptr;

    if (!command)
    {
        if (!(command = getenv("TARCMD")))
            command = "tar";
    }

    return command;
}

static const char *get_unzip_command(void)
{
    static const char *command = nullptr;

    if (!command)
    {
        if (!(command = getenv("UNZIPCMD")))
            command = "unzip";
    }

    return command;
}


static char *archive_extract_tar(const char *archive, const char *dest)
{
    return g_strdup_printf("%s >/dev/null xf \"%s\" -C %s", get_tar_command(),
                           archive, dest);
}

static char *archive_extract_zip(const char *archive, const char *dest)
{
    return g_strdup_printf("%s >/dev/null -o -j \"%s\" -d %s",
                           get_unzip_command(), archive, dest);
}

static char *archive_extract_tgz(const char *archive, const char *dest)
{
    return g_strdup_printf("%s >/dev/null xzf \"%s\" -C %s", get_tar_command(),
                           archive, dest);
}

static char *archive_extract_tbz2(const char *archive, const char *dest)
{
    return g_strdup_printf("bzip2 -dc \"%s\" | %s >/dev/null xf - -C %s",
                           archive, get_tar_command(), dest);
}


static ArchiveType archive_get_type(const char *filename)
{
    int i = 0;

    if (g_file_test(filename, G_FILE_TEST_IS_DIR))
        return ARCHIVE_DIR;

    while (archive_extensions[i].ext)
    {
        if (g_str_has_suffix(filename, archive_extensions[i].ext))
        {
            return archive_extensions[i].type;
        }
        i++;
    }

    return ARCHIVE_UNKNOWN;
}

gboolean file_is_archive(const char *filename)
{
    return (archive_get_type(filename) > ARCHIVE_DIR);
}

char *archive_basename(const char *str)
{
    int i = 0;

    while (archive_extensions[i].ext)
    {
        if (str_has_suffix_nocase(str, archive_extensions[i].ext))
        {
            const char *end = g_strrstr(str, archive_extensions[i].ext);
            if (end)
            {
                return g_strndup(str, end - str);
            }
            break;
        }
        i++;
    }

    return nullptr;
}

/**
 * Escapes characters that are special to the shell inside double quotes.
 *
 * @param string String to be escaped.
 * @return Given string with special characters escaped. Must be freed with g_free().
 */
static char *
escape_shell_chars(const char * string)
{
    const char *special = "$`\"\\";    /* Characters to escape */
    const char *in = string;
    char *out, *escaped;
    int num = 0;

    while (*in != '\0')
        if (strchr(special, *in++))
            num++;

    escaped = g_new (char, strlen(string) + num + 1);

    in = string;
    out = escaped;

    while (*in != '\0') {
        if (strchr(special, *in))
            *out++ = '\\';
        *out++ = *in++;
    }
    *out = '\0';

    return escaped;
}

/*
   decompress_archive

   Decompresses the archive "filename" to a temporary directory,
   returns the path to the temp dir, or nullptr if failed,
   watch out tho, doesn't actually check if the system command succeeds :-|
*/

char *archive_decompress(const char *filename)
{
    char *tmpdir, *cmd, *escaped_filename;
    ArchiveType type;
#ifndef HAVE_MKDTEMP
#ifdef S_IRGRP
    mode_t mode755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
#else
    mode_t mode755 = S_IRWXU;
#endif
#endif

    if ((type = archive_get_type(filename)) <= ARCHIVE_DIR)
        return nullptr;

#ifdef HAVE_MKDTEMP
    tmpdir = g_build_filename(g_get_tmp_dir(), "audacious.XXXXXXXX", nullptr);
    if (!mkdtemp(tmpdir))
    {
        g_free(tmpdir);
        AUDDBG("Unable to load skin: Failed to create temporary "
               "directory: %s\n", g_strerror(errno));
        return nullptr;
    }
#else
    tmpdir = g_strdup_printf("%s/audacious.%ld", g_get_tmp_dir(), (long)rand());
    make_directory(tmpdir, mode755);
#endif

    escaped_filename = escape_shell_chars(filename);
    cmd = archive_extract_funcs[type] (escaped_filename, tmpdir);
    g_free(escaped_filename);

    if (!cmd)
    {
        AUDDBG("extraction function is nullptr!\n");
        g_free(tmpdir);
        return nullptr;
    }

    AUDDBG("Attempt to execute \"%s\"\n", cmd);

    if (system(cmd) != 0)
    {
        AUDDBG("could not execute cmd %s\n", cmd);
        g_free(cmd);
        return nullptr;
    }
    g_free(cmd);

    return tmpdir;
}

static gboolean del_directory_func(const char *path, const char *basename,
                                   void *params)
{
    if (!strcmp(basename, ".") || !strcmp(path, ".."))
        return FALSE;

    if (g_file_test(path, G_FILE_TEST_IS_DIR))
    {
        dir_foreach(path, del_directory_func, nullptr, nullptr);
        g_rmdir(path);
        return FALSE;
    }

    g_unlink(path);

    return FALSE;
}

void del_directory(const char *path)
{
    dir_foreach(path, del_directory_func, nullptr, nullptr);
    g_rmdir(path);
}

GArray *string_to_garray(const char *str)
{
    GArray *array;
    int temp;
    const char *ptr = str;
    char *endptr;

    array = g_array_new(FALSE, TRUE, sizeof(int));
    for (;;)
    {
        temp = strtol(ptr, &endptr, 10);
        if (ptr == endptr)
            break;
        g_array_append_val(array, temp);
        ptr = endptr;
        while (!g_ascii_isdigit((int)*ptr) && (*ptr) != '\0')
            ptr++;
        if (*ptr == '\0')
            break;
    }
    return (array);
}

gboolean dir_foreach(const char *path, DirForeachFunc function,
                     void * user_data, GError **error)
{
    GError *error_out = nullptr;
    GDir *dir;
    const char *entry;
    char *entry_fullpath;

    if (!(dir = g_dir_open(path, 0, &error_out)))
    {
        g_propagate_error(error, error_out);
        return FALSE;
    }

    while ((entry = g_dir_read_name(dir)))
    {
        entry_fullpath = g_build_filename(path, entry, nullptr);

        if ((*function) (entry_fullpath, entry, user_data))
        {
            g_free(entry_fullpath);
            break;
        }

        g_free(entry_fullpath);
    }

    g_dir_close(dir);

    return TRUE;
}

#ifndef HAVE_MKDTEMP
static void make_directory(const char *path, mode_t mode)
{
    if (g_mkdir_with_parents(path, mode) == 0)
        return;

    g_printerr(_("Could not create directory (%s): %s\n"), path,
               g_strerror(errno));
}
#endif
