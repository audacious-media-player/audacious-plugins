/*  Audacious
 *  Copyright (c) 2007 Daniel Barkalow
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

#include <curl/curl.h>

#include <string.h>

#define BUFFER_SIZE 256 * 1024

typedef struct _CurlHandle CurlHandle;

struct _CurlHandle {
  CURL *curl;

  gsize length; // the length of the file
  gsize rd_abs; // the absolute position for reading from the stream
  gsize wr_abs; // the absolute position where the input connection is

  gsize buffer_length;
  gchar *buffer;

  gsize rd_index;
  gsize wr_index;

  gboolean no_data; // true if we're only looking for length currently
  gboolean cancel; // true if the thread should be cancelled
  GThread *thread; // the thread that's reading from the connection
};

/* TODO:
 *  - Icecast
 *  - Clever buffer stuff when you read a bit of the beginning and a bit of the
 *    end of a file
 */

/* The goal here is to have a buffering system which handles the following:
 *  1) open, seek, read (without fetching the beginning of the file)
 *  2) open, seek END, tell (using HEAD only)
 *  3) open, read, seek 0, read (without restarting fetch)
 */

static size_t buf_space(CurlHandle *handle)
{
  size_t rd_edge = handle->rd_abs - 2048;
  if (rd_edge < 0)
    rd_edge = 0;
  size_t buffer_limit = handle->buffer_length - 
    (handle->wr_abs - rd_edge);
  size_t cont_limit = handle->buffer_length - handle->wr_index;
  return buffer_limit < cont_limit ? buffer_limit : cont_limit;
}

static size_t buf_available(CurlHandle *handle)
{
  size_t buffer_limit = handle->wr_abs - handle->rd_abs;
  size_t cont_limit = handle->buffer_length - handle->rd_index;
  if (buffer_limit <= 0)
    return 0;
  return buffer_limit < cont_limit ? buffer_limit : cont_limit;
}

static void check(CurlHandle *handle)
{
  if (!((handle->wr_abs - handle->wr_index) % handle->buffer_length ==
	(handle->rd_abs - handle->rd_index) % handle->buffer_length))
    printf("%p Not aligned! wr %d rd %d\n", handle,
	   (handle->wr_abs - handle->wr_index) % handle->buffer_length,
	   (handle->rd_abs - handle->rd_index) % handle->buffer_length);
}

static void update_length(CurlHandle *handle)
{
  if (handle->length == -1)
    {
      double value;
      int retcode =
	curl_easy_getinfo(handle->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, 
			  &value);
      if (retcode == CURLE_OK)
	{
	  handle->length = value;
	}
      else
	{
	  g_print("getinfo gave error\n");
	}
    }
}

#define PROBE 262140

static size_t curl_writecb(void *ptr, size_t size, size_t nmemb, void *stream)
{
  CurlHandle *handle = stream;
  gint sz = size * nmemb;
  gint ret = 0;
  gint trans;

  update_length(handle);

  while (ret < sz)
    {
      while (!(trans = buf_space(handle)) && !handle->cancel)
	{
	  g_usleep(10000);
	  //g_print("Wait for free space\n");
	}
      if (handle->cancel)
	break;
      if (trans > sz - ret)
	trans = sz - ret;
      memcpy(handle->buffer + handle->wr_index, ptr + ret, trans);

      handle->wr_abs += trans;
      handle->wr_index = (handle->wr_index + trans) % handle->buffer_length;
      ret += trans;
    }
  return ret;
}

static gpointer
curl_manage_request(gpointer arg)
{
  CurlHandle *handle = arg;
  //g_print("Connect %p\n", handle);

  if (handle->no_data)
    curl_easy_setopt(handle->curl, CURLOPT_NOBODY, 1);
  else
    {
      //g_print("Start from %d\n", handle->wr_abs);
      curl_easy_setopt(handle->curl, CURLOPT_RESUME_FROM, handle->wr_abs);

      curl_easy_setopt(handle->curl, CURLOPT_NOBODY, 0);
      curl_easy_setopt(handle->curl, CURLOPT_HTTPGET, 1);
    }

  curl_easy_perform(handle->curl);
  update_length(handle);
  //g_print("Done %p%s", handle, handle->cancel ? " (aborted)\n" : "\n");
  handle->cancel = 1;
  return NULL;
}

