/* Audacious lastfm transport plugin
 * Copyright (c) 2007 Cristi Măgherușan <majeru@gentoo.ro>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 *       Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *       Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 * 
 *       Neither the name of the author nor the names of its contributors may be
 *       used to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Current status and known issues:
 *      - Works fine ant is relatively stable 
 *      - The adjust fails when having 2 or more opened streams at the same time.
 *              * It will randomly adjust to any one of them, because the playlist keeps 
 *                pulling metadata
 *      - When the network is disconnected opening a new track freezes the player 
 *              * This seems to recover after the connection is restablished
 *              * Ordinary mp3 streams seem to share this behavior. Didnt tested if others do.
 */

#include <audacious/vfs.h>
#include <audacious/plugin.h>
#include <audacious/configdb.h>
#include <libmowgli/mowgli_global_storage.h>
#include <curl/curl.h>
#include <glib.h>
#include <string.h>
#include "lastfm.h"
#include <glib/gi18n.h>

#define DEBUG 1

size_t lastfm_store_res(void *ptr, size_t size, size_t nmemb, void *udata)
{
        GString *data = (GString *) udata;
        g_string_append_len(data, ptr, nmemb);
        return size * nmemb;
}


gchar** lastfm_get_data_from_uri(gchar *url)
{
        gchar**result=NULL;
        GString* res=g_string_new(NULL);
        gint i=0;
        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Audacious");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_store_res);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LASTFM_CURL_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, res);
        gint status = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        /* g_print(res->str);*/
        if((status==CURLE_OK) && res && res->str)
                result = g_strsplit(res->str, "\n", 20);
        g_string_erase(res, 0, -1);
#if DEBUG
        g_print("Opened URL: '%s'\n",url);
        g_print("LASTFM: (get_uri) received data ---------------\n");
        for (i = 0; result && result[i]; i++)
                g_print("%s\n",result[i]);
        g_print("LASTFM: (get_uri) data ended    ---------------\n");
#endif
        return result;
}


static void show_login_error_dialog(void)
{
        const gchar *markup =
                N_("<b><big>Couldn't initialize the last.fm radio plugin.</big></b>\n\n"
                                "Check if your Scrobbler's plugin login data is set up properly.");

        GtkWidget *dialog =
                gtk_message_dialog_new_with_markup(NULL,
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_ERROR,
                                GTK_BUTTONS_OK,
                                _(markup));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
}

gchar* lastfm_get_login_uri()  /* reads the audioscrobbler login data from the config */
{                              /* and then uses them to create a login URL*/
        ConfigDb *cfg = NULL;
        gchar   *buf=NULL,
                *username = NULL, 
                *password = NULL;
        if ((cfg = aud_cfg_db_open()) != NULL)
        {
                aud_cfg_db_get_string(cfg, "audioscrobbler","username",
                                &username);
                aud_cfg_db_get_string(cfg, "audioscrobbler","password",
                                &password);
                g_free(cfg);
        }
        if (username != NULL && password != NULL)
        {
                buf=g_strdup_printf(LASTFM_HANDSHAKE_URL, username, password);
                g_free(password);
                g_free(username);
                return buf;
        }
        else 
        {
#if DEBUG
                g_print("LASTFM: (get_login_uri) Couldn't find the login data. Use the scrobbler plugin to set it up.\n");
#endif
                show_login_error_dialog();
                return NULL;
        }
}

void lastfm_store(gchar * var_name, gchar * var)  /*mowgli storage wrapper, for storing global data*/
{
        if (mowgli_global_storage_get(var_name))
                mowgli_global_storage_free(var_name);

        mowgli_global_storage_put(var_name,var);
}

int lastfm_login(void)  /*gets the session ID and the mp3 stream URL and stores them.
                          It's called just on the first fopen, since it doesnt change (hopefully!!!)*/
{
        gint    i,
                ret=LASTFM_LOGIN_OK; /*suppose everything goes fine*/
        gchar   **split = NULL;
        if(login_url!=NULL)
                g_free(login_url);
        login_url=lastfm_get_login_uri();
        if(login_url==NULL)
                return LASTFM_MISSING_LOGIN_DATA;
        split = lastfm_get_data_from_uri(login_url);
        if (split)
        {
                for (i = 0; split && split[i]; i++)
                {
                        if (g_str_has_prefix(split[i], "session="))
                                lastfm_store("lastfm_session_id",g_strndup(split[i] + 8, 32));
                        else if (g_str_has_prefix(split[i], "stream_url="))
                                lastfm_store("lastfm_stream_uri" ,g_strdup(split[i] + 11));
                }
                ret = LASTFM_LOGIN_OK;
        }
        else
                ret = LASTFM_LOGIN_ERROR;
        g_strfreev(split);
        g_free(login_url);
        login_url=NULL;
        return ret;
}

