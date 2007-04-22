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


//#define LASTFM_HS_URL "http://ws.audioscrobbler.com/radio/handshake.php"
#define LASTFM_CURL_TIMEOUT 5
//#define DEBUG 1
static gchar *lastfm_session_id = NULL, *lastfm_mp3_stream_url = NULL, *lastfm_station_name = NULL;
CURL *curl = NULL;

static size_t lastfm_store_res(void *ptr, size_t size, size_t nmemb, void *udata)
{
	GString *data = (GString *) udata;
	g_string_append_len(data, ptr, nmemb);
	return size * nmemb;
}

static gboolean setup_curl()
{
	curl = curl_easy_init();
	if (curl != NULL)
	{
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Audacious");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_store_res);
		curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LASTFM_CURL_TIMEOUT);
		return TRUE;
	}
	return FALSE;
}

static gchar *lastfm_login(CURL * curl)
{
	/*gets the session ID in lastfm_session_id and returns the URL to be played
	   make a URI like this one and use curl to open it:

	   http://ws.audioscrobbler.com/radio/handshake.php?
	   version=1.1.1&platform=linux&username=<yourlastfmusername>&
	   passwordmd5=<yourmd5sum>&debug=0&partner=

	   then parse the returned data
	 */

	gint status, i;
	gchar buf[4096], **split = NULL, *lastfm_media_url = NULL;
	GString *res = g_string_new(NULL);
	ConfigDb *cfgfile = NULL;
	char *username = NULL, *password = NULL;

	if ((cfgfile = bmp_cfg_db_open()) != NULL)
	{
		bmp_cfg_db_get_string(cfgfile, "audioscrobbler", "username", &username);
		bmp_cfg_db_get_string(cfgfile, "audioscrobbler", "password", &password);
	}

	if (username != NULL && password != NULL)
		snprintf(buf, sizeof(buf), "http://ws.audioscrobbler.com/radio/handshake.php?version=1.1.1&platform=linux&username=%s&passwordmd5=%s&debug=0&language=jp", username, password);
	else
		return NULL;

	curl_easy_setopt(curl, CURLOPT_URL, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, res);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (status == CURLE_OK)
	{
		split = g_strsplit(res->str, "\n", 7);

		for (i = 0; split && split[i]; i++)
		{
			if (g_str_has_prefix(split[i], "session="))
				lastfm_session_id = g_strndup(split[i] + 8, 32);
			else if (g_str_has_prefix(split[i], "stream_url="))
				lastfm_media_url = g_strdup(split[i] + 11);
		}
	}

	g_strfreev(split);
	g_string_erase(res, 0, -1);
	return lastfm_media_url;
}

static gboolean lastfm_adjust(const gchar * url)
{
	gchar tmp[4096], **split = NULL;
	gboolean ret = FALSE;
	int status, i;

	snprintf(tmp, sizeof(tmp), "http://ws.audioscrobbler.com/radio/adjust.php?session=%s&url=%s&debug=0", lastfm_session_id, url);
	curl_easy_reset(curl);
	setup_curl();
	curl_easy_setopt(curl, CURLOPT_URL, tmp);

	puts("am setat tmp, setez res");

	GString *res = g_string_new(NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, res);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	puts(res->str);
	if (status == CURLE_OK)
	{
		split = g_strsplit(res->str, "\n", 7);

		for (i = 0; split && split[i]; i++)
		{
			if (g_str_has_prefix(split[i], "response=OK"))
				ret = TRUE;
			else if (g_str_has_prefix(split[i], "stationname="))
				lastfm_station_name = g_strdup(split[i] + 12);
		}
	}

	if (ret == TRUE)
		puts("response=OK");

	g_string_erase(res, 0, -1);

	return ret;
}

/* TODO: it may be a good idea to store last.fm auth data here instead of in
 *       the global scope. --nenolod
 */
typedef struct {
	VFSFile *proxy_fd;
} LastFMFile;

VFSFile *lastfm_vfs_fopen_impl(const gchar * path, const gchar * mode)
{
	VFSFile *file;
	LastFMFile *handle;

	if (!lastfm_adjust(path))
		puts("Cannot tune to given channel");
	else
		puts("Adjust completed OK");

	file = g_new0(VFSFile, 1);
	handle = g_new0(LastFMFile, 1);

	puts("Opening stream with vfs_fopen");
	handle->proxy_fd = vfs_fopen(lastfm_mp3_stream_url, mode);
	file->handle = handle;

	return file;
}

gint lastfm_vfs_fclose_impl(VFSFile * file)
{
	gint ret = 0;
	LastFMFile *handle;

	if (file == NULL)
		return -1;

	handle = file->handle;

	ret = vfs_fclose(handle->proxy_fd);
	g_free(handle);

	return ret;
}

size_t lastfm_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
	LastFMFile *handle = file->handle;
	size_t ret = vfs_fread(ptr, size, nmemb, handle->proxy_fd);

	return ret;
}

size_t lastfm_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
	return -1;
}

gint lastfm_vfs_getc_impl(VFSFile * stream)
{
	LastFMFile *handle = stream->handle;

	return vfs_getc(handle->proxy_fd);
}

gint lastfm_vfs_ungetc_impl(gint c, VFSFile * stream)
{
	LastFMFile *handle = stream->handle;

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
	LastFMFile *handle = file->handle;

	return vfs_ftell(handle->proxy_fd);
}

gboolean lastfm_vfs_feof_impl(VFSFile * file)
{
	LastFMFile *handle = file->handle;

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

gchar *lastfm_vfs_metadata_impl(VFSFile * file, const gchar *key)
{
	if (!strcasecmp(key, "stream-name"))
		return g_strdup(lastfm_station_name);

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
	setup_curl();
	lastfm_mp3_stream_url = lastfm_login(curl);
	puts(lastfm_mp3_stream_url);
	puts(lastfm_session_id);

	vfs_register_transport(&lastfm_const);
}

static void cleanup(void)
{
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
