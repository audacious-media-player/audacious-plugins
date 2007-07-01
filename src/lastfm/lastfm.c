/* Audacious lastfm transport plugin
 * Copyright (c) 2007 Cristi Magherusan <majeru@gentoo.ro>
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

#include <audacious/vfs.h>
#include <audacious/plugin.h>
#include <audacious/configdb.h>
#include <libmowgli/mowgli_global_storage.h>
#include <curl/curl.h>
#include <glib.h>
#include "lastfm.h"

#define DEBUG 1 

size_t lastfm_store_res(void *ptr, size_t size, size_t nmemb, void *udata)
{
        GString *data = (GString *) udata;
        g_string_append_len(data, ptr, nmemb);
        return size * nmemb;
}

gint lastfm_get_data_from_uri(gchar *url, GString * result)
{
        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mplayer");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_store_res);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LASTFM_CURL_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, result);
        gint status = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return status;
}

gchar* lastfm_get_login_uri()
{
#if DEBUG
        g_print("Getting login data from config\n");
#endif

        ConfigDb *cfgfile = NULL;
        gchar   *buf=NULL,
                *username = NULL, 
                *password = NULL;
        if ((cfgfile = bmp_cfg_db_open()) != NULL)
        {
                bmp_cfg_db_get_string(cfgfile, "audioscrobbler","username",
                                &username);
                bmp_cfg_db_get_string(cfgfile, "audioscrobbler","password",
                                &password);
                g_free(cfgfile);
        }
        if (username != NULL && password != NULL)
        {
#if DEBUG
        g_print("Creating the login URI\n");
#endif

                buf=g_strdup_printf(LASTFM_HANDSHAKE_URL, username, password);
                g_free(password);
                g_free(username);
#if DEBUG
                g_print("Succesfully created the login uri\n");
#endif
                return buf;
        }
        else {
#if DEBUG
                g_print("Couldn't find the login data. Use the scrobbler plugin to set it up.\n");
#endif

        return NULL;
        }
}

void lastfm_store(gchar *var_name,gchar* var){
if (mowgli_global_storage_get(var_name))
                mowgli_global_storage_free(var_name);

        mowgli_global_storage_put(var_name,var);
#if DEBUG
        g_print("Storing into '%s' the value '%s'\n",var_name,var);
#endif
}

int lastfm_login(void)
{
        /*gets the session ID in the URL to be played and stores them using 
         * mowgli_global_storage
         * read http://gabistapler.de/blog/index.php?/archives/268-Play-last.fm-streams-without-the-player.html for more info
         */
      
        gint status, i;
        gchar   *lastfm_session_id=NULL, 
                *lastfm_stream_uri=NULL,
                *login_uri=NULL,
                **split = NULL;
        GString *res = g_string_new(NULL);
g_print("Logging in\n");
        login_uri=lastfm_get_login_uri();
        if(login_uri==NULL){
                g_free(login_uri);
                return LASTFM_MISSING_LOGIN_DATA;
                }
        status = lastfm_get_data_from_uri(login_uri, res);
#if DEBUG
        g_print("Opened login URI: '%s'\n", login_uri);
        g_print("Got following data: '%s'\n", res->str);
#endif

        if (status == CURLE_OK)
        {
                split = g_strsplit(res->str, "\n", 7);

                for (i = 0; split && split[i]; i++)
                {
                        if (g_str_has_prefix(split[i], "session="))
                        {
                                lastfm_session_id = g_strndup(split[i] + 8, 32);
#if DEBUG
                                g_print("Got session ID:'%s'\n",lastfm_session_id);
#endif
                        }
                        else if (g_str_has_prefix(split[i], "stream_url="))
                                lastfm_stream_uri = g_strdup(split[i] + 11);
                }
        }
        else
                {
                g_strfreev(split);
                g_string_erase(res, 0, -1);
                g_free(lastfm_session_id);
                g_free(lastfm_stream_uri);
                g_free(login_uri);
                return LASTFM_LOGIN_ERROR;
                }
        lastfm_store("lastfm_session_id",lastfm_session_id);
        lastfm_store("lastfm_stream_uri",lastfm_stream_uri);