static gchar* parse(gchar* input_string,gchar* token)
{
        if (!g_str_has_prefix(input_string, token))
                return NULL;

        return g_strdup(strchr(input_string, '=') + 1);
}

static gpointer lastfm_adjust(gpointer uri)  /*tunes into a channel*/
{
        gchar *fetch_url=NULL,
              *session_id = g_strdup(mowgli_global_storage_get("lastfm_session_id"));
        if (!session_id)
        {
#if DEBUG
                g_print("LASTFM: (adjust) Adjust failed! Session ID not set.\n");
#endif
                return NULL ;
        }
        fetch_url=g_strdup_printf(LASTFM_ADJUST_URL, session_id, (char*)uri);
        gchar** tmp = lastfm_get_data_from_uri(fetch_url);      /*the output doesn't matter*/
        g_strfreev(tmp);
        g_free(fetch_url);
        g_free(session_id);
        return NULL ;
}

gboolean parse_metadata(LastFM * handle,gchar ** split)
{
        int i;

        if(split == NULL)
                return FALSE;
        if(g_str_has_prefix(split[0],"streaming=false"))
        {
#if DEBUG
                g_print("LASTFM: (parse) Metadata is not available\n");
#endif
                return FALSE;
        }
        handle->lastfm_duration=0;
        handle->lastfm_progress=0;

        if (handle->lastfm_artist)
        {
                g_free(handle->lastfm_artist);
                handle->lastfm_artist=NULL;
        }
        if (handle->lastfm_title)
        {
                g_free(handle->lastfm_title);
                handle->lastfm_title=NULL;
        }
        if (handle->lastfm_album)
        {
                g_free(handle->lastfm_album);
                handle->lastfm_album=NULL;
        }
        if (handle->lastfm_station_name)
        {
                g_free(handle->lastfm_station_name);
                handle->lastfm_station_name=NULL;
        }
        for (i = 0; split && split[i]; i++)
        {
                if (g_str_has_prefix(split[i], "artist="))
                        handle->lastfm_artist = parse(split[i],"artist=");

                if (g_str_has_prefix(split[i], "track="))
                        handle->lastfm_title  = parse(split[i],"track=");

                if (g_str_has_prefix(split[i], "album="))
                        lastfm_store("lastfm_album", parse(split[i],"album=" ));

                if (g_str_has_prefix(split[i], "albumcover_medium="))
                        lastfm_store("lastfm_cover", parse(split[i],"albumcover_medium=")); 

                if (g_str_has_prefix(split[i], "station="))
                        handle->lastfm_station_name = parse(split[i],"station=");

                if (g_str_has_prefix(split[i], "trackduration="))
                        handle->lastfm_duration = g_ascii_strtoull(g_strdup(split[i] + 14), NULL, 10);
                if (g_str_has_prefix(split[i], "trackprogress="))
                        handle->lastfm_progress = g_ascii_strtoull(g_strdup(split[i] + 14), NULL, 10);

        }
#if DEBUG
        if(handle->lastfm_station_name!=NULL)
        {
                g_print("nLASTFM: (parse) Duration:%d\n", handle->lastfm_duration);
                g_print("LASTFM: (parse) Station Name: %s\n", handle->lastfm_station_name);
        }
#endif
        return TRUE;
}

int fetch_metadata(LastFM * handle)
{
#if DEBUG
        g_print("LASTFM: (fetch) starting to fetch\n");
#endif
        gchar *uri=NULL;
        gint res=METADATA_FETCH_FAILED;
        if(handle==NULL)
                return res;

        handle->lastfm_session_id=mowgli_global_storage_get("lastfm_session_id");
        if (handle->lastfm_session_id == NULL)
                return res;
        uri=g_strdup_printf(LASTFM_METADATA_URL, handle->lastfm_session_id);
        gchar**fetched_metadata =NULL;
        fetched_metadata = lastfm_get_data_from_uri(uri);
        if(fetched_metadata != NULL)
        {
                if(parse_metadata( handle,fetched_metadata))
                {
                        g_strfreev(fetched_metadata);
                        res=METADATA_FETCH_SUCCEEDED;
#if DEBUG
                        g_print("LASTFM: (fetch) metadata was parsed ok\n");
#endif
                }

        }
        return res;
}

