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

#include <config.h>

#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libmtp.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/strpool.h>

#include <gtk/gtk.h>
#include "filetype.h"

#define DEBUG 1

GMutex * mutex = NULL;
gboolean mtp_initialised = FALSE;
LIBMTP_mtpdevice_t *mtp_device = NULL;
LIBMTP_progressfunc_t *callback;
LIBMTP_file_t *filelist;

static gboolean plugin_active = FALSE,exiting=FALSE;

static gboolean mtp_init (void);
void mtp_cleanup ( void );

AUD_GENERAL_PLUGIN
(
    .name = "MTP Upload",
    .init = mtp_init,
    .cleanup = mtp_cleanup
)

void show_dialog(const gchar* message)
{
    GDK_THREADS_ENTER();
    GtkWidget *dialog = gtk_message_dialog_new (NULL,
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "%s",
            message);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_show(dialog);
    /* gtk_widget_destroy(dialog); */
    GDK_THREADS_LEAVE();

}

static void free_device (void)
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
    if(!mutex)
        return;
    g_mutex_lock(mutex);
    if(mtp_device!= NULL)
    {
        LIBMTP_Release_Device(mtp_device);
        mtp_device = NULL;
        mtp_initialised = FALSE;
#if 0
        gtk_widget_hide(mtp_submenu_item_free);
#endif
    }
    g_mutex_unlock(mutex);
    return;
}

GList *
get_upload_list(void)
{
    GList *up_list=NULL;
    gint current_play = aud_playlist_get_active();
    gint i = (aud_playlist_entry_count(current_play) - 1);

    for (; i >= 0; i--)
    {
        if (aud_playlist_entry_get_selected(current_play, i))
        {
            Tuple * tuple = aud_playlist_entry_get_tuple (current_play, i, FALSE);
            aud_playlist_entry_set_selected(current_play, i, FALSE);
            up_list = g_list_prepend (up_list, (void *) tuple);

            if (tuple)
                tuple_unref (tuple);
        }
    }

    return g_list_reverse(up_list);
}

static char * strdup_tuple_filename (const Tuple * tuple)
{
    gchar * fpath = tuple_get_str (tuple, FIELD_FILE_PATH, NULL);
    gchar * fname = tuple_get_str (tuple, FIELD_FILE_NAME, NULL);

    gchar * filename = g_strdup_printf ("%s/%s", fpath, fname);

    str_unref (fpath);
    str_unref (fname);

    return filename;
}

static char * strdup_tuple_field (const Tuple * tuple, int field)
{
    char * sval, * dup;
    int ival;

    switch (tuple_get_value_type (tuple, field, NULL))
    {
    case TUPLE_INT:
        ival = tuple_get_int (tuple, field, NULL);
        dup = g_strdup_printf ("%d", ival);
        break;
    case TUPLE_STRING:
        sval = tuple_get_str (tuple, field, NULL);
        dup = g_strdup (sval);
        str_unref (sval);
        break;
    default:
        dup = NULL;
        break;
    }

    return dup;
}

LIBMTP_track_t *track_metadata(Tuple *from_tuple)
{
    LIBMTP_track_t *tr;
    gchar *filename, *uri_path;
    VFSFile *f;
    uint64_t filesize;
    struct stat sb;

    uri_path = strdup_tuple_filename (from_tuple);
    gchar *tmp = g_strescape(uri_path,NULL);
    filename=g_filename_from_uri(tmp,NULL,NULL);
    g_free(tmp);
    /* dealing the stream upload (invalidating)*/
    if(filename)
    {
        f = vfs_fopen(uri_path,"r");
        g_free(uri_path);
        if(vfs_is_streaming(f))
        {
            vfs_fclose(f);
            g_free(filename);
            return NULL;
        }
    }
    else
    {
        g_print("Warning! the filename is NULL, exiting");
        return NULL;

    }

    if ( stat(filename, &sb) == -1 )
    {
#if DEBUG
        g_print("ERROR! encountered while stat()'ing \"%s\"\n",filename);
#endif
        g_free(filename);
        return NULL;
    }
    filesize = (uint64_t) sb.st_size;

    /* track metadata*/
    tr = LIBMTP_new_track_t();
    tr->title = strdup_tuple_field (from_tuple, FIELD_TITLE);
    tr->artist = strdup_tuple_field (from_tuple, FIELD_ARTIST);
    tr->album = strdup_tuple_field (from_tuple, FIELD_ALBUM);
    tr->filesize = filesize;
    tr->filename = strdup_tuple_field (from_tuple, FIELD_FILE_NAME);
    tr->duration = (uint32_t)tuple_get_int(from_tuple, FIELD_LENGTH, NULL);
    tr->filetype = find_filetype (filename);
    tr->genre = strdup_tuple_field (from_tuple, FIELD_GENRE);
    tr->date = strdup_tuple_field (from_tuple, FIELD_YEAR);
    g_free(filename);
    return tr;
}

