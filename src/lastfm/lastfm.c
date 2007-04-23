/*  Audacious
 *  Copyright (c) 2007 Cristi Magherusan
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <audacious/vfs.h>
#include <audacious/plugin.h>
#include <audacious/configdb.h>

#include <curl/curl.h>
#include <glib.h>


#define LASTFM_HANDSHAKE_URL "http://ws.audioscrobbler.com/radio/handshake.php?version=1.1.1&platform=linux&username=%s&passwordmd5=%s&debug=0&language=jp"
#define LASTFM_ADJUST_URL "http://ws.audioscrobbler.com/radio/adjust.php?session=%s&url=%s&debug=0"
#define LASTFM_METADATA_URL "http://ws.audioscrobbler.com/radio/np.php?session=%s&debug=0"
#define LASTFM_CURL_TIMEOUT 10


typedef struct
{
	VFSFile *proxy_fd;
	gchar *lastfm_session_id;
	gchar *lastfm_mp3_stream_url;
	gchar *lastfm_station_name;
        gchar *lastfm_artist;
        gchar *lastfm_title;
        gchar *lastfm_album;
        gchar *lastfm_cover;
        int lastfm_duration;
	int login_count;
} LastFM;

LastFM *LastFMGlobalData;
       /*this keeps the login data in a global place
       since we cannot login on every fopen call
       if anyone has a better solution to this any help is welcome */

static size_t lastfm_store_res(void *ptr, size_t size, size_t nmemb, void *udata)
{
	GString *data = (GString *) udata;
	g_string_append_len(data, ptr, nmemb);
	return size * nmemb;
}


static gboolean lastfm_login()
{
	/*gets the session ID in lastfm_session_id and returns the URL to be played
         * read http://gabistapler.de/blog/index.php?/archives/268-Play-last.fm-streams-without-the-player.html for more info
	 */
	gint status, i;
	gchar buf[4096], **split = NULL;
	GString *res = g_string_new(NULL);
	ConfigDb *cfgfile = NULL;
	char *username = NULL, *password = NULL;
	CURL *curl;
	if ((cfgfile = bmp_cfg_db_open()) != NULL)
	{
		bmp_cfg_db_get_string(cfgfile, "audioscrobbler", "username", &username);
		bmp_cfg_db_get_string(cfgfile, "audioscrobbler", "password", &password);
		g_free(cfgfile);
	}
	if (username != NULL && password != NULL)
	{
		snprintf(buf, sizeof(buf), LASTFM_HANDSHAKE_URL, username, password);
		g_free(password);
		g_free(username);
	}
	else
		return FALSE;

	puts("preparing curl");
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Audacious");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_store_res);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LASTFM_CURL_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_URL, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, res);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	puts("curl is done");
	puts(buf);
	puts(res->str);
	if (status == CURLE_OK)
	{
		split = g_strsplit(res->str, "\n", 7);

		for (i = 0; split && split[i]; i++)
		{
			if (g_str_has_prefix(split[i], "session="))
				LastFMGlobalData->lastfm_session_id = g_strndup(split[i] + 8, 32);
			else if (g_str_has_prefix(split[i], "stream_url="))
				LastFMGlobalData->lastfm_mp3_stream_url = g_strdup(split[i] + 11);
		}
	}
	else
		return FALSE;

	//     printf("URL: '%s'\n",LastFMGlobalData->lastfm_mp3_stream_url);
	//     printf("session_id: '%s'\n",LastFMGlobalData->lastfm_session_id);

	g_strfreev(split);
	g_string_erase(res, 0, -1);
	return (gboolean) TRUE;
}

static gboolean lastfm_adjust(const gchar * url)
{
	LastFM *LastFMData = g_new0(LastFM, 1);

	int status, i;
	gchar tmp[4096], **split = NULL;
	gboolean ret = FALSE;
	GString *res = g_string_new(NULL);
	CURL *curl;
	if (LastFMGlobalData->lastfm_session_id == NULL)
		return FALSE;
	snprintf(tmp, sizeof(tmp), LASTFM_ADJUST_URL, LastFMGlobalData->lastfm_session_id, url);
	puts("test1");


	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Audacious");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_store_res);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LASTFM_CURL_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_URL, tmp);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, res);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);


	puts("Adjust received data:");
	puts(res->str);

	if (status == CURLE_OK)
	{
		split = g_strsplit(res->str, "\n", 3);

		for (i = 0; split && split[i]; i++)
		{
			if (g_str_has_prefix(split[i], "response=OK"))
				ret = TRUE;
			if (g_str_has_prefix(split[i], "stationname="))
			{
				LastFMGlobalData->lastfm_station_name = g_strdup(split[i] + 12);
				puts("StationnName:");
				puts(LastFMGlobalData->lastfm_station_name);
			}
		}

		g_strfreev(split);
	}
	g_string_erase(res, 0, -1);

	return ret;
}