gpointer lastfm_metadata_thread_func(gpointer arg)
{    
        gint    sleep_duration=1,
                previous_track_duration=-1,
                count=1,
                status=0,
                err=0;
        gboolean track_end_expected=FALSE,track_beginning=TRUE;
        LastFM *handle = (LastFM *)arg;
        /*get it right after opened the stream, so it doesnt need the mutex */
        fetch_metadata(handle); 

        /* metadata is fetched 1 second after the stream is opened, 
         * and again after 2 seconds.
         * if metadata was fetched ok i'm waiting for 
         * track_length - fetch_duration - 10 seconds
         * then start polling for new metadata each 2 seconds, until
         * the track gets changed from the previous iteration
         */
        do
        {
                if(count%sleep_duration==0)
                {    
                        g_mutex_lock(metadata_mutex);
                        if(handle==NULL)
                        {
#if DEBUG
                                g_print("LASTFM: (thread) Exiting thread, ID = %p, thread's handle got freed\n", (void *)g_thread_self());
#endif
                                thread_count--;
                                return NULL;       
                        }
                        if(t0->tv_usec==-1)
                                g_get_current_time (t0);
#if DEBUG
                        g_print("LASTFM: (thread) Fetching metadata:\n");
#endif
                        status=fetch_metadata(handle);
                        g_get_current_time (t1);
                        if(status==METADATA_FETCH_SUCCEEDED)
                        {        
                                if(!track_end_expected)
                                {
                                        if(track_beginning)
                                        {       /*first try after track has changed*/
#if DEBUG
                                                g_print("LASTFM: (thread) retrying in 2 seconds\n");
#endif
                                                track_beginning=FALSE;
                                                sleep_duration=2;
                                        }
                                        else 
                                        {
                                                if(g_str_has_prefix(handle->lastfm_station_name, "Previewing 30-second clips"))
                                                        handle->lastfm_duration=30;
                                                sleep_duration=handle->lastfm_duration-(t1->tv_sec - t0->tv_sec)-6;
                                                previous_track_duration=handle->lastfm_duration;
                                                count=err=0;
                                                track_end_expected=TRUE; /*then the track_end will follow*/
                                                track_beginning=TRUE;
                                                t0->tv_usec=-1;
#if DEBUG
                                                g_print("LASTFM: (thread) second fetch after new track started, the next will follow in %d sec\n",sleep_duration);
#endif
                                        }

                                }
                                else
                                {       
                                        /*if the track hasnt yet changed (two tracks are considered identical if they 
                                         * have the same length or the same title)
                                         */
                                        if(handle->lastfm_duration == previous_track_duration)
                                        {      
#if DEBUG                                        
                                                g_print("LASTFM: (thread) it is the same track as before, waiting for new track to start\n");
#endif
                                                sleep_duration=2;
                                        }
                                        else
                                        {
#if DEBUG
                                                g_print("LASTFM: (thread) the track has changed\n");
#endif
                                                track_end_expected=FALSE;
                                                sleep_duration=2;
                                                /*if(previous_track_title)
                                                  {
                                                  g_free(previous_track_title);
                                                  previous_track_title=NULL;
                                                  }*/
                                        }
                                }
#if DEBUG
                                g_print("LASTFM: (thread) Current thread, ID = %p\n", (void *)g_thread_self());
#endif
                        }
                        else 
                        {
                                err++;
                                sleep_duration<<=1;
                        }
#if DEBUG
                        g_print("LASTFM: (thread) Thread_count: %d\n",thread_count);
                        g_print("LASTFM: (thread) sleepping for %d seconds. ",err? sleep_duration/2 :sleep_duration);
                        g_print("Track length = %d sec\n",handle->lastfm_duration);
#endif
                        g_mutex_unlock(metadata_mutex);
                }
                sleep(1);
                count++;
        }
        while ((g_thread_self()==metadata_thread )&& (err<7));

#if DEBUG
        g_print("LASTFM: (thread) Exiting thread, ID = %p\n", (void *)g_thread_self());
#endif
        thread_count--;
        return NULL;
}

VFSFile *lastfm_aud_vfs_fopen_impl(const gchar * path, const gchar * mode)
{
        VFSFile *file = g_new0(VFSFile, 1);
        LastFM  *handle = g_new0(LastFM, 1);
        handle->lastfm_artist=NULL;
        handle->lastfm_title=NULL;
        handle->lastfm_album=NULL;
        handle->lastfm_session_id=NULL;
        handle->lastfm_mp3_stream_url=NULL;
        handle->lastfm_station_name=g_strdup("Couldn't fetch metadata");
        int login_count = 0;
        gchar * temp_path=g_strdup(path);
        if(!mowgli_global_storage_get("lastfm_session_id")) /*login only if really needed*/
        {
                while((login_count++ <= 3)&&(lastfm_login()!= LASTFM_LOGIN_OK))
                        sleep(5);
                if(login_count>3)
                {
                        g_free(handle);
                        g_free(file);
                        return NULL;
                }
        }
        handle->lastfm_session_id = g_strdup(mowgli_global_storage_get("lastfm_session_id"));
        handle->lastfm_mp3_stream_url = g_strdup(mowgli_global_storage_get("lastfm_stream_uri"));
        g_get_current_time(t0);
        g_thread_create(lastfm_adjust,temp_path,FALSE,NULL);
        metadata_thread = g_thread_create(lastfm_metadata_thread_func, handle, FALSE, NULL);
        thread_count++;
        handle->proxy_fd = aud_vfs_fopen(handle->lastfm_mp3_stream_url, mode);
        file->handle = handle;
#if DEBUG
        g_print("LASTFM: (fopen) Thread_count: %d\n",thread_count);
#endif
        return file;
}

