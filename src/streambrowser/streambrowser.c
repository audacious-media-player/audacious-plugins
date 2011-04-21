/*
 * Audacious Streambrowser Plugin
 *
 * Copyright (c) 2008 Calin Crisan <ccrisan@gmail.com>
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


#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib.h>

#include <audacious/configdb.h>
#include <audacious/debug.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "streambrowser.h"
#include "streamdir.h"
#include "shoutcast.h"
#include "xiph.h"
#include "bookmarks.h"
#include "gui/streambrowser_win.h"


typedef struct
{
    bookmark_t *bookmarks;
    int bookmarks_count;
}
streambrowser_cfg_t;

typedef struct
{
    streamdir_t *streamdir;
    category_t *category;
    streaminfo_t *streaminfo;
    gboolean add_to_playlist;
}
update_thread_data_t;


static gboolean sb_init (void);
static void sb_about ();
static void sb_configure ();
static void sb_cleanup ();

static void gui_init ();
static void gui_done ();

static void streamdir_update (streamdir_t * streamdir, category_t * category,
                              streaminfo_t * streaminfo,
                              gboolean add_to_playlist);
static gpointer update_thread_core (gpointer user_data);
static void streaminfo_add_to_playlist (streaminfo_t * streaminfo);

static GQueue *update_thread_data_queue = NULL;
static GMutex *update_thread_mutex = NULL;

streambrowser_cfg_t streambrowser_cfg;

GtkWidget *sb_gui_widget = NULL;

static gpointer sb_widget ()
{
	return sb_gui_widget;
}

AUD_GENERAL_PLUGIN
(
    .name = "Stream Browser",
    .init = sb_init,
    .about = sb_about,
    .configure = sb_configure,
    .cleanup = sb_cleanup,
    .get_widget = sb_widget,
)

void failure (const char *fmt, ...)
{
    va_list ap;
    fprintf (stderr, "! streambrowser: ");
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
}

gboolean fetch_remote_to_local_file (gchar * remote_url, gchar * local_url)
{
    VFSFile *remote_file = vfs_fopen (remote_url, "r");
    if (remote_file == NULL)
    {
        failure ("failed to fetch file '%s'\n", remote_url);
        return FALSE;
    }

    VFSFile *local_file = vfs_fopen (local_url, "w");
    if (local_file == NULL)
    {
        vfs_fclose (remote_file);

        failure ("failed to create local file '%s'\n", local_file);
        return FALSE;
    }

    unsigned char buff[DEF_BUFFER_SIZE];
    int size;
    while (!vfs_feof (remote_file))
    {
        size = vfs_fread (buff, 1, DEF_BUFFER_SIZE, remote_file);

        // i don't know why vfs_feof() doesn't ever return TRUE
        // so this is a workaround to properly end the loop
        if (size == 0)
            break;

        size = vfs_fwrite (buff, 1, size, local_file);
        if (size == 0)
        {
            vfs_fclose (local_file);
            vfs_fclose (remote_file);

            failure ("failed to write to local file '%s'\n", local_file);
            return FALSE;
        }
    }

    vfs_fclose (local_file);
    vfs_fclose (remote_file);

    return TRUE;
}

void config_load ()
{
    streambrowser_cfg.bookmarks = NULL;
    streambrowser_cfg.bookmarks_count = 0;

    mcs_handle_t *db;
    if ((db = aud_cfg_db_open ()) == NULL)
    {
        failure ("failed to load configuration\n");
        return;
    }

    aud_cfg_db_get_int (db, "streambrowser", "bookmarks_count",
                        &streambrowser_cfg.bookmarks_count);

    if (streambrowser_cfg.bookmarks_count == 0)
        streambrowser_cfg.bookmarks = NULL;
    else
        streambrowser_cfg.bookmarks =
            g_malloc (sizeof (bookmark_t) * streambrowser_cfg.bookmarks_count);

    int i;
    gchar item[DEF_STRING_LEN];
    gchar *value;
    for (i = 0; i < streambrowser_cfg.bookmarks_count; i++)
    {
        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_streamdir_name", i);
        if (aud_cfg_db_get_string (db, "streambrowser", item, &value))
        {
            strncpy (streambrowser_cfg.bookmarks[i].streamdir_name, value,
                     DEF_STRING_LEN);
            g_free (value);
        }
        else
            streambrowser_cfg.bookmarks[i].streamdir_name[0] = '\0';

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_name", i);
        if (aud_cfg_db_get_string (db, "streambrowser", item, &value))
        {
            strncpy (streambrowser_cfg.bookmarks[i].name, value,
                     DEF_STRING_LEN);
            g_free (value);
        }
        else
            streambrowser_cfg.bookmarks[i].name[0] = '\0';

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_playlist_url", i);
        if (aud_cfg_db_get_string (db, "streambrowser", item, &value))
        {
            strncpy (streambrowser_cfg.bookmarks[i].playlist_url, value,
                     DEF_STRING_LEN);
            g_free (value);
        }
        else
            streambrowser_cfg.bookmarks[i].playlist_url[0] = '\0';

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_url", i);
        if (aud_cfg_db_get_string (db, "streambrowser", item, &value))
        {
            strncpy (streambrowser_cfg.bookmarks[i].url, value, DEF_STRING_LEN);
            g_free (value);
        }
        else
            streambrowser_cfg.bookmarks[i].url[0] = '\0';

        AUDDBG
            ("loaded a bookmark with streamdir_name = '%s', name = '%s', playlist_url = '%s', url = '%s'\n",
             streambrowser_cfg.bookmarks[i].streamdir_name,
             streambrowser_cfg.bookmarks[i].name,
             streambrowser_cfg.bookmarks[i].playlist_url,
             streambrowser_cfg.bookmarks[i].url);
    }

    AUDDBG("loaded %d bookmarks\n", streambrowser_cfg.bookmarks_count);

    aud_cfg_db_close (db);

    AUDDBG("configuration loaded\n");
}

void config_save ()
{
    mcs_handle_t *db;
    if ((db = aud_cfg_db_open ()) == NULL)
    {
        failure ("failed to save configuration\n");
        return;
    }

    int old_bookmarks_count = 0, i;
    gchar item[DEF_STRING_LEN];
    aud_cfg_db_get_int (db, "streambrowser", "bookmarks_count",
                        &old_bookmarks_count);
    aud_cfg_db_set_int (db, "streambrowser", "bookmarks_count",
                        streambrowser_cfg.bookmarks_count);

    for (i = 0; i < streambrowser_cfg.bookmarks_count; i++)
    {
        AUDDBG
            ("saving bookmark with streamdir_name = '%s', name = '%s', playlist_url = '%s', url = '%s'\n",
             streambrowser_cfg.bookmarks[i].streamdir_name,
             streambrowser_cfg.bookmarks[i].name,
             streambrowser_cfg.bookmarks[i].playlist_url,
             streambrowser_cfg.bookmarks[i].url);

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_streamdir_name", i);
        aud_cfg_db_set_string (db, "streambrowser", item,
                               streambrowser_cfg.bookmarks[i].streamdir_name);

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_name", i);
        aud_cfg_db_set_string (db, "streambrowser", item,
                               streambrowser_cfg.bookmarks[i].name);

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_playlist_url", i);
        aud_cfg_db_set_string (db, "streambrowser", item,
                               streambrowser_cfg.bookmarks[i].playlist_url);

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_url", i);
        aud_cfg_db_set_string (db, "streambrowser", item,
                               streambrowser_cfg.bookmarks[i].url);
    }

    for (i = streambrowser_cfg.bookmarks_count; i < old_bookmarks_count; i++)
    {
        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_streamdir_name", i);
        aud_cfg_db_unset_key (db, "streambrowser", item);

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_name", i);
        aud_cfg_db_unset_key (db, "streambrowser", item);

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_playlist_url", i);
        aud_cfg_db_unset_key (db, "streambrowser", item);

        g_snprintf (item, DEF_STRING_LEN, "bookmark%d_url", i);
        aud_cfg_db_unset_key (db, "streambrowser", item);
    }

    aud_cfg_db_close (db);

    AUDDBG("configuration saved\n");
}

gboolean mystrcasestr (const char *haystack, const char *needle)
{
    int len_h = strlen (haystack) + 1;
    int len_n = strlen (needle) + 1;
    int i;

    char *upper_h = malloc (len_h);
    char *upper_n = malloc (len_n);

    for (i = 0; i < len_h; i++)
        upper_h[i] = toupper (haystack[i]);
    for (i = 0; i < len_n; i++)
        upper_n[i] = toupper (needle[i]);

    char *p = strstr (upper_h, upper_n);

    free (upper_h);
    free (upper_n);

    if (p != NULL)
        return TRUE;
    else
        return FALSE;
}


static gboolean sb_init (void)
{
    AUDDBG("sb_init()\n");
    config_load ();
    gui_init ();
    return TRUE;
}

static void sb_about (void)
{
    static GtkWidget * about_window = NULL;

    audgui_simple_message (& about_window, GTK_MESSAGE_INFO,
     _("About Stream Browser"),
     _("Copyright (c) 2008, by Calin Crisan <ccrisan@gmail.com> and The Audacious Team.\n\n"
     "This is a simple stream browser that includes the most popular streaming directories.\n"
     "Many thanks to the Streamtuner developers <http://www.nongnu.org/streamtuner>,\n"
     "\tand of course to the whole Audacious community.\n\n"
     "Also thank you Tony Vroon for mentoring & guiding me, again.\n\n"
     "This was a Google Summer of Code 2008 project."));
}

static void sb_configure ()
{
    AUDDBG("sb_configure()\n");
}

static void sb_cleanup ()
{
    AUDDBG("sb_cleanup()\n");

    gui_done ();
    config_save ();
}

static void gui_init ()
{
    /* main streambrowser window */
    sb_gui_widget = streambrowser_win_init ();
    g_signal_connect (sb_gui_widget, "destroy", (GCallback) gtk_widget_destroyed, & sb_gui_widget);

    streambrowser_win_set_update_function (streamdir_update);

    /* others */
    update_thread_mutex = g_mutex_new ();
    update_thread_data_queue = g_queue_new ();

    AUDDBG("gui initialized\n");

    streamdir_update (NULL, NULL, NULL, FALSE);
}