static gboolean lastfm_get_metadata( LastFM *handle)
{
    
        gint status, i;
	gchar tmp[4096], **split = NULL;
	gboolean ret = FALSE;
	GString *res = g_string_new(NULL);
	CURL *curl;
	if (handle->lastfm_session_id == NULL)
		return FALSE;
	snprintf(tmp, sizeof(tmp), LASTFM_METADATA_URL, handle->lastfm_session_id);

        curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Audacious");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_store_res);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LASTFM_CURL_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_URL, tmp);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, res);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);


	puts("Received metadata:");
	puts(res->str);

	if (status == CURLE_OK)
	{
		split = g_strsplit(res->str, "\n", 20);

		for (i = 0; split && split[i]; i++)
		{
			if (g_str_has_prefix(split[i], "artist="))
                                handle->lastfm_artist=g_strdup(split[i] + 7);
                        if (g_str_has_prefix(split[i], "title="))
                                handle->lastfm_title=g_strdup(split[i] + 6);
                        if (g_str_has_prefix(split[i], "album="))
                                handle->lastfm_album=g_strdup(split[i] + 6);
                        if (g_str_has_prefix(split[i], "albumcover_medium="))
                                handle->lastfm_cover=g_strdup(split[i] + 18);
                        if (g_str_has_prefix(split[i], "trackduration="))
                                handle->lastfm_duration=atoi(g_strdup(split[i] + 14));
			if (g_str_has_prefix(split[i], "station="))
			{
				handle->lastfm_station_name = g_strdup(split[i] + 8);
				puts("StationnName:");
				puts(handle->lastfm_station_name);
			}
		}

		g_strfreev(split);
	}
	g_string_erase(res, 0, -1);

	return TRUE;
}





VFSFile *lastfm_vfs_fopen_impl(const gchar * path, const gchar * mode)
{
	VFSFile *file;
	LastFM *handle;
	file = g_new0(VFSFile, 1);
	handle = g_new0(LastFM, 1);

	puts("Starting fopen");

	while ((LastFMGlobalData->lastfm_mp3_stream_url == NULL) && (LastFMGlobalData->login_count <= 3))
	{
		printf("Login try count: %d\n", LastFMGlobalData->login_count++);
		lastfm_login();
		if (LastFMGlobalData->lastfm_mp3_stream_url == NULL)
			sleep(5);

	}



	handle->lastfm_mp3_stream_url = g_strdup(LastFMGlobalData->lastfm_mp3_stream_url);
	handle->lastfm_session_id = g_strdup(LastFMGlobalData->lastfm_session_id);
	handle->lastfm_station_name = g_strdup(LastFMGlobalData->lastfm_station_name);

        

	if (lastfm_adjust(path))
                {
        		printf("Tuning completed OK, getting metadata\n");
                        if(lastfm_get_metadata(handle))
                                puts("Successfully fetched the metadata");
                        else
                                puts("Errors were encountered while fetching the metadata");
                }
	else
		puts("Cannot tune to given channel");

	handle->proxy_fd = vfs_fopen(handle->lastfm_mp3_stream_url, mode);
	file->handle = handle;


	puts("Returning from fopen");
	return file;
}

gint lastfm_vfs_fclose_impl(VFSFile * file)
{
	gint ret = 0;


	if (file == NULL)
		return -1;

	LastFM *handle = file->handle;
	ret = vfs_fclose(handle->proxy_fd);


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
	return -1;
}

gchar *lastfm_vfs_metadata_impl(VFSFile * file, const gchar * field)
{
	LastFM *handle = file->handle;
        
        puts("Interesting metadata:");
            puts(handle->lastfm_station_name);
            puts(handle->lastfm_artist);
            puts(handle->lastfm_title);
            printf("%d\n\n",handle->lastfm_duration);


        if (!strcmp(field, "stream-name")&& (handle->lastfm_station_name != NULL))
	        return g_strdup(handle->lastfm_station_name);
        if (!strcmp(field, "track-name") && (handle->lastfm_title != NULL) && (handle->lastfm_artist != NULL))
                return g_strdup_printf("%s - %s", handle->lastfm_artist, handle->lastfm_title);
        if (!strcmp(field, "content-length"))
                return g_strdup_printf("%ld", handle->lastfm_duration);

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
	LastFMGlobalData = g_new0(LastFM, 1);
	vfs_register_transport(&lastfm_const);
}

static void cleanup(void)
{
	g_free(LastFMGlobalData);
#if 0
	vfs_unregister_transport(&default_const);
	vfs_unregister_transport(&file_const);
#endif
}

LowlevelPlugin llp_lastfm = {
	NULL,
	NULL,
	"lastfm:// URI Transport",
	init,
	cleanup,
};

LowlevelPlugin *get_lplugin_info(void)
{
	return &llp_lastfm;
}