gint lastfm_aud_vfs_fclose_impl(VFSFile * file)
{
        gint ret = 0;

        if (file == NULL)
                return -1;
        if (file->handle) 
        {
                g_mutex_lock(metadata_mutex);
                LastFM *handle = file->handle;
                ret = aud_vfs_fclose(handle->proxy_fd);
                if (!ret)
                        handle->proxy_fd = NULL;
                g_free(handle);
                handle=NULL;
                file->handle = NULL;
                g_mutex_unlock(metadata_mutex);
        }
        return ret;
}

size_t lastfm_aud_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
        LastFM *handle = file->handle;
        size_t ret = aud_vfs_fread(ptr, size, nmemb, handle->proxy_fd);
        return ret;
}

size_t lastfm_aud_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
        return -1;
}

gint lastfm_aud_vfs_getc_impl(VFSFile * stream)
{
        LastFM *handle = stream->handle;
        return aud_vfs_getc(handle->proxy_fd);
}

gint lastfm_aud_vfs_ungetc_impl(gint c, VFSFile * stream)
{
        LastFM *handle = stream->handle;
        return aud_vfs_ungetc(c, handle->proxy_fd);
}

gint lastfm_aud_vfs_fseek_impl(VFSFile * file, glong offset, gint whence)
{
        return -1;
}

void lastfm_aud_vfs_rewind_impl(VFSFile * file)
{
        return;
}

glong lastfm_aud_vfs_ftell_impl(VFSFile * file)
{
        LastFM *handle = file->handle;

        return aud_vfs_ftell(handle->proxy_fd);
}

gboolean lastfm_aud_vfs_feof_impl(VFSFile * file)
{
        LastFM *handle = file->handle;

        return aud_vfs_feof(handle->proxy_fd);
}

gint lastfm_aud_vfs_truncate_impl(VFSFile * file, glong size)
{
        return -1;
}

off_t lastfm_aud_vfs_fsize_impl(VFSFile * file)
{
        return 0;
}

gchar *lastfm_aud_vfs_metadata_impl(VFSFile * file, const gchar * field)
{
        LastFM * handle;
        if(file->handle!= NULL)
                handle = file->handle;
        else 
                return NULL;

        if (!g_ascii_strncasecmp(field, "stream-name", 11) && (handle->lastfm_station_name != NULL))
                return g_strdup_printf("last.fm radio: %s", handle->lastfm_station_name);
        if (!g_ascii_strncasecmp(field, "track-name", 10) && (handle->lastfm_title != NULL) && (handle->lastfm_artist != NULL))
                return g_strdup_printf("%s - %s", handle->lastfm_artist, handle->lastfm_title);
        if (!g_ascii_strncasecmp(field, "content-type", 12))
                return g_strdup("audio/mpeg");

        return NULL;
}

VFSConstructor lastfm_const = {
        "lastfm://",
        lastfm_aud_vfs_fopen_impl,
        lastfm_aud_vfs_fclose_impl,
        lastfm_aud_vfs_fread_impl,
        lastfm_aud_vfs_fwrite_impl,
        lastfm_aud_vfs_getc_impl,
        lastfm_aud_vfs_ungetc_impl,
        lastfm_aud_vfs_fseek_impl,
        lastfm_aud_vfs_rewind_impl,
        lastfm_aud_vfs_ftell_impl,
        lastfm_aud_vfs_feof_impl,
        lastfm_aud_vfs_truncate_impl,
        lastfm_aud_vfs_fsize_impl,
        lastfm_aud_vfs_metadata_impl
};

static void init(void)
{       
        aud_vfs_register_transport(&lastfm_const);
        if (!metadata_mutex)
                metadata_mutex = g_mutex_new ();
        t0=g_new0(GTimeVal,1);
        t1=g_new0(GTimeVal,1);
        lastfm_store("lastfm_loaded","TRUE");
}

static void cleanup(void)
{
        g_mutex_free(metadata_mutex);
        mowgli_global_storage_free("lastfm_stream_uri");
#if DEBUG
        g_print("LASTFM: (cleanup) Cleanup finished\n");
#endif
}

DECLARE_PLUGIN(lastfm, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL)
