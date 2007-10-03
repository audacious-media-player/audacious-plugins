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

#include <glib.h>

#include <audacious/plugin.h>
#include <audacious/discovery.h>

#include "xmms2-daap/daap_cmd.h"
#include "xmms2-daap/daap_util.h"
#include "xmms2-daap/daap_mdns_browse.h"
#define DEFAULT_DAAP_PORT 3689
#define DEBUG 1
typedef struct {
    gchar *host;
    guint port;

    GIOChannel *channel;

} daap_data_t;

typedef struct {
    gboolean logged_in;
    guint session_id;
    guint revision_id;
    guint request_id;
} daap_login_data_t;

typedef struct{
    daap_data_t *daap_data;
    gchar *url;
    int fd;
    guint pos;
    gulong length; 
}
daap_handle_t;



static GHashTable *login_sessions = NULL;

gboolean daap_initialized=FALSE;

GMutex * mutex_init = NULL; 

GMutex * mutex_discovery = NULL; 

GList * daap_servers = NULL;

guint request_id=0;



    static gboolean
get_data_from_url (const gchar *url, gchar **host, guint *port, gchar **cmd)
{
    const gchar *port_ptr, *cmd_ptr, *end_ptr, *stripped;

    stripped = url + sizeof (gchar) * strlen ("daap://");

    end_ptr = stripped + sizeof (gchar) * strlen (stripped);

    if (stripped == end_ptr) {
        g_print("DAAP: Empty URL\n");
        return FALSE;
    }

    port_ptr = strstr (stripped, ":");
    if (port && port_ptr && (port_ptr + 1) != end_ptr) {
        *port = strtol (port_ptr + 1, (gchar **) NULL, 10);
        if (*port == 0) {
            *port = DEFAULT_DAAP_PORT;
        }
    } else if (port) {
        *port = DEFAULT_DAAP_PORT;
    }

    cmd_ptr = strstr (stripped, "/");
    if (cmd && cmd_ptr && (cmd_ptr + 1) != end_ptr) {
        *cmd = g_strdup (cmd_ptr);
    } else if (cmd) {
        /* cmd wanted but not found */
        g_print("DAAP: No file requested\n");
    } else if (!cmd && cmd_ptr && (cmd_ptr + 1) != end_ptr) {
        /* cmd not wanted but found */
        g_print("DAAP: No such directory\n");
        return FALSE;
    }

    if (port_ptr) {
        *host = g_strndup (stripped, port_ptr - stripped);
    } else if (cmd_ptr) {
        *host = g_strndup (stripped, cmd_ptr - stripped);
    } else {
        *host = g_strdup (stripped);
    }

    return TRUE;
}



    static gboolean
daap_init (daap_handle_t *handle)
{
    gint dbid;
    GSList *dbid_list = NULL;
    daap_data_t *data;
    daap_login_data_t *login_data;
    guint filesize;
    gchar *command=NULL;
    gchar *hash;

    data = g_new0 (daap_data_t, 1);

    if (!get_data_from_url (handle->url, &(data->host), &(data->port), &command))
        return FALSE;

    hash = g_strdup_printf ("%s:%u", data->host, data->port);

    login_data = g_hash_table_lookup (login_sessions, hash);
    if (!login_data) 
    {
#if DEBUG
        g_print ("creating login data for %s\n", hash);
#endif
        login_data = g_new0 (daap_login_data_t, 1);

        login_data->request_id = 1;
        login_data->logged_in = TRUE;

        login_data->session_id = daap_command_login (data->host, data->port,
                login_data->request_id);
        if(login_data->session_id==0)
            return FALSE;

        g_hash_table_insert (login_sessions, hash, login_data);
    }

    login_data->revision_id = daap_command_update (data->host, data->port,
            login_data->session_id,
            login_data->request_id);
    dbid_list = daap_command_db_list (data->host, data->port,
            login_data->session_id,
            login_data->revision_id,
            login_data->request_id);
    if (!dbid_list) {
        return FALSE;
    }

    /* XXX: see XXX in the browse function above */
    dbid = ((cc_item_record_t *) dbid_list->data)->dbid;
    /* want to request a stream, but don't read the data yet */
    data->channel = daap_command_init_stream (data->host, data->port,
            login_data->session_id,
            login_data->revision_id,
            login_data->request_id, dbid,
            command,&filesize);
    if (!data->channel) {
        return FALSE;
    }

    login_data->request_id++;
    
    handle->daap_data=data;
    handle->length=filesize;
    handle->pos=0;
    g_slist_foreach (dbid_list, (GFunc) cc_item_record_free, NULL);
    g_slist_free (dbid_list);
    g_free (command);

    return TRUE;
}







    GList*