static void curl_req_xfer(CurlHandle *handle)
{
  if (!handle->thread)
    {
      handle->cancel = 0;
      handle->wr_index = 0;
      handle->rd_index = 0;
      handle->wr_abs = handle->rd_abs;
      //g_print("Starting connection %p at %d\n", handle, handle->wr_abs);
      handle->thread = g_thread_create(curl_manage_request, handle, 
				       TRUE, NULL);
    }
}

static void curl_req_sync_xfer(CurlHandle *handle, size_t old_rd_abs)
{
  handle->rd_index = (handle->rd_index + handle->rd_abs - old_rd_abs +
		      handle->buffer_length) % handle->buffer_length;
}

static void curl_req_no_xfer(CurlHandle *handle)
{
  if (handle->thread)
    {
      handle->cancel = 1;
      g_thread_join(handle->thread);
      handle->thread = NULL;
      handle->cancel = 0;
    }
}

VFSFile *
curl_vfs_fopen_impl(const gchar * path,
		    const gchar * mode)
{
  gchar *url = g_malloc(strlen(path) + strlen("http://") + 1);
  CurlHandle *handle;
  VFSFile *file;
  if (!path || !mode)
    return NULL;

  sprintf(url, "http://%s", path);

  file = g_new(VFSFile, 1);

  handle = g_new(CurlHandle, 1);
  handle->curl = curl_easy_init();
  handle->rd_index = 0;
  handle->wr_index = 0;
  handle->rd_abs = 0;
  handle->wr_abs = 0;
  handle->buffer_length = BUFFER_SIZE;
  handle->buffer = g_malloc(handle->buffer_length);
  handle->thread = NULL;
  handle->length = -1;
  handle->cancel = 0;
  handle->no_data = 0;
  curl_easy_setopt(handle->curl, CURLOPT_URL, url);
  curl_easy_setopt(handle->curl, CURLOPT_WRITEFUNCTION, curl_writecb);
  curl_easy_setopt(handle->curl, CURLOPT_WRITEDATA, handle);

  file->handle = handle;

  //g_print("Open %s with curl => %p\n", url, handle);

  return file;
}

gint
curl_vfs_fclose_impl(VFSFile * file)
{
  gint ret = 0;
  if (file == NULL)
    return -1;
  //g_print("Close %p\n", file->handle);
  if (file->handle)
    {
      CurlHandle *handle = file->handle;
      //g_print("Cancel transfer\n");
      curl_req_no_xfer(handle);
      //g_print("Okay\n");

      g_free(handle->buffer);
      curl_easy_cleanup(handle->curl);
      g_free(handle);
    }
  return ret;
}

size_t
curl_vfs_fread_impl(gpointer ptr,
		    size_t size,
		    size_t nmemb,
		    VFSFile * file)
{
  CurlHandle *handle = file->handle;
  ssize_t sz = size * nmemb;
  size_t ret = 0;

  if (sz < 0)
    return 0;

  //g_print("Reading %d*%d=%d from %p\n", size, nmemb, sz, handle);

  curl_req_xfer(handle);

  check(handle);

  while (ret < sz)
    {
      size_t available;
      while (!(available = buf_available(handle)) && 
	     (handle->length == -1 || handle->rd_abs < handle->length) &&
	     !handle->cancel)
	g_usleep(10000);
      if (available > sz - ret)
	available = sz - ret;
      memcpy(ptr + ret, handle->buffer + handle->rd_index, available);

      handle->rd_index = 
	(handle->rd_index + available) % handle->buffer_length;
      handle->rd_abs += available;
      ret += available;
      if (!available)
	{
	  //g_print("EOF reading from %p\n", handle);
	  break;
	}
    }

  //g_print("Read %d from %p\n", ret, handle);

  return ret;
}

