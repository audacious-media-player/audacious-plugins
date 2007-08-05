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
#include <audacious/discovery.h>
/*
#include <audacious/configdb.h>
#include <libmowgli/mowgli.h>
#include <curl/curl.h>
*/
#include <glib.h>
#include "daap_mdns_browse.h"
#include "daap_cmd.h"

gboolean daap_initialized=FALSE;

GMutex * mutex_init = NULL; 

GMutex * mutex_discovery = NULL; 

GList * daap_servers = NULL;

guint request_id=0;

GList * daap_get_server_playlist(gchar * host, gint port  )
{
return NULL;

}


GList * daap_discovery_get_devices_impl(void)
{   
    discovery_device_t * current_device=NULL;
    GList * returned_devices=NULL;
    GSList * daap_found_devices=NULL,
          * current_server=NULL;

    if(mutex_discovery==NULL)
        return NULL;

    g_mutex_lock(mutex_discovery);

    daap_found_devices  = daap_mdns_get_server_list ();
    current_server=daap_found_devices;
    for (current_server = daap_found_devices; current_server; current_server = g_slist_next (current_server)) 
    {
        current_device = g_new0(discovery_device_t,1);
        daap_mdns_server_t *serv=current_server->data;
        current_device->device_name = 
            g_strdup_printf("%s(%s)",
                    serv->server_name,
                    serv->mdns_hostname
                    );

        current_device->device_address = 
            g_strdup_printf(
                    "%s:%d",
                    serv->address,
                    serv->port
                    );
        current_device->device_playlist=
            daap_get_server_playlist(
                    serv->mdns_hostname,
                    serv->port
                    );
        returned_devices = g_list_prepend(returned_devices,current_device); 
#if DEBUG
        g_print("DAAP: Found device %s at address %s\n", current_device->device_name ,current_device->device_address );
#endif
    }
    g_slist_free(daap_found_devices);
    g_mutex_unlock(mutex_discovery);
    return g_list_reverse(returned_devices);
}




VFSFile * daap_vfs_fopen_impl(const gchar * path, const gchar * mode)
{
    VFSFile *file=NULL;
    if(!mutex_init)
        return NULL;

    g_mutex_lock(mutex_init); /* locking for init */
    if(!daap_initialized)
    {
        if( !daap_mdns_initialize ())
        {
#ifdef DEBUG    /*this isn't a fatal error, we can try again later*/
            g_print("Error while initializing DAAP !!!\n");
#endif          
            g_mutex_unlock(mutex_init);
            return NULL;
        }
        else
            daap_initialized=TRUE;

    }
    if(daap_initialized)
        daap_discovery_get_devices_impl();
    g_mutex_unlock(mutex_init);  /*init ended*/

    file = g_new0(VFSFile, 1);
//    GList * l = 

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
    mutex_init = g_mutex_new();        
    mutex_discovery = g_mutex_new();        
    vfs_register_transport(&daap_const);
}
static void cleanup(void)
{
    g_mutex_free (mutex_init);
    g_mutex_free (mutex_discovery);
    daap_mdns_destroy ();
}
DECLARE_PLUGIN(daap, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL)
