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

GMutex * mutex = NULL; 
gboolean mtp_initialised = FALSE;
LIBMTP_mtpdevice_t *mtp_device = NULL;
LIBMTP_progressfunc_t *callback;
LIBMTP_file_t *filelist;
Playlist *active_playlist;

void mtp_init ( void );
void mtp_cleanup ( void );
void mtp_prefs ( void );
void mtp_about ( void );

GeneralPlugin mtp_gp =
{
    NULL,					/* handle */
    NULL,					/* filename */
    "MTP Upload " ,			/* description */
    mtp_init,				/* init */
    mtp_about,				/* about */
    mtp_prefs,				/* configure */
    mtp_cleanup				/* cleanup */
};
GtkWidget *menuitem;

GeneralPlugin *mtp_gplist[] = { &mtp_gp, NULL };
DECLARE_PLUGIN(mtp_gp, NULL, NULL, NULL, NULL, NULL, mtp_gplist, NULL, NULL)


gpointer upload(gpointer arg)
{
    Playlist *current_play;
    Tuple *tuple;
    gchar *from_path;
    gchar *comp;
    char *filename;
    uint64_t filesize;
    struct stat sb;
    LIBMTP_file_t *genfile;
    int ret;
    uint32_t parent_id = 0;
    GList *node;
    PlaylistEntry *entry;
    current_play = g_new0(Playlist,1);
    current_play = playlist_get_active();
    node = current_play->entries;
    PLAYLIST_LOCK(current_play->mutex); /*needed so that the user doesn't modify the selection*/
    while (node) {
        entry = PLAYLIST_ENTRY(node->data);
        if (entry->selected)  
        {
            tuple = entry->tuple;
            from_path = g_strdup_printf("%s/%s", tuple_get_string(tuple, "file-path"), tuple_get_string(tuple, "file-name"));
            comp = g_strescape(from_path,NULL);
            if ( stat(from_path, &sb) == -1 )
            {
#if DEBUG
                g_print("ERROR!");
#endif
                return NULL;
            }
            filesize = (uint64_t) sb.st_size;
            filename = basename(from_path);
            parent_id = 0;
            genfile = LIBMTP_new_file_t();
            genfile->filesize = filesize;
            genfile->filename = strdup(filename);
#if DEBUG
            g_print("Uploading track '%s'\n",comp);
#endif
            ret = LIBMTP_Send_File_From_File(mtp_device,comp , genfile, NULL , NULL, parent_id);
#if DEBUG
            if (ret == 0) 
                g_print("Upload finished!\n");
            else
                g_print("An error has occured while uploading '%s'...\nUpload failed!!!",comp);
#endif
            LIBMTP_destroy_file_t(genfile);
            g_free(from_path);
            g_free(comp);
            entry->selected = FALSE;
        }
        node = g_list_next(node);
    }
    PLAYLIST_UNLOCK(current_play->mutex);
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
    g_thread_create(upload,NULL,FALSE,NULL);	
}

void mtp_init(void)
{
    menuitem=gtk_menu_item_new_with_label("Upload to MTP");
    gtk_widget_show (menuitem);
    audacious_menu_plugin_item_add(AUDACIOUS_MENU_PLAYLIST_RCLICK, menuitem);
    g_signal_connect (G_OBJECT (menuitem), "button_press_event",G_CALLBACK (mtp_press), NULL);  
    LIBMTP_Init();
    mutex = g_mutex_new();
}

void mtp_cleanup(void)
{
#if DEBUG
    g_print("Cleaning up MTP_upload\n");
#endif
    audacious_menu_plugin_item_remove(AUDACIOUS_MENU_PLAYLIST, menuitem );
    g_mutex_free (mutex);
    mutex = NULL;
}