#if DEBUG
        g_print("Login finished succesfully\n");
#endif
        g_strfreev(split);
        g_string_erase(res, 0, -1);
        g_free(login_uri);
 return LASTFM_LOGIN_OK ;
}

gint lastfm_adjust(const gchar * uri)
{
        int status, i;
        gchar tmp[4096], **split = NULL;
        gboolean ret = FALSE;
        GString *res = g_string_new(NULL);
        gchar* session_id=mowgli_global_storage_get("lastfm_session_id");


        if (!session_id)
		return LASTFM_SESSION_MISSING;
#if DEBUG
        g_print("Session ID: '%s'\n",session_id);
#endif

        snprintf(tmp, sizeof(tmp), LASTFM_ADJUST_URL, session_id, uri);

	status = lastfm_get_data_from_uri(tmp, res);

#if DEBUG
	g_print("Adjust received data:%s\n", res->str);
#endif
	if (status == CURLE_OK)
	{
		split = g_strsplit(res->str, "\n", 3);

		for (i = 0; split && split[i]; i++)
		{
			if (g_str_has_prefix(split[i], "response=OK"))
				ret = LASTFM_ADJUST_OK;
			if (g_str_has_prefix(split[i], "stationname="))
			{
				mowgli_global_storage_put("lastfm_station_name", g_strdup(split[i] + 12));
#if DEBUG
				g_print ("Setting station name: '%s'\n", 
                                        (gchar*)mowgli_global_storage_get("lastfm_station_name"));
#endif
			}
		}
		g_strfreev(split);
	}
	g_string_erase(res, 0, -1);
       
	return ret;
}

void parse( gchar **split , gchar* field, gchar* data){


}

void parse_metadata(LastFM * handle,GString * metadata)
{
        gchar **split = g_strsplit(metadata->str, "\n", 20);
        int i;
        handle->lastfm_duration=0;
        
        for (i = 0; split && split[i]; i++)
        {
                if (g_str_has_prefix(split[i], "artist="))
                {
                        if (handle->lastfm_artist) 
                                g_free(handle->lastfm_artist);

                        handle->lastfm_artist = g_strdup(split[i] + 7);
#if DEBUG
                        g_print("Artist: %s\n", handle->lastfm_artist);
#endif
                }
                if (g_str_has_prefix(split[i], "track="))
                {
                        if (handle->lastfm_title) 
                                g_free(handle->lastfm_title);

                        handle->lastfm_title = g_strdup(split[i] + 6);
#if DEBUG
                        g_print("Title: %s\n", handle->lastfm_title);
#endif
                }

                if (g_str_has_prefix(split[i], "album="))
                        handle->lastfm_album = g_strdup(split[i] + 6);

                if (g_str_has_prefix(split[i], "albumcover_medium="))
                        handle->lastfm_cover = g_strdup(split[i] + 18);

                if (g_str_has_prefix(split[i], "trackduration="))
                {
                        handle->lastfm_duration = g_ascii_strtoull(g_strdup(split[i] + 14), NULL, 10);
#if DEBUG
                        g_print("Duration:%d\n", handle->lastfm_duration);
#endif
                }
                if (g_str_has_prefix(split[i], "trackprogress="))
                        handle->lastfm_progress = g_ascii_strtoull(g_strdup(split[i] + 14), NULL, 10);

                if (g_str_has_prefix(split[i], "station="))
                {
                        handle->lastfm_station_name = g_strdup(split[i] + 8);
#if DEBUG
                        g_print("Station Name: %s\n", handle->lastfm_station_name);
#endif
                }
        }

        g_strfreev(split);
return;
}