static void gui_done ()
{
    /* others */
    if (update_thread_mutex)
        g_mutex_free (update_thread_mutex);
    update_thread_mutex = NULL;
    if (update_thread_data_queue)
        g_queue_free (update_thread_data_queue);
    update_thread_data_queue = NULL;

    AUDDBG("gui destroyed\n");
}

static void streamdir_update (streamdir_t * streamdir, category_t * category,
                              streaminfo_t * streaminfo,
                              gboolean add_to_playlist)
{
    AUDDBG
        ("requested streamdir update (streamdir = '%s', category = '%s', streaminfo = '%s', add_to_playlist = %d)\n",
         streamdir == NULL ? "" : streamdir->name,
         category == NULL ? "" : category->name,
         streaminfo == NULL ? "" : streaminfo->name, add_to_playlist);

    if (g_queue_get_length (update_thread_data_queue) >= MAX_UPDATE_THREADS)
    {
        AUDDBG
            ("another %d streamdir updates are pending, this request will be dropped\n",
             g_queue_get_length (update_thread_data_queue));
    }
    else
    {
        g_mutex_lock (update_thread_mutex);

        /* do we have a running thread? */
        if (g_queue_get_length (update_thread_data_queue) > 0)
        {
            int i;
            gboolean exists = FALSE;
            update_thread_data_t *update_thread_data;

            /* search for another identic update request */
            for (i = 0; i < g_queue_get_length (update_thread_data_queue); i++)
            {
                update_thread_data =
                    g_queue_peek_nth (update_thread_data_queue, i);
                if (update_thread_data->streamdir == streamdir
                    && update_thread_data->category == category
                    && update_thread_data->streaminfo == streaminfo
                    && update_thread_data->add_to_playlist == add_to_playlist)
                {
                    exists = TRUE;
                    break;
                }
            }

            /* if no other similar request exists, we enqueue it */
            if (!exists)
            {
                AUDDBG
                    ("another %d streamdir updates are pending, this request will be queued\n",
                     g_queue_get_length (update_thread_data_queue));

                update_thread_data = g_malloc (sizeof (update_thread_data_t));

                update_thread_data->streamdir = streamdir;
                update_thread_data->category = category;
                update_thread_data->streaminfo = streaminfo;
                update_thread_data->add_to_playlist = add_to_playlist;

                g_queue_push_tail (update_thread_data_queue,
                                   update_thread_data);
            }
            else
            {
                AUDDBG
                    ("this request is already present in the queue, dropping\n");
            }
        }
        /* no thread is currently running, we start one */
        else
        {
            AUDDBG
                ("no other streamdir updates are pending, starting to process this request immediately\n");

            update_thread_data_t *data =
                g_malloc (sizeof (update_thread_data_t));

            data->streamdir = streamdir;
            data->category = category;
            data->streaminfo = streaminfo;
            data->add_to_playlist = add_to_playlist;

            g_queue_push_tail (update_thread_data_queue, data);

            g_thread_create ((GThreadFunc) update_thread_core, NULL, FALSE,
                             NULL);
        }

        g_mutex_unlock (update_thread_mutex);
    }
}

