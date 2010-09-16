/*  Audacious
 *  Copyright (c) 2007 William Pitcock
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

#include <glib.h>

#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string.h>
#include <errno.h>

#include <libmms/mms.h>
#include <libmms/mmsh.h>

#include <audacious/debug.h>
#include <audacious/plugin.h>

typedef struct {
    mms_t *mms;
    mmsh_t *mmsh;
    GSList *charstack;
} MMSHandle;

VFSFile *
mms_vfs_fopen_impl(const gchar * path,
          const gchar * mode)
{
    AUDDBG("Opening %s.\n", path);

    VFSFile *file;
    MMSHandle *handle;

    if (!path || !mode)
	return NULL;

    file = g_new(VFSFile, 1);
    handle = g_new0(MMSHandle, 1);

    file->handle = handle;

    if (file->handle == NULL) {
        g_free(file);
        file = NULL;
    }

    handle->mmsh = mmsh_connect(NULL, NULL, path, 128 * 1024);

    if (handle->mmsh == NULL) {
        AUDDBG("Failed to connect with MMSH protocol; trying MMS.\n");
        handle->mms = mms_connect(NULL, NULL, path, 128 * 1024);
    }

    if (handle->mms == NULL && handle->mmsh == NULL)
    {
        fprintf(stderr, "mms: Failed to open %s.\n", path);
        g_free(handle);
        g_free(file);
        file = NULL;
    }

    return file;
}

gint
mms_vfs_fclose_impl(VFSFile * file)
{
    gint ret = 0;

    if (file == NULL)
        return -1;

    if (file->handle)
    {
        MMSHandle *handle = (MMSHandle *) file->handle;

        if (handle->mms != NULL)
            mms_close(handle->mms);
        else /* if (handle->mmsh != NULL) */
            mmsh_close(handle->mmsh);

        g_free(handle);
        file->handle = NULL;
    }

    return ret;
}

gint64 mms_vfs_fread_impl (void * ptr, gint64 size, gint64 nmemb, VFSFile *
 file)
{
    MMSHandle *handle;
    gint ret;

    handle = (MMSHandle *) file->handle;

    if (handle->mms != NULL)
        ret = mms_read(NULL, handle->mms, ptr, size * nmemb);
    else /* if (handle->mmsh != NULL) */
        ret = mmsh_read(NULL, handle->mmsh, ptr, size * nmemb);

    if (ret < 0)
    {
        g_message("** mms **: errno(%d): %s", errno, strerror(errno));
        ret = EOF;
    }

    return ret;
}

gint64 mms_vfs_fwrite_impl (const void * ptr, gint64 size, gint64 nmemb,
 VFSFile * file)
{
    return 0;
}

gint
mms_vfs_getc_impl(VFSFile *stream)
{
    MMSHandle *handle = (MMSHandle *) stream->handle;
    guchar c;

    if (handle->charstack != NULL)
    {
        c = GPOINTER_TO_UINT(handle->charstack->data);
        handle->charstack = g_slist_remove_link(handle->charstack, handle->charstack);
        return c;
    }
    else
    {
        if (handle->mms != NULL)
            mms_read(NULL, handle->mms, (char *)&c, 1);
        else /* if (handle->mmsh != NULL) */
            mmsh_read(NULL, handle->mmsh, (char *)&c, 1);

        return c;
    }

    return EOF;
}

gint
mms_vfs_ungetc_impl(gint c, VFSFile *stream)
{
    MMSHandle *handle = (MMSHandle *) stream->handle;

    handle->charstack = g_slist_append(handle->charstack, GUINT_TO_POINTER(c));

    if (handle->charstack != NULL)
        return c;

    return EOF;
}

gint
mms_vfs_fseek_impl(VFSFile * file,
          glong offset,
          gint whence)
{
    return -1;
}

void
mms_vfs_rewind_impl(VFSFile * file)
{
    return;
}

glong
mms_vfs_ftell_impl(VFSFile * file)
{
    MMSHandle *handle = (MMSHandle *) file->handle;

    if (handle->mms != NULL)
        return mms_get_current_pos(handle->mms);
    else /* if (handle->mmsh != NULL) */
        return mmsh_get_current_pos(handle->mmsh);
}

gboolean
mms_vfs_feof_impl(VFSFile * file)
{
    MMSHandle *handle = (MMSHandle *) file->handle;

    if (handle->mms != NULL)
        return (gboolean) (mms_get_current_pos(handle->mms) ==
         mms_get_length(handle->mms));
    else /* if (handle->mmsh != NULL) */
        return (gboolean) (mmsh_get_current_pos(handle->mmsh) ==
         mmsh_get_length(handle->mmsh));
}

gint
mms_vfs_truncate_impl(VFSFile * file, glong size)
{
    return -1;
}

off_t
mms_vfs_fsize_impl(VFSFile * file)
{
    return -1;
}

VFSConstructor mms_const = {
	"mms://",
	mms_vfs_fopen_impl,
	mms_vfs_fclose_impl,
	mms_vfs_fread_impl,
	mms_vfs_fwrite_impl,
	mms_vfs_getc_impl,
	mms_vfs_ungetc_impl,
	mms_vfs_fseek_impl,
	mms_vfs_rewind_impl,
	mms_vfs_ftell_impl,
	mms_vfs_feof_impl,
	mms_vfs_truncate_impl,
	mms_vfs_fsize_impl
};

static void init(void)
{
	vfs_register_transport(&mms_const);
}

static void cleanup(void)
{
#if 0
	vfs_unregister_transport(&mms_const);
#endif
}

DECLARE_PLUGIN (mms, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL)
