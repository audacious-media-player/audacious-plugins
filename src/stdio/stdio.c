/*  Audacious
 *  Copyright (c) 2006 William Pitcock
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <audacious/plugin.h>
#include <audacious/strings.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string.h>

static gchar *
aud_vfs_stdio_urldecode_path(const gchar * encoded_path)
{
    const gchar *cur, *ext;
    gchar *path, *tmp;
    gint realchar;

    if (!encoded_path)
        return NULL;

    if (!aud_str_has_prefix_nocase(encoded_path, "file:"))
        return NULL;

    cur = encoded_path + 5;

    if (aud_str_has_prefix_nocase(cur, "//localhost"))
        cur += 11;

    if (*cur == '/')
        while (cur[1] == '/')
            cur++;

    tmp = g_malloc0(strlen(cur) + 1);

    while ((ext = strchr(cur, '%')) != NULL) {
        strncat(tmp, cur, ext - cur);
        ext++;
        cur = ext + 2;
        if (!sscanf(ext, "%2x", &realchar)) {
            /* Assume it is a literal '%'.  Several file
             * managers send unencoded file: urls on drag
             * and drop. */
            realchar = '%';
            cur -= 2;
        }
        tmp[strlen(tmp)] = realchar;
    }

    path = g_strconcat(tmp, cur, NULL);
    g_free(tmp);
    return path;
}

VFSFile *
stdio_aud_vfs_fopen_impl(const gchar * path,
          const gchar * mode)
{
    VFSFile *file;
    gchar *decpath;

    if (!path || !mode)
	return NULL;

    decpath = aud_vfs_stdio_urldecode_path(path);

    file = g_new(VFSFile, 1);

    file->handle = fopen(decpath != NULL ? decpath : path, mode);

    if (decpath != NULL)
        g_free(decpath);

    if (file->handle == NULL) {
        g_free(file);
        file = NULL;
    }

    return file;
}

gint
stdio_aud_vfs_fclose_impl(VFSFile * file)
{
    gint ret = 0;

    if (file == NULL)
        return -1;

    if (file->handle)
    {
        FILE *handle = (FILE *) file->handle;

        if (fclose(handle) != 0)
            ret = -1;
        file->handle = NULL;
    }

    return ret;
}

size_t
stdio_aud_vfs_fread_impl(gpointer ptr,
          size_t size,
          size_t nmemb,
          VFSFile * file)
{
    FILE *handle;

    if (file == NULL)
        return 0;

    handle = (FILE *) file->handle;

    return fread(ptr, size, nmemb, handle);
}

size_t
stdio_aud_vfs_fwrite_impl(gconstpointer ptr,
           size_t size,
           size_t nmemb,
           VFSFile * file)
{
    FILE *handle;

    if (file == NULL)
        return 0;

    handle = (FILE *) file->handle;

    return fwrite(ptr, size, nmemb, handle);
}

gint
stdio_aud_vfs_getc_impl(VFSFile *stream)
{
  FILE *handle = (FILE *) stream->handle;

  return getc( handle );
}

gint
stdio_aud_vfs_ungetc_impl(gint c, VFSFile *stream)
{
  FILE *handle = (FILE *) stream->handle;

  return ungetc( c , handle );
}

gint
stdio_aud_vfs_fseek_impl(VFSFile * file,
          glong offset,
          gint whence)
{
    FILE *handle;

    if (file == NULL)
        return 0;

    handle = (FILE *) file->handle;

    return fseek(handle, offset, whence);
}

void
stdio_aud_vfs_rewind_impl(VFSFile * file)
{
    FILE *handle;

    if (file == NULL)
        return;

    handle = (FILE *) file->handle;

    rewind(handle);
}

glong
stdio_aud_vfs_ftell_impl(VFSFile * file)
{
    FILE *handle;

    if (file == NULL)
        return 0;

    handle = (FILE *) file->handle;

    return ftell(handle);
}

gboolean
stdio_aud_vfs_feof_impl(VFSFile * file)
{
    FILE *handle;

    if (file == NULL)
        return FALSE;

    handle = (FILE *) file->handle;

    return (gboolean) feof(handle);
}

gint
stdio_aud_vfs_truncate_impl(VFSFile * file, glong size)
{
    FILE *handle;

    if (file == NULL)
        return -1;

    handle = (FILE *) file->handle;

    return ftruncate(fileno(handle), size);
}

off_t
stdio_aud_vfs_fsize_impl(VFSFile * file)
{
    FILE *handle;
    struct stat s;

    if (file == NULL)
        return -1;

    handle = (FILE *) file->handle;

    if (-1 == fstat(fileno(handle), &s))
        return -1;

    return s.st_size;
}

VFSConstructor file_const = {
	.uri_id = "file://",
	.vfs_fopen_impl = stdio_aud_vfs_fopen_impl,
	.vfs_fclose_impl = stdio_aud_vfs_fclose_impl,
	.vfs_fread_impl = stdio_aud_vfs_fread_impl,
	.vfs_fwrite_impl = stdio_aud_vfs_fwrite_impl,
	.vfs_getc_impl = stdio_aud_vfs_getc_impl,
	.vfs_ungetc_impl = stdio_aud_vfs_ungetc_impl,
	.vfs_fseek_impl = stdio_aud_vfs_fseek_impl,
	.vfs_rewind_impl = stdio_aud_vfs_rewind_impl,
	.vfs_ftell_impl = stdio_aud_vfs_ftell_impl,
	.vfs_feof_impl = stdio_aud_vfs_feof_impl,
	.vfs_truncate_impl = stdio_aud_vfs_truncate_impl,
	.vfs_fsize_impl = stdio_aud_vfs_fsize_impl
};

static void init(void)
{
	aud_vfs_register_transport(&file_const);
}

static void cleanup(void)
{
#if 0
	aud_vfs_unregister_transport(&file_const);
#endif
}

DECLARE_PLUGIN(stdio, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL);