daap_get_tracks_from_server (gchar *host, guint port)
{
    GList * output=NULL;
    GSList *dbid_list = NULL;
    GSList *song_list = NULL, *song_el;
    cc_item_record_t *db_data;
    daap_login_data_t *login_data;
    gchar *hash;

    hash = g_strdup_printf ("%s:%u", host, port);

    login_data = g_hash_table_lookup (login_sessions, hash);

    if (!login_data) {
        login_data = g_new0 (daap_login_data_t, 1);

        login_data->session_id = daap_command_login (host, port, 0);

        login_data->revision_id = daap_command_update (host, port,
                login_data->session_id,
                0);

        login_data->request_id = 1;
        login_data->logged_in = TRUE;

        g_hash_table_insert (login_sessions, hash, login_data);
    } else {
        login_data->revision_id = daap_command_update (host, port,
                login_data->session_id,
                0);
    }

    dbid_list = daap_command_db_list (host, port, login_data->session_id,
            login_data->revision_id, 0);
    if (!dbid_list) {
        return NULL;
    }

    /* XXX i've never seen more than one db per server out in the wild,
     *     let's hope that never changes *wink*
     *     just use the first db in the list */
    db_data = (cc_item_record_t *) dbid_list->data;
    song_list = daap_command_song_list (host, port, login_data->session_id,
            login_data->revision_id,
            0, db_data->dbid);

    g_slist_foreach (dbid_list, (GFunc) cc_item_record_free, NULL);
    g_slist_free (dbid_list);

    if (!song_list) {
        return NULL;
    }

    for (song_el = song_list; song_el; song_el = g_slist_next (song_el)) 
        output=g_list_prepend(output, g_memdup(song_el->data,sizeof(cc_item_record_t)));


    g_slist_foreach (song_list, (GFunc) cc_item_record_free, NULL);
    g_slist_free (song_list);

    return g_list_reverse(output);
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
    g_print ("caut\n");
    daap_found_devices  = daap_mdns_get_server_list ();
    current_server=daap_found_devices;

    for (; current_server; current_server = g_slist_next (current_server)) 
    {
        current_device = g_new0(discovery_device_t,1);
        daap_mdns_server_t *serv=current_server->data;
        current_device->device_name = 
            g_strdup_printf("%s(%s)",serv->server_name, serv->mdns_hostname);

        current_device->device_address = 
            g_strdup_printf(
                    "%s:%d",
                    serv->address,
                    serv->port
                    );
        current_device->device_playlist=
            daap_get_tracks_from_server(
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
    daap_handle_t *handle = g_new0(daap_handle_t, 1);
    handle->url=g_strdup(path);

    if (!path || !mode)
        return NULL;

    if( !daap_init(handle) )
    {
#if DEBUG    /*this isn't a fatal error, we can try again later*/
        g_print("Error while initializing DAAP !!!\n");
#endif          
        return NULL;
    }
#if DEBUG   
    g_print("DAAP was initialized succesfully\n");
#endif  
    file=g_new0(VFSFile, 1);
    handle->fd=g_io_channel_unix_get_fd(handle->daap_data->channel);

    if (handle->fd < 0) 
    {
        g_print("vfs_fopen got a negative FD \n");
        g_free(file);
        file = NULL;
    } 
    else 
        file->handle=(void*)handle;
    return file;
}

gint daap_vfs_fclose_impl(VFSFile * file)
{
    gint ret=0;
    daap_handle_t * handle = (daap_handle_t *)file->handle;
    if (file == NULL)
        return -1;

    if (file->handle)
    {
        if(g_io_channel_shutdown(handle->daap_data->channel,TRUE,NULL)!=G_IO_STATUS_NORMAL) 
            ret = -1;
        else
            {
                g_io_channel_unref(handle->daap_data->channel);
                ret=0;
            }
        g_free(file->handle);
        file->handle=NULL;
    }
    return ret;

return -1;
}

size_t daap_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
    daap_handle_t *handle= (daap_handle_t *)file->handle;
    size_t ret=0;
if (file == NULL)
        return 0;
if( g_io_channel_read_chars (handle->daap_data->channel,ptr,size*nmemb,&ret,NULL)==G_IO_STATUS_NORMAL)
    {
        handle->pos+=(size*nmemb);
        return ret;
    }
else 
    return 0;
}

size_t daap_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
    return -1;
}

gint daap_vfs_getc_impl(VFSFile * file)
{
guchar ret=EOF;
daap_handle_t *handle = (daap_handle_t *)file->handle;
int status=g_io_channel_read_chars (handle->daap_data->channel,(void*)&ret,1,NULL,NULL);
if(status==G_IO_STATUS_NORMAL)
    {
    g_print ("fgetc OK\n");
    handle->pos++;
    return ret;
    }
else 
{
g_print ("fgetc failed\n");
    return EOF;
}
}

gint daap_vfs_fseek_impl(VFSFile * file, glong offset, gint whence)
{
    return -1;
}

gint daap_vfs_ungetc_impl(gint c, VFSFile * stream)
{
return c;
}

void daap_vfs_rewind_impl(VFSFile * stream)
{
    return;
}

glong daap_vfs_ftell_impl(VFSFile * stream)
{
    daap_handle_t *handle=stream->handle;
   return handle->pos;
}

gboolean daap_vfs_feof_impl(VFSFile * file)
{
   daap_handle_t *handle=file->handle;
  off_t at = daap_vfs_ftell_impl(file);
  return (gboolean) (at >= handle->length) ? TRUE : FALSE;
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
daap_handle_t *handle;
g_print("Requested metadata: '%s' \n",field);

if(file && file->handle)
    handle = (daap_handle_t *)file->handle;
else 
    return NULL;

/*if (!g_ascii_strncasecmp(field, "stream-name", 11))
    return handle->url;
if (!g_ascii_strncasecmp(field, "track-name",10))
    return handle->url;
*/

if (!g_ascii_strncasecmp(field, "content-type", 12))
    return g_strdup("audio/mpeg");
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
    daap_mdns_initialize ();
    if (!login_sessions) 
        login_sessions = g_hash_table_new (g_str_hash, g_str_equal);


}
static void cleanup(void)
{
    g_mutex_free (mutex_init);
    g_mutex_free (mutex_discovery);
    daap_mdns_destroy ();
}
DECLARE_PLUGIN(daap, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL)
