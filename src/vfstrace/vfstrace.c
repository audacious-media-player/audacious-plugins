/*  Audacious
 *  Copyright (c) 2009 William Pitcock
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

#include <audacious/plugin.h>
#include "config.h"
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#define PRIu64 "llu"
#endif

VFSFile *(*vfs_fopen_impl)(const gchar *path, const gchar *mode) = NULL;
gint (*vfs_fclose_impl)(VFSFile * file) = NULL;
gint64 (* vfs_fread_impl) (void * ptr, gint64 size, gint64 nmemb, VFSFile *
 file) = NULL;
gint64 (* vfs_fwrite_impl) (const void * ptr, gint64 size, gint64 nmemb,
 VFSFile * file) = NULL;
gint (*vfs_getc_impl)(VFSFile *stream) = NULL;
gint (*vfs_ungetc_impl)(gint c, VFSFile *stream) = NULL;
gint (*vfs_fseek_impl)(VFSFile *file, glong offset, gint whence) = NULL;
void (*vfs_rewind_impl)(VFSFile *file) = NULL;
glong (*vfs_ftell_impl)(VFSFile *file) = NULL;
gboolean (*vfs_feof_impl)(VFSFile *file) = NULL;
gint (* vfs_ftruncate_impl) (VFSFile * file, gint64 length) = NULL;
off_t (*vfs_fsize_impl)(VFSFile *file) = NULL;

VFSFile *
vt_vfs_fopen_impl(const gchar *path, const gchar *mode)
{
    VFSFile *ret;

    ret = vfs_fopen_impl(path, mode);
    g_print("%p fopen       : path:%s : mode:%s : ret:%p\n", ret, path, mode, ret);

    return ret;
}

gint
vt_vfs_fclose_impl(VFSFile *file)
{
    gint ret;

    ret = vfs_fclose_impl(file);
    g_print("%p fclose      : file:%p : ret:%d\n", file, file, ret);

    return ret;
}

gint64 vt_vfs_fread_impl (void * ptr, gint64 size, gint64 nmemb, VFSFile * file)
{
    gsize ret;

    ret = vfs_fread_impl(ptr, size, nmemb, file);
    g_print("%p fread       : size:%" PRIu64 " : nmemb:%" PRIu64 " : ret:%" PRIu64 "\n",
        file, (guint64) size, (guint64) nmemb, (guint64) ret);

    return ret;
}

gint64 vt_vfs_fwrite_impl (const void * ptr, gint64 size, gint64 nmemb,
 VFSFile * file)
{
    gsize ret;

    ret = vfs_fwrite_impl(ptr, size, nmemb, file);
    g_print("%p fwrite      : size:%" PRIu64 " : nmemb:%" PRIu64 " : ret:%" PRIu64 "\n",
        file, (guint64) size, (guint64) nmemb, (guint64) ret);

    return ret;
}

gint
vt_vfs_getc_impl(VFSFile *stream)
{
    gint ret;

    ret = vfs_getc_impl(stream);
    g_print("%p getc        : ret:%d\n", stream, ret);

    return ret;
}

gint
vt_vfs_ungetc_impl(gint c, VFSFile *stream)
{
    gint ret;

    ret = vfs_ungetc_impl(c, stream);
    g_print("%p ungetc      : c:%d : ret:%d\n", stream, c, ret);

    return ret;
}

gint
vt_vfs_fseek_impl(VFSFile *file, glong offset, gint whence)
{
    gint ret;

    ret = vfs_fseek_impl(file, offset, whence);
    g_print("%p fseek       : offset:%ld : whence:%d : ret:%d\n", file, offset, whence, ret);

    return ret;
}

void
vt_vfs_rewind_impl(VFSFile *file)
{
    vfs_rewind_impl(file);
    g_print("%p rewind\n", file);
}

glong
vt_vfs_ftell_impl(VFSFile *file)
{
    glong ret;

    ret = vfs_ftell_impl(file);
    g_print("%p ftell       : ret:%ld\n", file, ret);

    return ret;
}

gboolean
vt_vfs_feof_impl(VFSFile *file)
{
    gboolean ret;

    ret = vfs_feof_impl(file);
    g_print("%p feof        : ret:%d\n", file, ret);

    return ret;
}

gint vt_vfs_ftruncate_impl (VFSFile * file, gint64 length)
{
    gboolean ret;

    ret = vfs_ftruncate_impl (file, length);
    g_print("%p truncate    : length:%ld : ret:%d\n", file, length, ret);

    return ret;
}

off_t
vt_vfs_fsize_impl(VFSFile *file)
{
    off_t ret;

    ret = vfs_fsize_impl(file);
    g_print("%p fsize       : ret:%lu\n", file, ret);

    return ret;
}

void
patch_vfs(void)
{
    vfs_fopen_impl = aud_vfs_fopen;
    vfs_fclose_impl = aud_vfs_fclose;
    vfs_fread_impl = aud_vfs_fread;
    vfs_fwrite_impl = aud_vfs_fwrite;
    vfs_getc_impl = aud_vfs_getc;
    vfs_ungetc_impl = aud_vfs_ungetc;
    vfs_fseek_impl = aud_vfs_fseek;
    vfs_rewind_impl = aud_vfs_rewind;
    vfs_ftell_impl = aud_vfs_ftell;
    vfs_feof_impl = aud_vfs_feof;
    vfs_ftruncate_impl = aud_vfs_ftruncate;
    vfs_fsize_impl = aud_vfs_fsize;

    aud_vfs_fopen = vt_vfs_fopen_impl;
    aud_vfs_fclose = vt_vfs_fclose_impl;
    aud_vfs_fread = vt_vfs_fread_impl;
    aud_vfs_fwrite = vt_vfs_fwrite_impl;
    aud_vfs_getc = vt_vfs_getc_impl;
    aud_vfs_ungetc = vt_vfs_ungetc_impl;
    aud_vfs_fseek = vt_vfs_fseek_impl;
    aud_vfs_rewind = vt_vfs_rewind_impl;
    aud_vfs_ftell = vt_vfs_ftell_impl;
    aud_vfs_feof = vt_vfs_feof_impl;
    aud_vfs_ftruncate = vt_vfs_ftruncate_impl;
    aud_vfs_fsize = vt_vfs_fsize_impl;
}

void
unpatch_vfs(void)
{
    aud_vfs_fopen = vfs_fopen_impl;
    aud_vfs_fclose = vfs_fclose_impl;
    aud_vfs_fread = vfs_fread_impl;
    aud_vfs_fwrite = vfs_fwrite_impl;
    aud_vfs_getc = vfs_getc_impl;
    aud_vfs_ungetc = vfs_ungetc_impl;
    aud_vfs_fseek = vfs_fseek_impl;
    aud_vfs_rewind = vfs_rewind_impl;
    aud_vfs_ftell = vfs_ftell_impl;
    aud_vfs_feof = vfs_feof_impl;
    aud_vfs_ftruncate = vfs_ftruncate_impl;
    aud_vfs_fsize = vfs_fsize_impl;
}

static GeneralPlugin vfstrace_plugin =
{
     .description = "VFSTrace",
     .init = patch_vfs,
     .cleanup = unpatch_vfs,
};

GeneralPlugin *vfstrace_gplist[] = { &vfstrace_plugin, NULL };
SIMPLE_GENERAL_PLUGIN(vfstrace, vfstrace_gplist);

