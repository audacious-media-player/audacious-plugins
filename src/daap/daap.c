/*  Audacious DAAP transport plugin
 *  Copyright (c) 2007 Cristi Magherusan <majeru@gentoo.ro>
 *      
 *  With inspiration and code from David Hammerton's tunesbrowser
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
 *  along with this program; if not, write to the Free Softmcware
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <audacious/vfs.h>
#include <audacious/plugin.h>
/*
#include <audacious/configdb.h>
#include <libmowgli/mowgli.h>
#include <curl/curl.h>
*/
#include <glib.h>
#include <daap/client.h>






DAAP_SClientHost *libopendaap_host;


VFSFile * daap_vfs_fopen_impl(const gchar * path, const gchar * mode)
{
    VFSFile *file;
    file = g_new0(VFSFile, 1);
    /* connectiong daap*/


    return file;
}

gint daap_vfs_fclose_impl(VFSFile * file)
{
    return 0;
}
size_t daap_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
    return 0;
}

size_t daap_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
    return -1;
}

gint daap_vfs_getc_impl(VFSFile * stream)
{
    return 0;
}

gint daap_vfs_ungetc_impl(gint c, VFSFile * stream)
{
    return 0;
}
gint daap_vfs_fseek_impl(VFSFile * file, glong offset, gint whence)
{
    return -1;
}

void daap_vfs_rewind_impl(VFSFile * file)
{
    return;
}

glong daap_vfs_ftell_impl(VFSFile * file)
{
    return 0;
}

gboolean daap_vfs_feof_impl(VFSFile * file)
{
    return 1;
}

gint daap_vfs_truncate_impl(VFSFile * file, glong size)
{
    return -1;
}
off_t daap_vfs_fsize_impl(VFSFile * file)
{
    return 0;
}
gchar *daap_vfs_metadata_impl(VFSFile * file, const gchar * field)
{
    return NULL;
}

VFSConstructor daap_const = {
    "daap://",
    daap_vfs_fopen_impl,
    daap_vfs_fclose_impl,
    daap_vfs_fread_impl,
    daap_vfs_fwrite_impl,
    daap_vfs_getc_impl,
    daap_vfs_ungetc_impl,
    daap_vfs_fseek_impl,
    daap_vfs_rewind_impl,
    daap_vfs_ftell_impl,
    daap_vfs_feof_impl,
    daap_vfs_truncate_impl,
    daap_vfs_fsize_impl,
    daap_vfs_metadata_impl
};

static void init(void)
{       
    vfs_register_transport(&daap_const);
}
static void cleanup(void)
{
}
DECLARE_PLUGIN(daap, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL)