gint upload_file(Tuple *from_tuple)
{
    int ret;
    gchar *tmp, *from_path = NULL, *filename;
    LIBMTP_track_t *gentrack;
    gentrack = track_metadata(from_tuple);
    from_path = strdup_tuple_filename (from_tuple);
    if(gentrack == NULL) return 1;
    tmp = g_strescape(from_path,NULL);
    filename=g_filename_from_uri(tmp,NULL,NULL);

    g_free(from_path);
    g_free(tmp);

#if DEBUG
    g_print("Uploading track '%s'\n",filename);
#endif
    gentrack->parent_id = mtp_device->default_music_folder;
    ret = LIBMTP_Send_Track_From_File(mtp_device, filename , gentrack, NULL , NULL);
    LIBMTP_destroy_track_t(gentrack);
    if (ret == 0)
        g_print("Track upload finished!\n");
    else
    {
        g_print("An error has occured while uploading '%s'...\nUpload failed!!!\n\n",filename);
        mtp_initialised = FALSE;
        g_free(filename);
        return 1;
    }
    g_free(filename);
    return 0;
}


gpointer upload(gpointer arg)
{
#if 0
    gtk_widget_hide(mtp_submenu_item_free);
#endif
    if(!mutex)
    {
#if 0
        gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(mtp_submenu_item_up))),UP_DEFAULT_LABEL);
        gtk_widget_set_sensitive(mtp_submenu_item_up, TRUE);
#endif
        return NULL;
    }
    g_mutex_lock(mutex);
    if(!mtp_device)
    {
#if 0
        gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(mtp_submenu_item_up))),UP_DEFAULT_LABEL);
        gtk_widget_set_sensitive(mtp_submenu_item_up, TRUE);
#endif
        g_mutex_unlock(mutex);
        return NULL;
    }

    Tuple* tuple;
    GList *up_list=NULL,*node;
    node=up_list=get_upload_list();
    gint up_err=0;
    while(node)
    {
        tuple=(Tuple*)(node->data);
        up_err = upload_file(tuple);
        if(up_err )
        {
            /*show_dialog("An error has occured while uploading...\nUpload failed!");*/
            break;
        }
        if(exiting)
        {
            /*show_dialog("Shutting down MTP while uploading.\nPending uploads were cancelled");*/
            break;
        }

        node = g_list_next(node);
    }
    g_list_free(up_list);
#if 0
    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(mtp_submenu_item_up))),UP_DEFAULT_LABEL);
    gtk_widget_set_sensitive(mtp_submenu_item_up, TRUE);
#endif
    g_mutex_unlock(mutex);
#if DEBUG
    g_print("MTP upload process finished\n");
#endif
#if 0
    gtk_widget_show(mtp_submenu_item_free);
#endif
    g_thread_exit(NULL);
    return NULL;
}

static void mtp_press (void)
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
#if 0
        gtk_widget_show(mtp_submenu_item_free);
#endif
    }
    g_mutex_unlock(mutex);
    if(mtp_device == NULL)
    {
#if DEBUG
        g_print("No MTP devices have been found !!!\n");
#endif
        /* show_dialog("No MTP devices have been found !!!"); */
        mtp_initialised = FALSE;
        return;
    }
#if 0
    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(mtp_submenu_item_up))), _("Upload in progress..."));
    gtk_widget_set_sensitive(mtp_submenu_item_up, FALSE);
#endif
    g_thread_create(upload,NULL,FALSE,NULL);
}

static gboolean mtp_init (void)
{
    mutex = g_mutex_new();
    plugin_active = TRUE;
    exiting=FALSE;

    aud_plugin_menu_add (AUD_MENU_MAIN, mtp_press, _("Upload to MTP Device"), NULL);
    aud_plugin_menu_add (AUD_MENU_MAIN, free_device, _("Disconnect MTP Device"), NULL);

    return TRUE;
}

void mtp_cleanup(void)
{
    if (plugin_active)
    {
        aud_plugin_menu_remove (AUD_MENU_MAIN, mtp_press);
        aud_plugin_menu_remove (AUD_MENU_MAIN, free_device);

#if DEBUG
        if(mtp_initialised)
        {
            g_print("\n\n                 !!!CAUTION!!! \n\n"
                    "Cleaning up MTP upload plugin, please wait!!!...\n"
                    "This will block until the pending tracks are uploaded,\n"
                    "then it will gracefully close your device\n\n"
                    "!!! FORCING SHUTDOWN NOW MAY CAUSE DAMAGE TO YOUR DEVICE !!!\n\n\n"
                    "Waiting for the MTP mutex to unlock...\n");
            exiting=TRUE;
        }
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

        g_mutex_free (mutex);
        mutex = NULL;
        plugin_active = FALSE;
#if DEBUG
        if(mtp_initialised)
            g_print("MTP upload plugin has been cleaned up successfully\n");
#endif
    }
}

