/*  MMS/MMSH Transport for Audacious
 *  Copyright (c) 2007 William Pitcock
 *  Copyright (c) 2011 John Lindgren
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

#define BUFSIZE 65536
#define BLOCKSIZE 4096

typedef struct {
    mms_t *mms;
    mmsh_t *mmsh;
    guchar * buf;
    gint64 offset;
    gint len, used;
} MMSHandle;

static void * mms_vfs_fopen_impl (const gchar * path, const gchar * mode)
{
    AUDDBG("Opening %s.\n", path);

    MMSHandle *handle;
    handle = g_new0(MMSHandle, 1);
    handle->mmsh = mmsh_connect(NULL, NULL, path, 128 * 1024);

    if (handle->mmsh == NULL) {
        AUDDBG("Failed to connect with MMSH protocol; trying MMS.\n");
        handle->mms = mms_connect(NULL, NULL, path, 128 * 1024);
    }

    if (handle->mms == NULL && handle->mmsh == NULL)
    {
        fprintf(stderr, "mms: Failed to open %s.\n", path);
        g_free(handle);
        return NULL;
    }

    handle->buf = g_malloc (BUFSIZE);
    return handle;
}

static gint mms_vfs_fclose_impl (VFSFile * file)
{
    MMSHandle *handle = (MMSHandle *) vfs_get_handle (file);

    if (handle->mms != NULL)
        mms_close(handle->mms);
    else /* if (handle->mmsh != NULL) */
        mmsh_close(handle->mmsh);

    g_free (handle->buf);
    g_free (handle);

    return 0;
}

static gint64 mms_vfs_fread_impl (void * buf, gint64 size, gint64 count,
 VFSFile * file)
{
    MMSHandle * h = vfs_get_handle (file);
    gint64 goal = size * count;
    gint64 total = 0;

    while (total < goal)
    {
        if (h->used == h->len)
        {
            if (h->len == BUFSIZE)
            {
                memmove (h->buf, h->buf + BLOCKSIZE, BUFSIZE - BLOCKSIZE);
                h->offset += BLOCKSIZE;
                h->len = BUFSIZE - BLOCKSIZE;
                h->used = BUFSIZE - BLOCKSIZE;
            }

            gint size = MIN (BLOCKSIZE, BUFSIZE - h->len);

            if (h->mms)
                size = mms_read (NULL, h->mms, (gchar *) h->buf + h->len, size);
            else /* if (h->mmsh) */
                size = mmsh_read (NULL, h->mmsh, (gchar *) h->buf + h->len, size);

            if (size < 0)
                fprintf (stderr, "mms: Read error: %s.\n", strerror (errno));
            if (size <= 0)
                break;

            h->len += size;
        }

        gint copy = MIN (h->len - h->used, goal - total);

        memcpy (buf, h->buf + h->used, copy);
        h->used += copy;
        buf += copy;
        total += copy;
    }

    return (size > 0) ? total / size : 0;
}

static gint64 mms_vfs_fwrite_impl (const void * data, gint64 size, gint64 count,
 VFSFile * file)
{
    fprintf (stderr, "mms: Writing is not supported.\n");
    return 0;
}

static gint mms_vfs_fseek_impl (VFSFile * file, gint64 offset, gint whence)
{
    MMSHandle * h = vfs_get_handle (file);

    if (whence == SEEK_SET)
    {
        whence = SEEK_CUR;
        offset -= h->offset + h->used;
    }

    if (whence != SEEK_CUR || offset < -h->used || offset > h->len - h->used)
    {
        fprintf (stderr, "mms: Attempt to seek outside buffered region.\n");
        return -1;
    }

    h->used += offset;
    return 0;
}

static void mms_vfs_rewind_impl (VFSFile * file)
{
    mms_vfs_fseek_impl (file, 0, SEEK_SET);
}

static gint64 mms_vfs_ftell_impl (VFSFile * file)
{
    MMSHandle * h = vfs_get_handle (file);
    return h->offset + h->used;
}

static gint mms_vfs_getc_impl (VFSFile * file)
{
    guchar c;
    return (mms_vfs_fread_impl (& c, 1, 1, file) == 1) ? c : EOF;
}

static gint mms_vfs_ungetc_impl (gint c, VFSFile * file)
{
    return (! mms_vfs_fseek_impl (file, -1, SEEK_CUR)) ? c : EOF;
}

static gboolean mms_vfs_feof_impl (VFSFile * file)
{
    return FALSE;

    MMSHandle * h = vfs_get_handle (file);

    if (h->mms)
        return (h->offset + h->used == mms_get_length (h->mms));
    else /* if (h->mmsh) */
        return (h->offset + h->used == mmsh_get_length (h->mmsh));
}

static gint mms_vfs_truncate_impl (VFSFile * file, gint64 size)
{
    fprintf (stderr, "mms: Truncating is not supported.\n");
    return -1;
}

static gint64 mms_vfs_fsize_impl (VFSFile * file)
{
    MMSHandle * h = vfs_get_handle (file);

    if (h->mms)
        return mms_get_length (h->mms);
    else /* if (h->mmsh) */
        return mmsh_get_length (h->mmsh);
}

static const gchar * const mms_schemes[] = {"mms", NULL};

static VFSConstructor constructor = {
 .vfs_fopen_impl = mms_vfs_fopen_impl,
 .vfs_fclose_impl = mms_vfs_fclose_impl,
 .vfs_fread_impl = mms_vfs_fread_impl,
 .vfs_fwrite_impl = mms_vfs_fwrite_impl,
 .vfs_getc_impl = mms_vfs_getc_impl,
 .vfs_ungetc_impl = mms_vfs_ungetc_impl,
 .vfs_fseek_impl = mms_vfs_fseek_impl,
 .vfs_rewind_impl = mms_vfs_rewind_impl,
 .vfs_ftell_impl = mms_vfs_ftell_impl,
 .vfs_feof_impl = mms_vfs_feof_impl,
 .vfs_ftruncate_impl = mms_vfs_truncate_impl,
 .vfs_fsize_impl = mms_vfs_fsize_impl
};

AUD_TRANSPORT_PLUGIN
(
 .name = "MMS Support",
 .schemes = mms_schemes,
 .vtable = & constructor
)