static gpointer update_thread_core (gpointer user_data)
{
    AUDDBG("entering update thread core\n");

    /* try to get the last item in the queue, but don't remove it */
    g_mutex_lock (update_thread_mutex);
    update_thread_data_t *data = NULL;
    if (g_queue_get_length (update_thread_data_queue) > 0)
    {
        data = g_queue_peek_head (update_thread_data_queue);
    }
    g_mutex_unlock (update_thread_mutex);

    /* repetitively process the queue elements, until queue is empty */
    while (data != NULL && g_queue_get_length (update_thread_data_queue) > 0)
    {
        if (data->streamdir && !streamdir_is_valid(data->streamdir)) {
            g_free(data);
            g_mutex_lock(update_thread_mutex);
            /* remove the just processed data from the queue */
            g_queue_pop_head(update_thread_data_queue);
            /* try to get the last item in the queue */
            if (g_queue_get_length(update_thread_data_queue) > 0)
                data = g_queue_peek_head(update_thread_data_queue);
            else
                data = NULL;
            g_mutex_unlock(update_thread_mutex);
            continue;
        }
        /* update a streaminfo */
        if (data->streaminfo != NULL)
        {
            gdk_threads_enter ();
            if (sb_gui_widget)
                streambrowser_win_set_streaminfo_state (data->streamdir, data->category, data->streaminfo, TRUE);
            gdk_threads_leave ();

            if (data->add_to_playlist)
                streaminfo_add_to_playlist (data->streaminfo);
            else
            {
                /* shoutcast */
                if (strncmp
                    (data->streamdir->name, SHOUTCAST_NAME,
                     strlen (SHOUTCAST_NAME)) == 0)
                {
                    shoutcast_streaminfo_fetch (data->category,
                                                data->streaminfo);
                }
                /* xiph */
                else if (strncmp
                         (data->streamdir->name, XIPH_NAME,
                          strlen (XIPH_NAME)) == 0)
                {
                    xiph_streaminfo_fetch (data->category, data->streaminfo);
                }
                /* bookmarks */
                else if (strncmp
                         (data->streamdir->name, BOOKMARKS_NAME,
                          strlen (BOOKMARKS_NAME)) == 0)
                {
                    bookmarks_streaminfo_fetch (data->category,
                                                data->streaminfo);
                }
            }

            gdk_threads_enter ();
            if (sb_gui_widget)
            {
                if (! data->add_to_playlist)
                    streambrowser_win_set_streaminfo (data->streamdir, data->category, data->streaminfo);
                streambrowser_win_set_streaminfo_state (data->streamdir, data->category, data->streaminfo, FALSE);
            }
            gdk_threads_leave ();
        }
        /* update a category */
        else if (data->category != NULL)
        {
            gdk_threads_enter ();
            if (sb_gui_widget)
                streambrowser_win_set_category_state (data->streamdir, data->category, TRUE);
            gdk_threads_leave ();

            /* shoutcast */
            if (strncmp
                (data->streamdir->name, SHOUTCAST_NAME,
                 strlen (SHOUTCAST_NAME)) == 0)
            {
                shoutcast_category_fetch (data->streamdir, data->category);
            }
            /* xiph */
            else if (strncmp
                     (data->streamdir->name, XIPH_NAME,
                      strlen (XIPH_NAME)) == 0)
            {
                xiph_category_fetch (data->streamdir, data->category);
            }
            /* bookmarks */
            else if (strncmp
                     (data->streamdir->name, BOOKMARKS_NAME,
                      strlen (BOOKMARKS_NAME)) == 0)
            {
                bookmarks_category_fetch (data->streamdir, data->category);
            }

            gdk_threads_enter ();
            if (sb_gui_widget)
            {
                streambrowser_win_set_category (data->streamdir, data->category);
                streambrowser_win_set_category_state (data->streamdir, data->category, FALSE);
            }
            gdk_threads_leave ();
        }
        /* update a streamdir */
        else if (data->streamdir != NULL)
        {
            /* shoutcast */
            if (strncmp
                (data->streamdir->name, SHOUTCAST_NAME,
                 strlen (SHOUTCAST_NAME)) == 0)
            {
                streamdir_t *streamdir = shoutcast_streamdir_fetch ();
                if (streamdir != NULL)
                {
                    gdk_threads_enter ();
                    if (sb_gui_widget)
                        streambrowser_win_set_streamdir (streamdir, SHOUTCAST_ICON);
                    gdk_threads_leave ();
                }
            }
            /* xiph */
            else if (strncmp
                     (data->streamdir->name, XIPH_NAME,
                      strlen (XIPH_NAME)) == 0)
            {
                streamdir_t *streamdir = xiph_streamdir_fetch ();
                if (streamdir != NULL)
                {
                    gdk_threads_enter ();
                    if (sb_gui_widget)
                        streambrowser_win_set_streamdir (streamdir, XIPH_ICON);
                    gdk_threads_leave ();
                }
            }
            /* bookmarks */
            else if (strncmp
                     (data->streamdir->name, BOOKMARKS_NAME,
                      strlen (BOOKMARKS_NAME)) == 0)
            {
                streamdir_t *streamdir =
                    bookmarks_streamdir_fetch (&streambrowser_cfg.bookmarks,
                                               &streambrowser_cfg.bookmarks_count);
                if (streamdir != NULL)
                {
                    gdk_threads_enter ();
                    if (sb_gui_widget)
                        streambrowser_win_set_streamdir (streamdir, BOOKMARKS_ICON);
                    gdk_threads_leave ();
                }
            }
        }
        /* update all streamdirs */
        else
        {
            /* shoutcast */
            streamdir_t *streamdir = shoutcast_streamdir_fetch ();
            if (streamdir != NULL)
            {
                gdk_threads_enter ();
                if (sb_gui_widget)
                    streambrowser_win_set_streamdir (streamdir, SHOUTCAST_ICON);
                gdk_threads_leave ();
            }
            /* xiph */
            streamdir = xiph_streamdir_fetch ();
            if (streamdir != NULL)
            {
                gdk_threads_enter ();
                if (sb_gui_widget)
                    streambrowser_win_set_streamdir (streamdir, XIPH_ICON);
                gdk_threads_leave ();
            }
            /* bookmarks */
            streamdir =
                bookmarks_streamdir_fetch (&streambrowser_cfg.bookmarks,
                                           &streambrowser_cfg.bookmarks_count);
            if (streamdir != NULL)
            {
                gdk_threads_enter ();
                if (sb_gui_widget)
                    streambrowser_win_set_streamdir (streamdir, BOOKMARKS_ICON);
                gdk_threads_leave ();

                int i;
                for (i = 0; i < category_get_count (streamdir); i++)
                    streamdir_update (streamdir,
                                      category_get_by_index (streamdir, i),
                                      NULL, FALSE);
            }
        }

        g_free (data);

        g_mutex_lock (update_thread_mutex);

        /* remove the just processed data from the queue */
        g_queue_pop_head (update_thread_data_queue);

        /* try to get the last item in the queue */
        if (g_queue_get_length (update_thread_data_queue) > 0)
            data = g_queue_peek_head (update_thread_data_queue);
        else
            data = NULL;

        g_mutex_unlock (update_thread_mutex);
    }

    AUDDBG("leaving update thread core\n");

    return NULL;
}

