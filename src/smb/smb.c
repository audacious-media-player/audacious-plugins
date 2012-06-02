/*  Audacious
 *  Copyright (c) 2007 Daniel Bradshaw
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

#include "config.h"
#include <audacious/plugin.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libsmbclient.h>

static void smb_auth_fn(const char *srv,
                        const char *shr,
                        char *wg, int wglen,
                        char *un, int unlen,
                        char *pw, int pwlen)
{
  /* Does absolutely nothing :P */
}

typedef struct _SMBFile {
	int fd;
	gint64 length;
} SMBFile;

/* TODO: make writing work. */
void * smb_vfs_fopen_impl (const gchar * path, const gchar * mode)
{
  SMBFile *handle;
  struct stat st;

  if (!path || !mode)
    return NULL;

  handle = g_new0(SMBFile, 1);
  handle->fd = smbc_open(path, O_RDONLY, 0);

  if (handle->fd < 0)
  {
    g_free (handle);
    return NULL;
  }

  smbc_stat (path, & st);
  handle->length = st.st_size;

  return handle;
}

gint smb_vfs_fclose_impl(VFSFile * file)
{
  gint ret = 0;
  SMBFile * handle = vfs_get_handle (file);

  if (smbc_close (handle->fd))
    ret = -1;

  g_free (handle);
  return ret;
}

static gint64 smb_vfs_fread_impl (void * ptr, gint64 size, gint64 nmemb, VFSFile * file)
{
  SMBFile * handle = vfs_get_handle (file);
  return smbc_read(handle->fd, ptr, size * nmemb);
}

static gint64 smb_vfs_fwrite_impl (const void * ptr, gint64 size, gint64 nmemb, VFSFile * file)
{
  return 0;
}

gint smb_vfs_getc_impl(VFSFile *file)
{
  SMBFile * handle = vfs_get_handle (file);
  char temp;

  smbc_read(handle->fd, &temp, 1);
  return (gint) temp;
}

static gint smb_vfs_fseek_impl(VFSFile * file, gint64 offset, gint whence)
{
  SMBFile * handle = vfs_get_handle (file);
  gint64 roffset = offset;
  gint ret = 0;

  if (whence == SEEK_END)
  {
    roffset = handle->length + offset;
    if (roffset < 0)
      roffset = 0;

    ret = smbc_lseek(handle->fd, roffset, SEEK_SET);
    //printf("%ld -> %ld = %d\n",offset, roffset, ret);
  }
  else
  {
    if (roffset < 0)
      roffset = 0;
    ret = smbc_lseek(handle->fd, roffset, whence);
    //printf("%ld %d = %d\n",roffset, whence, ret);
  }

  return ret;
}

gint smb_vfs_ungetc_impl(gint c, VFSFile *file)
{
  smb_vfs_fseek_impl(file, -1, SEEK_CUR);
  return c;
}

void smb_vfs_rewind_impl(VFSFile * file)
{
  smb_vfs_fseek_impl(file, 0, SEEK_SET);
}

static gint64 smb_vfs_ftell_impl(VFSFile * file)
{
  SMBFile * handle = vfs_get_handle (file);
  return smbc_lseek(handle->fd, 0, SEEK_CUR);
}

gboolean
smb_vfs_feof_impl(VFSFile * file)
{
  SMBFile * handle = vfs_get_handle (file);
  gint64 at;

  at = smb_vfs_ftell_impl(file);

  return (gboolean) (at == handle->length) ? TRUE : FALSE;
}

static gint smb_vfs_truncate_impl (VFSFile * file, gint64 size)
{
  return -1;
}

static gint64 smb_vfs_fsize_impl (VFSFile * file)
{
    SMBFile * handle = vfs_get_handle (file);
    return handle->length;
}

VFSConstructor smb_const = {
	smb_vfs_fopen_impl,
	smb_vfs_fclose_impl,
	smb_vfs_fread_impl,
	smb_vfs_fwrite_impl,
	smb_vfs_getc_impl,
	smb_vfs_ungetc_impl,
	smb_vfs_fseek_impl,
	smb_vfs_rewind_impl,
	smb_vfs_ftell_impl,
	smb_vfs_feof_impl,
	smb_vfs_truncate_impl,
	smb_vfs_fsize_impl
};

static gboolean init (void)
{
	int err;

	err = smbc_init(smb_auth_fn, 1);
	if (err < 0)
	{
		g_message("[smb] not starting samba support due to error code %d", err);
		return FALSE;
	}

	return TRUE;
}

static const gchar * const smb_schemes[] = {"smb", NULL};

AUD_TRANSPORT_PLUGIN
(
 .name = N_("SMB Plugin"),
 .domain = PACKAGE,
 .init = init,
 .schemes = smb_schemes,
 .vtable = & smb_const
)
