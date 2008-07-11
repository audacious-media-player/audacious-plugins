
#include <stdlib.h>
#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/ui_plugin_menu.h>

#include "streambrowser.h"
#include "streamdir.h"
#include "shoutcast.h"
#include "gui/streambrowser_win.h"
#include "gui/about_win.h"


typedef struct {
    streamdir_t *streamdir;
    category_t *category;
    streaminfo_t *streaminfo;
} update_thread_data_t;


static void sb_init();
static void sb_about();
static void sb_configure();
static void sb_cleanup();

static void gui_init();
static void gui_done();
static void config_load();
static void config_save();

static void streamdir_update(streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo);
static gpointer update_thread_core(gpointer user_data);
static void streaminfo_add_to_playlist(streaminfo_t *streaminfo);
static void on_plugin_services_menu_item_click();

static GtkWidget *playlist_menu_item;
static GtkWidget *main_menu_item;
static GQueue *update_thread_data_queue = NULL;
static GMutex *update_thread_mutex = NULL;
static gint update_thread_count = 0;

streambrowser_cfg_t streambrowser_cfg;

static GeneralPlugin sb_plugin = 
{
    .description = "Stream Browser",
    .init = sb_init,
    .about = sb_about,
    .configure = sb_configure,
    .cleanup = sb_cleanup
};

GeneralPlugin *sb_gplist[] = 
{
    &sb_plugin,
    NULL
};

SIMPLE_GENERAL_PLUGIN(streambrowser, sb_gplist);