static void streaminfo_add_to_playlist (streaminfo_t * streaminfo)
{
    gint playlist = aud_playlist_get_active ();
    gchar * unix_name = g_build_filename (aud_util_get_localdir (),
     PLAYLIST_TEMP_FILE, NULL);
    gchar * uri_name = g_filename_to_uri (unix_name, NULL, NULL);

    if (strlen (streaminfo->playlist_url) > 0)
    {
        AUDDBG("fetching stream playlist for station '%s' from '%s'\n",
               streaminfo->name, streaminfo->playlist_url);

        if (! fetch_remote_to_local_file (streaminfo->playlist_url, uri_name))
        {
            failure ("shoutcast: stream playlist '%s' could not be downloaded "
             "to '%s'\n", streaminfo->playlist_url, uri_name);
            goto DONE;
        }

        AUDDBG("stream playlist '%s' successfuly downloaded to '%s'\n",
         streaminfo->playlist_url, uri_name);

        aud_playlist_entry_insert (playlist, -1, g_strdup (uri_name), NULL,
         FALSE);
        AUDDBG("stream playlist '%s' added\n", streaminfo->playlist_url);
    }

    if (strlen (streaminfo->url) > 0)
    {
        aud_playlist_entry_insert (playlist, -1, g_strdup (streaminfo->url),
         NULL, FALSE);
        AUDDBG("stream '%s' added\n", streaminfo->url);
    }

DONE:
    g_free (unix_name);
    g_free (uri_name);
}
