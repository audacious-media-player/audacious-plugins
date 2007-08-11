/*
 * Audacious MTP upload plugin
 *
 * Copyright (c) 2007 Cristian Magherusan <majeru@atheme.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */

#include <glib.h>
#include <libmtp.h>
#include <audacious/plugin.h>
#include <audacious/playlist.h>
#include <audacious/ui_plugin_menu.h>
#define DEBUG 1

#define DEFAULT_LABEL "Upload to MTP device"
#define DISABLED_LABEL "MTP upload in progress..."


GMutex * mutex = NULL; 
gboolean mtp_initialised = FALSE;
LIBMTP_mtpdevice_t *mtp_device = NULL;
LIBMTP_progressfunc_t *callback;
LIBMTP_file_t *filelist;
Playlist *active_playlist;

static gboolean plugin_active = FALSE;

void mtp_init ( void );
void mtp_cleanup ( void );
void mtp_prefs ( void );
void mtp_about ( void );

GeneralPlugin mtp_gp =
{
    NULL,                                   /* handle */
    NULL,                                   /* filename */
    "MTP Upload " ,                         /* description */
    mtp_init,                               /* init */
    mtp_about,                              /* about */
    mtp_prefs,                              /* configure */
    mtp_cleanup                             /* cleanup */
};
GtkWidget *menuitem;

GeneralPlugin *mtp_gplist[] = { &mtp_gp, NULL };
DECLARE_PLUGIN(mtp_gp, NULL, NULL, NULL, NULL, NULL, mtp_gplist, NULL, NULL)

GList * get_upload_list()
{
    Tuple *tuple;
    gchar *from_path;
    GList *node=NULL,*up_list=NULL;
    PlaylistEntry *entry;
    Playlist *current_play = playlist_get_active();

    node = current_play->entries;
    PLAYLIST_LOCK(current_play->mutex);     /*needed so that the user doesn't modify the selection*/ 
    while (node)                            /*while creating the list of files to be uploaded*/
    {
        entry = PLAYLIST_ENTRY(node->data);
        if (entry->selected)  
        {
            tuple = entry->tuple;
            from_path = g_strdup_printf("%s/%s", tuple_get_string(tuple, "file-path"), tuple_get_string(tuple, "file-name"));
            VFSFile* f = vfs_fopen(from_path,"r");
            if(!vfs_is_streaming(f))
                up_list=g_list_prepend(up_list,from_path);
            vfs_fclose(f);
            entry->selected = FALSE;
        }
        node = g_list_next(node);
    }
    PLAYLIST_UNLOCK(current_play->mutex);
    return g_list_reverse(up_list);
}

void upload_file(gchar *from_path)
{
    int ret;
    gchar *comp, *filename;
    uint64_t filesize;
    uint32_t parent_id = 0;
    struct stat sb;
    LIBMTP_file_t *genfile;

    comp = g_strescape(from_path,NULL);
    if ( stat(from_path, &sb) == -1 )
    {
#if DEBUG
        g_print("ERROR! encountered while stat()'ing \"%s\"\n",from_path);
#endif
        return;
    }
    filesize = (uint64_t) sb.st_size;
    filename = g_path_get_basename(from_path);
    parent_id = 0;
    genfile = LIBMTP_new_file_t();
    genfile->filesize = filesize;
    genfile->filename = strdup(filename);
#if DEBUG
    g_print("Uploading track '%s'\n",comp);
#endif
    ret = LIBMTP_Send_File_From_File(mtp_device, comp , genfile, NULL , NULL, parent_id);
#if DEBUG
    if (ret == 0) 
        g_print("Upload finished!\n");
    else
        g_print("An error has occured while uploading '%s'...\nUpload failed!!!",comp);
#endif
    LIBMTP_destroy_file_t(genfile);
    g_free(filename);
    g_free(comp);
}


gpointer upload(gpointer arg)
{
    if(!mutex)
        return NULL;
    g_mutex_lock(mutex);
    if(!mtp_device)
        {
            g_mutex_unlock(mutex); 
            return NULL;
        }
    gchar* from_path;
    GList *up_list=NULL,*node;
    node=up_list=get_upload_list();
    while(node)
    {
        from_path=(gchar*)(node->data);
        upload_file(from_path);
        node = g_list_next(node);
    }
    g_list_free(up_list);
    g_mutex_unlock(mutex);

    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))),DEFAULT_LABEL);
    gtk_widget_set_sensitive(menuitem, TRUE);
  
    return NULL;
}

    void
mtp_prefs ( void )
{
    /*pref stub*/
}


    void
mtp_about ( void )
{
    /*about stub*/
}

void mtp_press()
{
    if(!mutex) 
        return;
    g_mutex_lock(mutex);
    if(!mtp_initialised)
    {
#if DEBUG
        g_print("Initializing the MTP device...\n");
#endif
        LIBMTP_Init();
        mtp_device = LIBMTP_Get_First_Device();
        mtp_initialised = TRUE;
    }
    g_mutex_unlock(mutex);
    if(mtp_device == NULL) 
    {
#if DEBUG
        g_print("No MTP devices have been found !!!");
#endif
        return;

    }
    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))),DISABLED_LABEL);
    gtk_widget_set_sensitive(menuitem, FALSE);
    g_thread_create(upload,NULL,FALSE,NULL);    
}

void mtp_init(void)
{
    menuitem=gtk_menu_item_new_with_label(DEFAULT_LABEL);
    gtk_widget_show (menuitem);
    audacious_menu_plugin_item_add(AUDACIOUS_MENU_PLAYLIST_RCLICK, menuitem);
    g_signal_connect (G_OBJECT (menuitem), "button_press_event",G_CALLBACK (mtp_press), NULL);  
    mutex = g_mutex_new();
    plugin_active = TRUE;
}

void mtp_cleanup(void)
{
    if (plugin_active)
    {

#if DEBUG
        if(mtp_initialised)
            g_print("\n\n                 !!!CAUTION!!! \n\n"
                    "Cleaning up MTP upload plugin, please wait!!!...\n"
                    "This will block until the pending tracks are uploaded,\n"
                    "then it will gracefully close your device\n\n"
                    "!!! FORCING SHUTDOWN NOW MAY CAUSE DAMAGE TO YOUR DEVICE !!!\n\n\n"
                    "Waiting for the MTP mutex to unlock...\n");
#endif
        if(mutex)
            g_mutex_lock(mutex);
        if(mtp_device!= NULL)
        {
            LIBMTP_Release_Device(mtp_device);
            mtp_device = NULL;
        }
        g_mutex_unlock(mutex);
#if DEBUG
        if(mtp_initialised)
            g_print("The MTP mutex has been unlocked\n");
#endif
        audacious_menu_plugin_item_remove(AUDACIOUS_MENU_PLAYLIST_RCLICK, menuitem);
        gtk_widget_destroy(menuitem);
        g_mutex_free (mutex);
        mutex = NULL;
        plugin_active = FALSE;
#if DEBUG
        if(mtp_initialised)
            g_print("MTP upload plugin has been cleaned up successfully\n");
#endif
    }
}