gpointer lastfm_get_metadata(gpointer arg)
{     
        gint    err=0,
                delay=-2,
                sleep_duration=1,
                count=0,
                status;
        gchar uri[4096];
        GString *res = g_string_new(NULL);
        gboolean track_end=FALSE;
        LastFM *handle = (LastFM *)arg;
        handle->lastfm_session_id =g_strdup(mowgli_global_storage_get("lastfm_session_id"));
#if DEBUG
                        g_print("Session ID: %s\n", handle->lastfm_session_id);
#endif

        if (handle->lastfm_session_id == NULL)
                return NULL;
        snprintf(uri, sizeof(uri), LASTFM_METADATA_URL, handle->lastfm_session_id);
#if DEBUG
                        g_print("Download URI: %s\n", uri);
#endif


        while ( (handle!= NULL)  && (err<5))
                //exit after 5 failed retries or after metadata_thread changes
        {       
                count++;
                if (count==sleep_duration)
                {
                        handle->lastfm_duration = 0;
                        handle->lastfm_progress = 0;
                        status = lastfm_get_data_from_uri(uri, res);
                        if (status == CURLE_OK)
                        {
#if DEBUG
                                g_print("Successfully got Metadata\n");
                                g_print("Received metadata:'%s'\nParsing...", res->str);
#endif
                                parse_metadata( handle,res);
                        }
                        g_string_erase(res, 0, -1);


                        if ((!track_end) && (handle->lastfm_duration >0))
                        {       //refresh metadata 2 sec before track's end and 2 sec after the next track starts
                                sleep_duration = handle->lastfm_duration - handle->lastfm_progress  - delay -4;
                                track_end=TRUE;
                                err=delay=count=0;
                        }
                        else
                        {
                                err++;
                                track_end=FALSE;
                                sleep_duration=4;
                                count=0;
                        }       

                        if(handle->lastfm_duration ==0)  //polling every 2 seconds until I get first data
                        {
                        sleep_duration=2;
                        count=0;
                        delay+=2;               //time until I get first data 
                                                //starts from -2 to have uniform handling for in first iteration
                                                //when calculating sleep_time
                        }

#if DEBUG
		g_print("Sleeping for %d seconds\n", sleep_duration);
#endif
	        }
                sleep(1);
        }

#if DEBUG
        g_print("Exiting thread, ID = %p\n", (void *)g_thread_self());
#endif
        handle->metadata_thread = NULL;
	return NULL;
}

VFSFile *lastfm_vfs_fopen_impl(const gchar * path, const gchar * mode)
{
//	static GThread *th;
	VFSFile *file;
	LastFM *handle;
	file = g_new0(VFSFile, 1);
	handle = g_new0(LastFM, 1);
        int login_count = 0;
        while( ! mowgli_global_storage_get("lastfm_stream_uri")&&(login_count <= 3))
	{
		printf("Login try count: %d\n", login_count++);
		lastfm_login();
		if (!mowgli_global_storage_get("lastfm_stream_uri"))
			sleep(5);
	}

        if (!mowgli_global_storage_get("lastfm_stream_uri"))
		return NULL;

	handle->lastfm_session_id = g_strdup(mowgli_global_storage_get("lastfm_session_id"));
	handle->lastfm_mp3_stream_url = g_strdup(mowgli_global_storage_get("lastfm_stream_uri"));
	
        if (!mowgli_global_storage_get("lastfm_station_name"))
                {
                        if(lastfm_adjust(path)==LASTFM_ADJUST_OK)
                        {	                                
#if DEBUG
        		g_print("Tuning was successfully completed into the channel\n");
#endif
		        }
                        else
                        {
#if DEBUG            
        		g_print("Cannot tune to given channel\n");
#endif
                        }
                }
        
        if ((handle->metadata_thread = g_thread_create(lastfm_get_metadata, handle, FALSE, NULL)) == NULL)
		            {
#if DEBUG
                                        g_print("Error creating metadata thread!!!\n");
#endif
			             return NULL;
		            }
		        else
                            {
#if DEBUG
                                        g_print("A metadata thread has just been created, ID = %p \n", (void *)handle->metadata_thread);
#endif
                            }

        handle->proxy_fd = vfs_fopen(handle->lastfm_mp3_stream_url, mode);
	file->handle = handle;

	return file;
}

