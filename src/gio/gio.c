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

#include "config.h"
#include <audacious/plugin.h>
#include <stdio.h>
#include <gio/gio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string.h>

typedef struct {
    GFile *file;
    GFileInputStream *istream;
    GFileOutputStream *ostream;
    GSeekable *seekable;
} VFSGIOHandle;

VFSFile *
gio_aud_vfs_fopen_impl(const gchar *path, const gchar *mode)
{
    VFSFile *file;
    VFSGIOHandle *handle;
    GError *error = NULL;

    if (path == NULL || mode == NULL)
	    return NULL;

    handle = g_slice_new0(VFSGIOHandle);
    handle->file = g_file_new_for_uri(path);

    if (*mode == 'r')
    {
        handle->istream = g_file_read(handle->file, NULL, &error);
        handle->seekable = G_SEEKABLE(handle->istream);
    }
    else if (*mode == 'w')
    {
        handle->ostream = g_file_replace(handle->file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
        handle->seekable = G_SEEKABLE(handle->ostream);
    }
    else
    {
        g_warning("UNSUPPORTED ACCESS MODE: %s", mode);
        g_object_unref(handle->file);
        g_slice_free(VFSGIOHandle, handle);
        return NULL;
    }

    if (handle->istream == NULL && handle->ostream == NULL)
    {
        g_warning("Could not open %s for reading or writing: %s", path, error->message);
        g_object_unref(handle->file);
        g_slice_free(VFSGIOHandle, handle);
        g_error_free(error);
        return NULL;
    }

    file = g_new(VFSFile, 1);
    file->handle = handle;

    return file;
}

gint
gio_aud_vfs_fclose_impl(VFSFile * file)
{
    gint ret = 0;

    if (file == NULL)
        return -1;

    if (file->handle)
    {
        VFSGIOHandle *handle = (VFSGIOHandle *) file->handle;

        g_object_unref(handle->file);
        g_slice_free(VFSGIOHandle, handle);

        file->handle = NULL;
    }

    return ret;
}

size_t
gio_aud_vfs_fread_impl(gpointer ptr,
          size_t size,
          size_t nmemb,
          VFSFile * file)
{
    VFSGIOHandle *handle;

    if (file == NULL)
        return 0;

    handle = (VFSGIOHandle *) file->handle;

    return g_input_stream_read(G_INPUT_STREAM(handle->istream), ptr, size * nmemb, NULL, NULL);
}

size_t
gio_aud_vfs_fwrite_impl(gconstpointer ptr,
           size_t size,
           size_t nmemb,
           VFSFile * file)
{
    VFSGIOHandle *handle;

    if (file == NULL)
        return 0;

    handle = (VFSGIOHandle *) file->handle;

    return g_output_stream_write(G_OUTPUT_STREAM(handle->ostream), ptr, size * nmemb, NULL, NULL);
}

gint
gio_aud_vfs_getc_impl(VFSFile *file)
{
    guchar buf[1];
    VFSGIOHandle *handle;

    if (file == NULL)
        return -1;

    handle = (VFSGIOHandle *) file->handle;

    g_input_stream_read(G_INPUT_STREAM(handle->istream), &buf, 1, NULL, NULL);

    return *buf;
}

gint
gio_aud_vfs_ungetc_impl(gint c, VFSFile * file)
{
    g_print("ungetc(): unimplemented function!\n");
    return 0;
#if 0
    VFSGIOHandle *handle;
    
    if (file == NULL)
        return -1;
	
    handle = (VFSGIOHandle *) file->handle;
	
    return ungetc(c, handle);
#endif
}

gint
gio_aud_vfs_fseek_impl(VFSFile * file,
          glong offset,
          gint whence)
{
    VFSGIOHandle *handle;
    GSeekType seektype;

    if (file == NULL)
        return 0;

    handle = (VFSGIOHandle *) file->handle;

    if (!g_seekable_can_seek(handle->seekable))
        return 0;

    switch (whence)
    {
    case SEEK_CUR:
        seektype = G_SEEK_CUR;
        break;
    case SEEK_END:
        seektype = G_SEEK_END;
        break;
    default:
        seektype = G_SEEK_SET;
        break;
    }

    return g_seekable_seek(handle->seekable, seektype, offset, NULL, NULL);
}

void
gio_aud_vfs_rewind_impl(VFSFile * file)
{
    if (file == NULL)
        return;

    file->base->vfs_fseek_impl(file, 0, SEEK_SET);
}

glong
gio_aud_vfs_ftell_impl(VFSFile * file)
{
    VFSGIOHandle *handle;

    if (file == NULL)
        return 0;

    handle = (VFSGIOHandle *) file->handle;

    return (glong) g_seekable_tell(handle->seekable);
}

gboolean
gio_aud_vfs_feof_impl(VFSFile * file)
{
    return (file->base->vfs_ftell_impl(file) == file->base->vfs_fsize_impl(file));
}

gint
gio_aud_vfs_truncate_impl(VFSFile * file, glong size)
{
    VFSGIOHandle *handle;

    if (file == NULL)
        return -1;

    handle = (VFSGIOHandle *) file->handle;

    return g_seekable_truncate(handle->seekable, size, NULL, NULL);
}

off_t
gio_aud_vfs_fsize_impl(VFSFile * file)
{
    GFileInfo *info;
    VFSGIOHandle *handle;
    GError *error = NULL;
    goffset size;

    if (file == NULL)
        return -1;

    handle = (VFSGIOHandle *) file->handle;
    info = g_file_query_info(handle->file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, &error);

    if (info == NULL)
    {
        g_warning("gio fsize(): error: %s", error->message);
        g_error_free(error);
        return -1;
    }

    size = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    g_object_unref(info);

    return size;
}

VFSConstructor file_const = {
	.uri_id = "file://",
	.vfs_fopen_impl = gio_aud_vfs_fopen_impl,
	.vfs_fclose_impl = gio_aud_vfs_fclose_impl,
	.vfs_fread_impl = gio_aud_vfs_fread_impl,
	.vfs_fwrite_impl = gio_aud_vfs_fwrite_impl,
	.vfs_getc_impl = gio_aud_vfs_getc_impl,
	.vfs_ungetc_impl = gio_aud_vfs_ungetc_impl,
	.vfs_fseek_impl = gio_aud_vfs_fseek_impl,
	.vfs_rewind_impl = gio_aud_vfs_rewind_impl,
	.vfs_ftell_impl = gio_aud_vfs_ftell_impl,
	.vfs_feof_impl = gio_aud_vfs_feof_impl,
	.vfs_truncate_impl = gio_aud_vfs_truncate_impl,
	.vfs_fsize_impl = gio_aud_vfs_fsize_impl
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

DECLARE_PLUGIN(gio, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL);