void debug(const char *fmt, ...)
{
    if (streambrowser_cfg.debug) {
        va_list ap;
        fprintf(stderr, "* streambrowser: ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
}

void failure(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "! streambrowser: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

gboolean fetch_remote_to_local_file(gchar *remote_url, gchar *local_url)
{
    VFSFile *remote_file = aud_vfs_fopen(remote_url, "r");
    if (remote_file == NULL) {
        failure("failed to fetch file '%s'\n", remote_url);
        return FALSE;
    }

    VFSFile *local_file = aud_vfs_fopen(local_url, "w");
    if (local_file == NULL) {
        aud_vfs_fclose(remote_file);

        failure("failed to create local file '%s'\n", local_file);
        return FALSE;
    }

    unsigned char buff[DEF_BUFFER_SIZE];
    int size;
    while (!aud_vfs_feof(remote_file)) {
        size = aud_vfs_fread(buff, 1, DEF_BUFFER_SIZE, remote_file);

        // i don't know why aud_vfs_feof() doesn't ever return TRUE
        // so this is a workaround to properly end the loop
        if (size == 0)
            break;

        size = aud_vfs_fwrite(buff, 1, size, local_file);
        if (size == 0) {
            aud_vfs_fclose(local_file);
            aud_vfs_fclose(remote_file);

            failure("failed to write to local file '%s'\n", local_file);
            return FALSE;
        }
    }

    aud_vfs_fclose(local_file);
    aud_vfs_fclose(remote_file);

    return TRUE;
}


static void sb_init()
{
    /* workaround to print sb_init() */
    streambrowser_cfg.debug = TRUE;
    debug("sb_init()\n");
    streambrowser_cfg.debug = FALSE;

    config_load();
    gui_init();
}

static void sb_about()
{
    debug("sb_about()\n");
}

static void sb_configure()
{
    debug("sb_configure()\n");
}

static void sb_cleanup()
{
    debug("sb_cleanup()\n");

    gui_done();
    config_save();
}

static void gui_init()
{
    /* the plugin services menu */
    playlist_menu_item = gtk_image_menu_item_new_with_label(_("Streambrowser"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(playlist_menu_item), gtk_image_new_from_file(STREAMBROWSER_ICON_SMALL));
    gtk_widget_show(playlist_menu_item);
    g_signal_connect(G_OBJECT(playlist_menu_item), "activate", G_CALLBACK(on_plugin_services_menu_item_click), NULL);
    audacious_menu_plugin_item_add(AUDACIOUS_MENU_PLAYLIST_RCLICK, playlist_menu_item);

    main_menu_item = gtk_image_menu_item_new_with_label(_("Streambrowser"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(main_menu_item), gtk_image_new_from_file(STREAMBROWSER_ICON_SMALL));
    gtk_widget_show(main_menu_item);
    g_signal_connect(G_OBJECT(main_menu_item), "activate", G_CALLBACK(on_plugin_services_menu_item_click), NULL);
    audacious_menu_plugin_item_add(AUDACIOUS_MENU_MAIN, main_menu_item);

    /* main streambrowser window */
    streambrowser_win_init();
    streambrowser_win_set_update_function(streamdir_update);

    /* others */
    update_thread_mutex = g_mutex_new();
    update_thread_data_queue = g_queue_new();

    debug("gui initialized\n");
}

static void gui_done()
{
    /* the plugin services menu */
    audacious_menu_plugin_item_remove(AUDACIOUS_MENU_PLAYLIST_RCLICK, playlist_menu_item);
    audacious_menu_plugin_item_remove(AUDACIOUS_MENU_MAIN, main_menu_item);

    /* main streambrowser window */
    streambrowser_win_hide();
    streambrowser_win_done();

    /* others */
    if (update_thread_mutex)
        g_mutex_free(update_thread_mutex);
    update_thread_mutex = NULL;
    if (update_thread_data_queue)
        g_queue_free(update_thread_data_queue);
    update_thread_data_queue = NULL;

    debug("gui destroyed\n");
}

static void config_load()
{
    streambrowser_cfg.debug = FALSE;

    mcs_handle_t *db;
    if ((db = aud_cfg_db_open()) == NULL) {
        failure("failed to load configuration\n");
        return;
    }

    aud_cfg_db_get_bool(db, "streambrowser", "debug", &streambrowser_cfg.debug);

    aud_cfg_db_close(db);

    debug("configuration loaded\n");
    debug("debug = %d\n", streambrowser_cfg.debug);
}

static void config_save()
{
    mcs_handle_t *db;
    if ((db = aud_cfg_db_open()) == NULL) {
        failure("failed to save configuration\n");
        return;
    }

    aud_cfg_db_set_bool(db, "streambrowser", "debug", streambrowser_cfg.debug);

    aud_cfg_db_close(db);

    debug("configuration saved\n");
}

static void streamdir_update(streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo)
{
    debug("requested streamdir update (streamdir = '%s', category = '%s', streaminfo = '%s')\n", 
          streamdir == NULL ? "" : streamdir->name, 
          category == NULL ? "" : category->name,
          streaminfo == NULL ? "" : streaminfo->name);

    if (update_thread_count >= MAX_UPDATE_THREADS) {
        debug("another %d streamdir updates are pending, this request will be dropped\n", update_thread_count);
    }
    else {
        g_mutex_lock(update_thread_mutex);
            
    	/* do we have a running thread? */
        if (update_thread_count > 0) {
            int i;
            gboolean exists = FALSE;
            update_thread_data_t *update_thread_data;

            /* search for another identic update request */
            for (i = 0; i < g_queue_get_length(update_thread_data_queue); i++) {
                update_thread_data = g_queue_peek_nth(update_thread_data_queue, i);
                if (update_thread_data->streamdir == streamdir &&
                    update_thread_data->category == category &&
                    update_thread_data->streaminfo == streaminfo) {
                    exists = TRUE;
                    break;
                }
            }
            
            /* if no other similar request exists, we enqueue it */
            if (!exists) {
                debug("another %d streamdir updates are pending, this request will be queued\n", update_thread_count);

                update_thread_data = g_malloc(sizeof(update_thread_data_t));

                update_thread_data->streamdir = streamdir;
                update_thread_data->category = category;
                update_thread_data->streaminfo = streaminfo;
 
                g_queue_push_tail(update_thread_data_queue, update_thread_data);
                update_thread_count++;
            }
            else {
                debug("this request is already present in the queue, dropping\n");          
            }
        }
        /* no thread is currently running, we start one */
        else {
        	debug("no other streamdir updates are pending, starting to process this request immediately\n");
        
            update_thread_data_t *data = g_malloc(sizeof(update_thread_data_t));

            data->streamdir = streamdir;
            data->category = category;
            data->streaminfo = streaminfo;
 
            g_queue_push_tail(update_thread_data_queue, data);
            update_thread_count++;

			g_thread_create((GThreadFunc) update_thread_core, NULL, FALSE, NULL);
        }

        g_mutex_unlock(update_thread_mutex);
    }
}

static gpointer update_thread_core(gpointer user_data)
{
	debug("entering update thread core\n");

	/* try to get the last item in the queue */
	g_mutex_lock(update_thread_mutex);
	update_thread_data_t *data = NULL;
	if (update_thread_count > 0) {
		data = g_queue_pop_head(update_thread_data_queue);
	}
	g_mutex_unlock(update_thread_mutex);

	/* repetitively process the queue elements, until queue is empty */
	while (data != NULL && update_thread_count > 0) {
	    /* update a streaminfo - that is - add this streaminfo to playlist */
		if (data->streaminfo != NULL) {
	    	gdk_threads_enter();
			streambrowser_win_set_streaminfo_state(data->streamdir, data->category, data->streaminfo, TRUE);
	    	gdk_threads_leave();

		    streaminfo_add_to_playlist(data->streaminfo);

	        gdk_threads_enter();
			streambrowser_win_set_streaminfo_state(data->streamdir, data->category, data->streaminfo, FALSE);
	        gdk_threads_leave();
		}
		/* update a category */
		else if (data->category != NULL) {
		    /* shoutcast */
		    if (strncmp(data->streamdir->name, SHOUTCAST_NAME, strlen(SHOUTCAST_NAME)) == 0) {
		    	gdk_threads_enter();
				streambrowser_win_set_category_state(data->streamdir, data->category, TRUE);
		    	gdk_threads_leave();
		    	
		        shoutcast_category_fetch(data->category);

		        gdk_threads_enter();
		        streambrowser_win_set_category(data->streamdir, data->category);
				streambrowser_win_set_category_state(data->streamdir, data->category, FALSE);
		        gdk_threads_leave();
		    }
		}
		/* update a streamdir */
		else if (data->streamdir != NULL) {
		    /* shoutcast */
		    if (strncmp(data->streamdir->name, SHOUTCAST_NAME, strlen(SHOUTCAST_NAME)) == 0) {
		        streamdir_t *streamdir = shoutcast_streamdir_fetch();
		        if (streamdir != NULL) {
		            gdk_threads_enter();
		            streambrowser_win_set_streamdir(streamdir, SHOUTCAST_ICON);
		            gdk_threads_leave();
		        }
		    }
		}
		/* update all streamdirs */
		else {
		    /* shoutcast */
		    streamdir_t *shoutcast_streamdir = shoutcast_streamdir_fetch();
		    if (shoutcast_streamdir != NULL) {
		        gdk_threads_enter();
		        streambrowser_win_set_streamdir(shoutcast_streamdir, SHOUTCAST_ICON);
		        gdk_threads_leave();
		    }
		}

		g_free(data);

		g_mutex_lock(update_thread_mutex);
		update_thread_count--;	

		/* try to get the last item in the queue */
		if (update_thread_count > 0)
			data = g_queue_pop_head(update_thread_data_queue);
		else
			data = NULL;
		g_mutex_unlock(update_thread_mutex);
	}

	debug("leaving update thread core\n");

    return NULL;
}

static void streaminfo_add_to_playlist(streaminfo_t *streaminfo)
{
    debug("fetching stream playlist for station '%s' from '%s'\n", streaminfo->name, streaminfo->playlist_url);
    if (!fetch_remote_to_local_file(streaminfo->playlist_url, PLAYLIST_TEMP_FILE)) {
        failure("shoutcast: stream playlist '%s' could not be downloaded to '%s'\n", streaminfo->playlist_url, PLAYLIST_TEMP_FILE);
        return;
    }
    debug("stream playlist '%s' successfuly downloaded to '%s'\n", streaminfo->playlist_url, PLAYLIST_TEMP_FILE);

    aud_playlist_add_url(aud_playlist_get_active(), PLAYLIST_TEMP_FILE);
}

static void on_plugin_services_menu_item_click()
{
    debug("on_plugin_services_menu_item_click()\n");

    streambrowser_win_show();
    streamdir_update(NULL, NULL, NULL);
}