size_t
curl_vfs_fwrite_impl(gconstpointer ptr,
		     size_t size,
		     size_t nmemb,
		     VFSFile * file)
{
  return 0;
}

gint
curl_vfs_getc_impl(VFSFile *stream)
{
  gchar c;
  if (curl_vfs_fread_impl(&c, 1, 1, stream) != 1)
    return -1;
  return c;
}

gint
curl_vfs_ungetc_impl(gint c, VFSFile *stream)
{
  g_print("Tried ungetc\n");
  return -1;
}

gint
curl_vfs_fseek_impl(VFSFile * file,
		    glong offset,
		    gint whence)
{
  size_t posn;
  CurlHandle *handle = file->handle;
  //g_print("Seek %p to %d %d\n", handle, offset, whence);
  if (whence == SEEK_END && handle->length == -1)
    {
      if (!handle->thread)
	{
	  // We need a HEAD to find out the length
	  handle->no_data = 1;
	  //g_print("Request for head info\n");
	  curl_manage_request(handle);
	  //g_print("Completed\n");
	  handle->no_data = 0;
	}
      else
	{
	  // Wait a bit?
	}
    }

  if (whence == SEEK_END && handle->length == -1)
    {
      //g_print("Tried to seek to the end of a file with unknown length\n");
      // don't know how long it is...
      return -1;
    }

  posn = handle->rd_abs;

  if (whence == SEEK_SET)
    handle->rd_abs = offset;
  else if (whence == SEEK_END)
    handle->rd_abs = handle->length + offset;
  else
    handle->rd_abs = handle->rd_abs + offset;

  // XXXX
  // There's a race here between finding available space and
  // allocating it and the check below.

  if (handle->thread)
    {
      if (handle->rd_abs + handle->buffer_length < handle->wr_abs ||
	  handle->rd_abs > handle->wr_abs)
	{
	  //g_print("Stop transfer\n");
	  curl_req_no_xfer(handle);
	  //g_print("Okay\n");
	}
      else
	{
	  //g_print("Continue transfer\n");
	  curl_req_sync_xfer(handle, posn);
	}
    }

  //g_print("Seeked %p from %d to %d\n", handle, posn, handle->rd_abs);
  return 0;
}

void
curl_vfs_rewind_impl(VFSFile * file)
{
  curl_vfs_fseek_impl(file, 0, SEEK_SET);
}

glong
curl_vfs_ftell_impl(VFSFile * file)
{
  CurlHandle *handle = file->handle;
  return handle->rd_abs;
}

gboolean
curl_vfs_feof_impl(VFSFile * file)
{
  CurlHandle *handle = file->handle;
  return handle->rd_abs == handle->length;
}

gint
curl_vfs_truncate_impl(VFSFile * file, glong size)
{
  return -1;
}

VFSConstructor curl_const = {
  "http://",
  curl_vfs_fopen_impl,
  curl_vfs_fclose_impl,
  curl_vfs_fread_impl,
  curl_vfs_fwrite_impl,
  curl_vfs_getc_impl,
  curl_vfs_ungetc_impl,
  curl_vfs_fseek_impl,
  curl_vfs_rewind_impl,
  curl_vfs_ftell_impl,
  curl_vfs_feof_impl,
  curl_vfs_truncate_impl
};

static void init(void)
{
  vfs_register_transport(&curl_const);
}

static void cleanup(void)
{
#if 0
  vfs_unregister_transport(&curl_const);
#endif
}

LowlevelPlugin llp_curl = {
  NULL,
  NULL,
  "http:// URI Transport",
  init,
  cleanup,
};

LowlevelPlugin *get_lplugin_info(void)
{
  return &llp_curl;
}
