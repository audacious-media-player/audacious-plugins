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
#include "lastfm.h"


LastFM *LastFMGlobalData;
        /*this keeps the login data in a global place since 
         * we cannot login on every fopen call* if anyone 
         * has a better solution to this any hint is welcome */

static size_t lastfm_store_res(void *ptr, size_t size, size_t nmemb, void *udata)
{
	GString *data = (GString *) udata;
	g_string_append_len(data, ptr, nmemb);
	return size * nmemb;
}

gint get_data_from_url(gchar buf[4096],GString* res )
{
        CURL*   curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Audacious");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_store_res);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LASTFM_CURL_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_URL, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, res);
	gint status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
return status;
}

static gboolean lastfm_login(void)
{
	/*gets the session ID in lastfm_session_id and returns the URL to be played
	 * read http://gabistapler.de/blog/index.php?/archives/268-Play-last.fm-streams-without-the-player.html for more info
	 */
	gint status, i;
	gchar buf[4096], **split = NULL;
	GString *res = g_string_new(NULL);
	ConfigDb *cfgfile = NULL;
	char *username = NULL, *password = NULL;
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

	status=get_data_from_url(buf,res);

#ifdef DEBUG
	g_print("URL:%s\n",buf);
	g_print("Downloaded data:%s\n",res->str);
#endif
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

	g_strfreev(split);
	g_string_erase(res, 0, -1);
	return (gboolean) TRUE;
}

static gboolean lastfm_adjust(const gchar * url)
{
	int status, i;
	gchar tmp[4096], **split = NULL;
	gboolean ret = FALSE;
	GString *res = g_string_new(NULL);
	
	if (LastFMGlobalData->lastfm_session_id == NULL)
		return FALSE;
	snprintf(tmp, sizeof(tmp), LASTFM_ADJUST_URL, LastFMGlobalData->lastfm_session_id, url);

        status=get_data_from_url(tmp,res);

#ifdef DEBUG
	g_print("Adjust received data:%s\n",res->str);
#endif
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
#ifdef DEBUG
				g_print("StationnName:%s\n",LastFMGlobalData->lastfm_station_name);
                                
#endif
			}
		}

		g_strfreev(split);
	}
	g_string_erase(res, 0, -1);

	return ret;
}


static gboolean lastfm_get_metadata(LastFM * handle)
{

	gint status, i;
	gchar tmp[4096], **split = NULL;
	GString *res = g_string_new(NULL);
	
	if (handle->lastfm_session_id == NULL)
		return FALSE;
	snprintf(tmp, sizeof(tmp), LASTFM_METADATA_URL, handle->lastfm_session_id);

        status=get_data_from_url(tmp,res);

#ifdef DEBUG
	g_print("Received metadata:%s\n",res->str);
#endif

	if (status == CURLE_OK)
	{
		split = g_strsplit(res->str, "\n", 20);

		for (i = 0; split && split[i]; i++)
		{
			if (g_str_has_prefix(split[i], "artist="))
				handle->lastfm_artist = g_strdup(split[i] + 7);
			if (g_str_has_prefix(split[i], "title="))
				handle->lastfm_title = g_strdup(split[i] + 6);
			if (g_str_has_prefix(split[i], "album="))
				handle->lastfm_album = g_strdup(split[i] + 6);
			if (g_str_has_prefix(split[i], "albumcover_medium="))
				handle->lastfm_cover = g_strdup(split[i] + 18);
			if (g_str_has_prefix(split[i], "trackduration="))
				handle->lastfm_duration = g_ascii_strtoull(g_strdup(split[i] + 14),NULL,10);
			if (g_str_has_prefix(split[i], "station="))
			{
				handle->lastfm_station_name = g_strdup(split[i] + 8);
				g_print("Station Name: %s\n", handle->lastfm_station_name);
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
	
        while ((LastFMGlobalData->lastfm_mp3_stream_url == NULL) && (LastFMGlobalData->login_count <= 3))
	{
		printf("Login try count: %d\n", LastFMGlobalData->login_count++);
		lastfm_login();
		if (LastFMGlobalData->lastfm_mp3_stream_url == NULL)
			sleep(5);
	}

	if (LastFMGlobalData->lastfm_mp3_stream_url == NULL)
		return NULL;

	handle->lastfm_mp3_stream_url = g_strdup(LastFMGlobalData->lastfm_mp3_stream_url);
	handle->lastfm_session_id = g_strdup(LastFMGlobalData->lastfm_session_id);
	handle->lastfm_station_name = g_strdup(LastFMGlobalData->lastfm_station_name);

	if (lastfm_adjust(path))
	{
		gint ret;
           	ret = lastfm_get_metadata(handle);

#ifdef DEBUG
		g_print("Tuning completed OK, getting metadata\n");
		if (ret)
			g_print("Successfully fetched the metadata\n");
		else
			g_print("Errors were encountered while fetching the metadata\n");
#endif
	}
#ifdef DEBUG
	else
		g_print("Cannot tune to given channel\n");
#endif

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

#ifdef DEBUG
	g_print("Interesting metadata:\n");

	if (handle->lastfm_station_name != NULL)
		g_print("%s\n",handle->lastfm_station_name);

	if (handle->lastfm_artist != NULL)
	g_print("%s\n",handle->lastfm_artist);

	if (handle->lastfm_title != NULL)
	g_print("%s\n",handle->lastfm_title);

	g_print("%ul\n\n", handle->lastfm_duration);
#endif

	if (!g_ascii_strncasecmp(field, "stream-name",12) && (handle->lastfm_station_name != NULL))
		return g_strdup(handle->lastfm_station_name);
	if (!g_ascii_strncasecmp(field, "track-name",11) && (handle->lastfm_title != NULL) && (handle->lastfm_artist != NULL))
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
	LastFMGlobalData = g_new0(LastFM, 1);
	vfs_register_transport(&lastfm_const);
}

static void cleanup(void)
{
	g_free(LastFMGlobalData);
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
