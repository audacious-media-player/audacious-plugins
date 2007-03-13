/*  Audacious
 *  Copyright (c) 2007 Daniel Bradshaw
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

#include <audacious/vfs.h>
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
}

typedef struct smb_file smb_file;

struct smb_file
{
	int fd;
	long length;
};

VFSFile *
smb_vfs_fopen_impl(const gchar * path,
          const gchar * mode)
{
    VFSFile *file;
		smb_file *handle;
		struct stat st;

    if (!path || !mode)
	return NULL;

		if (smbc_init(smb_auth_fn, 1) != 0)
			return NULL;
		 
    file = g_new0(VFSFile, 1);
		handle = g_new0(smb_file, 1);
		handle->fd = smbc_open(path, O_RDONLY, 0);
		
		if (file->handle < 0) {
      g_free(file);
      file = NULL;
    } else {
      smbc_stat(path,&st);
		  handle->length = st.st_size;
      file->handle = (void *)handle;
		}
		
		return file;
}

gint
smb_vfs_fclose_impl(VFSFile * file)
{
    gint ret = 0;
    smb_file *handle;

    if (file == NULL)
        return -1;
		
    if (file->handle)
    {
			handle = (smb_file *)file->handle;
      if (smbc_close(handle->fd) != 0)
        ret = -1;
			g_free(file->handle);
    }

    return ret;
}

size_t
smb_vfs_fread_impl(gpointer ptr,
          size_t size,
          size_t nmemb,
          VFSFile * file)
{
    smb_file *handle;
    if (file == NULL)
        return 0;
		handle = (smb_file *)file->handle;
    return smbc_read(handle->fd, ptr, size * nmemb);
}

size_t
smb_vfs_fwrite_impl(gconstpointer ptr,
           size_t size,
           size_t nmemb,
           VFSFile * file)
{
  return 0;
}

gint
smb_vfs_getc_impl(VFSFile *file)
{
	smb_file *handle;
	char temp[2];
	handle = (smb_file *)file->handle;
	smbc_read(handle->fd, &temp, 1);
  return (gint) temp[0];
}

gint
smb_vfs_fseek_impl(VFSFile * file,
          glong offset,
          gint whence)
{
	smb_file *handle;
	glong roffset = offset;
	gint ret = 0;
  if (file == NULL)
      return 0;
	
	handle = (smb_file *)file->handle;
	
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
		if (ret == 0) ret = handle->length;
	}
	
	return ret;
}

gint
smb_vfs_ungetc_impl(gint c, VFSFile *file)
{
	smb_vfs_fseek_impl(file, -1, SEEK_CUR);
  return c;
}

void
smb_vfs_rewind_impl(VFSFile * file)
{
	smb_vfs_fseek_impl(file, 0, SEEK_SET);
}

glong
smb_vfs_ftell_impl(VFSFile * file)
{
	smb_file *handle;
	handle = (smb_file *)file->handle;
	return smbc_lseek(handle->fd, 0, SEEK_CUR);
}

gboolean
smb_vfs_feof_impl(VFSFile * file)
{
  smb_file *handle;
  off_t at;
	handle = (smb_file *)file->handle;
	at = smbc_lseek(handle->fd, 0, SEEK_CUR);
	//printf("%d %d %ld %ld\n",sizeof(int), sizeof(off_t), at, handle->length);
  return (gboolean) (at == handle->length) ? TRUE : FALSE;
}

gint
smb_vfs_truncate_impl(VFSFile * file, glong size)
{
  return -1;
}

VFSConstructor smb_const = {
	"smb://",
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
	smb_vfs_truncate_impl
};

static void init(void)
{
	vfs_register_transport(&smb_const);
}

static void cleanup(void)
{
#if 0
	vfs_unregister_transport(&smb_const);
#endif
}

LowlevelPlugin llp_smb = {
	NULL,
	NULL,
	"smb:// URI Transport",
	init,
	cleanup,
};

LowlevelPlugin *get_lplugin_info(void)
{
        return &llp_smb;
}