gint lastfm_vfs_fclose_impl(VFSFile * file)
{
	gint ret = 0;

	if (file == NULL)
		return -1;
        LastFM *handle = file->handle;
	ret = vfs_fclose(handle->proxy_fd);
	if (!ret)
		{
                        handle->proxy_fd = NULL;
                        handle->metadata_thread=NULL;
                }
        g_free(handle);
        handle=NULL;
  	return ret;
}

size_t lastfm_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
	LastFM *handle = file->handle;
	size_t ret = vfs_fread(ptr, size, nmemb, handle->proxy_fd);
	return ret;
}

size_t lastfm_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
	return -1;
}

gint lastfm_vfs_getc_impl(VFSFile * stream)
{
	LastFM *handle = stream->handle;
	return vfs_getc(handle->proxy_fd);
}

gint lastfm_vfs_ungetc_impl(gint c, VFSFile * stream)
{
	LastFM *handle = stream->handle;

	return vfs_ungetc(c, handle->proxy_fd);
}

gint lastfm_vfs_fseek_impl(VFSFile * file, glong offset, gint whence)
{
	return -1;
}

void lastfm_vfs_rewind_impl(VFSFile * file)
{
	return;
}

glong lastfm_vfs_ftell_impl(VFSFile * file)
{
	LastFM *handle = file->handle;

	return vfs_ftell(handle->proxy_fd);
}

gboolean lastfm_vfs_feof_impl(VFSFile * file)
{
	LastFM *handle = file->handle;

	return vfs_feof(handle->proxy_fd);
}

gint lastfm_vfs_truncate_impl(VFSFile * file, glong size)
{
	return -1;
}

off_t lastfm_vfs_fsize_impl(VFSFile * file)
{
	return 0;
}

gchar *lastfm_vfs_metadata_impl(VFSFile * file, const gchar * field)
{
	LastFM *handle = file->handle;

#if 0
        g_print("Interesting metadata (want: %s):\n", field);

	if (handle->lastfm_station_name != NULL)
		g_print("%s\n", handle->lastfm_station_name);

	if (handle->lastfm_artist != NULL)
		g_print("%s\n", handle->lastfm_artist);


	if (handle->lastfm_title != NULL)
		g_print("%s\n", handle->lastfm_title);

	g_print("%u\n\n", handle->lastfm_duration);
#endif

	if (!g_ascii_strncasecmp(field, "stream-name", 11) && (handle->lastfm_station_name != NULL))
		return g_strdup(handle->lastfm_station_name);
	if (!g_ascii_strncasecmp(field, "track-name", 10) && (handle->lastfm_title != NULL) && (handle->lastfm_artist != NULL))
		return g_strdup_printf("%s - %s", handle->lastfm_artist, handle->lastfm_title);

	return NULL;
}

VFSConstructor lastfm_const = {
	"lastfm://",
	lastfm_vfs_fopen_impl,
	lastfm_vfs_fclose_impl,
	lastfm_vfs_fread_impl,
	lastfm_vfs_fwrite_impl,
	lastfm_vfs_getc_impl,
	lastfm_vfs_ungetc_impl,
	lastfm_vfs_fseek_impl,
	lastfm_vfs_rewind_impl,
	lastfm_vfs_ftell_impl,
	lastfm_vfs_feof_impl,
	lastfm_vfs_truncate_impl,
	lastfm_vfs_fsize_impl,
	lastfm_vfs_metadata_impl
};

static void init(void)
{       
	vfs_register_transport(&lastfm_const);
}

static void cleanup(void)
{

mowgli_global_storage_free("lastfm_session_id");
mowgli_global_storage_free("lastfm_stream_uri");
#if DEBUG
g_print ("Cleanup finished\n");
#endif
}

DECLARE_PLUGIN(lastfm, init, cleanup, NULL, NULL, NULL, NULL, NULL)
