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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <audacious/vfs.h>
#include <audacious/plugin.h>
#include <audacious/configdb.h>

#include <curl/curl.h>

#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 256 * 1024
#define REVERSE_SEEK_SIZE 2048

#define DEBUG_CONNECTION 0
#define DEBUG_OPEN_CLOSE 1
#define DEBUG_SEEK 0
#define DEBUG_READ 0
#define DEBUG_HEADERS 0
#define DEBUG_ICY 0
#define DEBUG_ICY_WRAP 0
#define DEBUG_ICY_VERBOSE 0
#define DEBUG_METADATA_REPORT 0

typedef struct _CurlHandle CurlHandle;

struct _CurlHandle {
  CURL *curl;

  gssize length; // the length of the file
  gssize rd_abs; // the absolute position for reading from the stream
  gssize wr_abs; // the absolute position where the input connection is

  gssize icy_left;
  gssize icy_interval;
  gint in_icy_meta; // 0=no, 1=before size, 2=in data
  gssize meta_abs; // the absolute position where the metadata changes

  gssize buffer_length;
  gchar *buffer;

  gssize rd_index;
  gssize wr_index;

  gssize hdr_index;

  GSList *stream_stack; // stack for stream functions (getc, ungetc)

  gboolean header; // true if we haven't finished the header yet
  gboolean no_data; // true if we're only looking for length currently
  gboolean cancel; // true if the thread should be cancelled
  gboolean failed; // true if we've tried and failed already
  GThread *thread; // the thread that's reading from the connection

  VFSFile *download; // file to write to as we download

  gchar *name;
  gchar *title;

  struct {
    gchar *proxy_host;
    gchar *proxy_auth;
  } proxy_info;
  
  gchar *local_ip;
};

void curl_vfs_rewind_impl(VFSFile * file);
glong curl_vfs_ftell_impl(VFSFile * file);
gboolean curl_vfs_feof_impl(VFSFile * file);
gint curl_vfs_truncate_impl(VFSFile * file, glong size);
gchar *curl_vfs_metadata_impl(VFSFile * file, const gchar * field);
size_t curl_vfs_fwrite_impl(gconstpointer ptr, size_t size,
		     size_t nmemb,
		     VFSFile * file);
size_t curl_vfs_fread_impl(gpointer ptr_, size_t size,
		     size_t nmemb,
		     VFSFile * file);
gint curl_vfs_fclose_impl(VFSFile * file);
gint curl_vfs_getc_impl(VFSFile *stream);
gint curl_vfs_ungetc_impl(gint c, VFSFile *stream);
gint curl_vfs_fseek_impl(VFSFile * file, glong offset, gint whence);
VFSFile *curl_vfs_fopen_impl(const gchar * path,
		    const gchar * mode);
LowlevelPlugin *get_lplugin_info(void);

VFSConstructor curl_const;

/* TODO:
 *  - Fix hang if the server closes the connection in the middle
 *  - Clever buffer stuff when you read a bit of the beginning and a bit of the
 *    end of a file
 */

/* The goal here is to have a buffering system which handles the following:
 *  1) open, seek, read (without fetching the beginning of the file)
 *  2) open, seek END, tell (using HEAD only)
 *  3) open, read, seek 0, read (without restarting fetch)
 */

static ssize_t buf_space(CurlHandle *handle)
{
  ssize_t rd_edge = handle->rd_abs - REVERSE_SEEK_SIZE;
  ssize_t buffer_limit;
  ssize_t cont_limit;
  if (rd_edge < 0)
    rd_edge = 0;
  buffer_limit = handle->buffer_length - 
    (handle->wr_abs - rd_edge);
  cont_limit = handle->buffer_length - handle->wr_index;
  if (cont_limit < buffer_limit)
    buffer_limit = cont_limit;
  if (handle->icy_interval && handle->icy_left)
    {
      if (handle->icy_left < buffer_limit)
	buffer_limit = handle->icy_left;
    }
  return buffer_limit;
}

static size_t buf_available(CurlHandle *handle)
{
  size_t buffer_limit;
  size_t cont_limit;
  if (handle->header)
    return 0;
  buffer_limit = handle->wr_abs - handle->rd_abs;
  cont_limit = handle->buffer_length - handle->rd_index;
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
	  if (handle->length == 0)
	    handle->length = -2;
	  //g_print("Length: %d\n", handle->length);
	}
      else
	{
	  handle->length = -2;
	  g_print("getinfo gave error\n");
	}
    }
}

#define PROBE 262140

#define ICE_NAME "ice-name:"
#define ICY_NAME "icy-name:"
#define ICY_METAINT "icy-metaint:"

static gboolean match_header(CurlHandle *handle, size_t size,
			     const char *header)
{
  if (strlen(header) > size)
    return FALSE;
  // XXXX wrapped headers
  return !(strncmp(handle->buffer + handle->hdr_index,
		   header, strlen(header)));
}

static gchar *get_value(CurlHandle *handle, size_t size, const char *header)
{
  // XXXX wrapped headers
  return g_strdup(handle->buffer + 
		(handle->hdr_index + strlen(header)) % handle->buffer_length);
}

static void got_header(CurlHandle *handle, ssize_t size)
{
  if (DEBUG_HEADERS)
    g_print("Got header %d bytes\n", size);
  if (match_header(handle, size, ICY_NAME))
    {
      handle->name = get_value(handle, size, ICY_NAME);
      if (DEBUG_HEADERS)
	{
	  g_print("Stream name: %s\n", handle->name);
	}
    }
  if (match_header(handle, size, ICE_NAME))
    {
      handle->name = get_value(handle, size, ICE_NAME);
      if (DEBUG_HEADERS)
	{
	  g_print("Stream name: %s\n", handle->name);
	}
    }
  if (match_header(handle, size, ICY_METAINT))
    {
      gchar *value = get_value(handle, size, ICY_METAINT);
      handle->icy_interval = atoi(value);
      g_free(value);
      if (DEBUG_HEADERS)
	g_print("Metadata interval: %d\n", handle->icy_interval);
    }
}

#define TITLE_INLINE "StreamTitle="

static gboolean match_inline(CurlHandle *handle, size_t posn, 
			     const char *name)
{
  size_t len = strlen(name);
  size_t i;
  if (DEBUG_ICY_WRAP)
    g_print("Posn=%d\n", posn);
  if (DEBUG_ICY_WRAP && posn + len > handle->buffer_length)
    g_print("Wrapped inline key\n");
  if (((handle->wr_index - posn + handle->buffer_length) %
       handle->buffer_length) <= len)
    return FALSE;
  for (i = 0; i < len; i++)
    if (handle->buffer[(posn + i) % handle->buffer_length] != name[i])
      {
	return FALSE;
      }
  return TRUE;
}

static gchar *get_inline_value(CurlHandle *handle, size_t posn,
			       const char *name)
{
  size_t end;
  size_t sz;
  gchar *ret;
  posn = (posn + strlen(name) + 1) % handle->buffer_length;
  end = posn;
  while (handle->buffer[end % handle->buffer_length] != ';')
    end++;
  sz = (end - posn + handle->buffer_length) % handle->buffer_length;
  ret = g_malloc(sz);
  if (end % handle->buffer_length < posn % handle->buffer_length)
    {
      size_t prewrap = handle->buffer_length - posn;
      memcpy(ret, handle->buffer + posn, prewrap);
      memcpy(ret + prewrap, handle->buffer, sz - prewrap);
      if (DEBUG_ICY_WRAP)
	g_print("Wrapped inline metadata value\n");
    }
  else
    {
      memcpy(ret, handle->buffer + posn, sz);
    }
  ret[sz - 1] = '\0';
  return ret;
}

static void got_inline_metadata(CurlHandle *handle)
{
  size_t i = (handle->hdr_index + 1) % handle->buffer_length;
  if (match_inline(handle, i, TITLE_INLINE))
    {
      if (handle->title)
	g_free(handle->title);
      handle->title = get_inline_value(handle, i, TITLE_INLINE);
      if (DEBUG_ICY)
	g_print("Title: '%s'\n", handle->title);
    }
  handle->meta_abs = handle->wr_abs;
}

static size_t curl_writecb(unsigned char *ptr, size_t size, size_t nmemb, void *stream)
{
  CurlHandle *handle = stream;
  gint sz = size * nmemb;
  gint ret = 0;
  gint trans;

  if (!handle->header)
    update_length(handle);

  while (ret < sz)
    {
      while (!(trans = buf_space(handle)) && !handle->cancel)
	{
	  //g_print("Wait for free space on %p\n", handle);
	  g_usleep(10000);
	}
      if (handle->cancel)
	break;
      if (trans > sz - ret)
	trans = sz - ret;
      memcpy(handle->buffer + handle->wr_index, ptr + ret, trans);

      if (!handle->header)
	{
	  if (handle->icy_interval)
	    handle->icy_left -= trans;
	  if (!handle->in_icy_meta)
	    {
	      handle->wr_abs += trans;
	      if (handle->download)
		{
		  vfs_fwrite(ptr + ret, trans, 1, handle->download);
		}
	      if (handle->icy_interval && !handle->icy_left)
		{
		  if (DEBUG_ICY)
		    g_print("Metadata inline after %d\n", handle->wr_abs);
		  handle->in_icy_meta = 1;
		  handle->icy_left = 1;
		}
	    }
	  else if (handle->in_icy_meta == 1)
	    {
	      // Track where the header insert starts
	      handle->hdr_index = handle->wr_index;
	      handle->icy_left = 
		((unsigned char)(handle->buffer[handle->wr_index])) * 16;
	      if (DEBUG_ICY)
		g_print("Metadata of size %d\n", handle->icy_left);
	      if (handle->icy_left)
		{
		  handle->in_icy_meta = 2;
		}
	      else
		{
		  handle->in_icy_meta = 0;
		  handle->icy_left = handle->icy_interval;
		  handle->wr_index--;
		}
	    }
	  else
	    {
	      if (!handle->icy_left)
		{
		  handle->wr_index = (handle->wr_index + trans) % 
		    handle->buffer_length;
		  if (DEBUG_ICY_VERBOSE)
		    {
		      if (handle->wr_index < handle->hdr_index)
			{
			  // wrapped
			  fwrite(handle->buffer + handle->hdr_index + 1,
				 handle->buffer_length - handle->hdr_index - 1,
				 1, stdout);
			  fwrite(handle->buffer, handle->wr_index, 1, stdout);
			}
		      else
			{
			  fwrite(handle->buffer + handle->hdr_index, 
				 handle->wr_index - handle->hdr_index, 1, 
				 stdout);
			}
		      g_print("\n");
		    }
		    got_inline_metadata(handle);

		  // Rewind the buffer usage to write over the
		  // metadata with content. -trans because we're about
		  // to add it.
		  handle->wr_index = handle->hdr_index - trans;
		  handle->in_icy_meta = 0;
		  handle->icy_left = handle->icy_interval;
		}
	    }
	}
      handle->wr_index = (handle->wr_index + trans) % handle->buffer_length;
      ret += trans;

      if (handle->header)
	{
	  gssize i = handle->hdr_index;
	  while (1)
	    {
	      if ((i + 1) % handle->buffer_length == handle->wr_index)
		break;
	      if (handle->buffer[i] == '\r' &&
		  handle->buffer[(i + 1) % handle->buffer_length] == '\n')
		{
		  gssize size_ = (handle->buffer_length + i - 
				handle->hdr_index) % handle->buffer_length;
		  handle->buffer[i] = '\0';
		  got_header(handle, size_);
		  if (i == handle->hdr_index)
		    {
		      size_t leftover;
		      // Empty header means the end of the headers
		      handle->header = 0;
		      handle->hdr_index = (i + 2) % handle->buffer_length;
		      // We read from the start of the data in the request
		      handle->rd_index = handle->hdr_index;
		      // We've already written the amount that's after
		      // the header.
		      leftover = (handle->wr_index - handle->hdr_index + handle->buffer_length) % handle->buffer_length;
		      handle->wr_abs += leftover;
		      if (handle->download)
			{
			  // the data which has to go into the
			  // beginning of the file must be at the end
			  // of the input that we've dealt with.
			  vfs_fwrite(ptr + ret - leftover, leftover, 1, 
				     handle->download);
			}
		      handle->icy_left = handle->icy_interval;
		      if (handle->icy_interval)
			{
			  handle->icy_left -=
			    (handle->wr_index - handle->hdr_index + handle->buffer_length) % handle->buffer_length;
			}
		      if (DEBUG_ICY)
			g_print("Left %d\n", handle->icy_left);
		      handle->in_icy_meta = 0;
		      break;
		    }
		  handle->hdr_index = (i + 2) % handle->buffer_length;
		}
	      i = (i + 1) % handle->buffer_length;
	    }
	}
    }
  return ret;
}

static gpointer
curl_manage_request(gpointer arg)
{
  CurlHandle *handle = arg;
  CURLcode result;
  if (DEBUG_CONNECTION)
    g_print("Connect %p\n", handle);

  if (handle->no_data)
    curl_easy_setopt(handle->curl, CURLOPT_NOBODY, 1);
  else
    {
      if (DEBUG_CONNECTION)
	g_print("Start from %d\n", handle->wr_abs);
      curl_easy_setopt(handle->curl, CURLOPT_RESUME_FROM, handle->wr_abs);

      curl_easy_setopt(handle->curl, CURLOPT_NOBODY, 0);
      curl_easy_setopt(handle->curl, CURLOPT_HTTPGET, 1);
    }

  handle->header = 1;
  handle->hdr_index = 0;
  handle->icy_interval = 0;

  result = curl_easy_perform(handle->curl);
  if (result == CURLE_OK)
    update_length(handle);
  // We expect to get CURLE_WRITE_ERROR if we cancel.
  // We get CURLE_GOT_NOTHING if we send a HEAD request to a shoutcast server.
  // We get CURLE_HTTP_RANGE_ERROR if we try to use range with shoutcast.
  if (result != CURLE_OK && result != CURLE_WRITE_ERROR && 
      result != CURLE_GOT_NOTHING && result != CURLE_HTTP_RANGE_ERROR &&
      result != CURLE_PARTIAL_FILE)
    {
      g_print("Got curl error %d: %s\n", result, curl_easy_strerror(result));
      handle->failed = 1;
    }
  if (DEBUG_CONNECTION)
    g_print("Done %p%s", handle, handle->cancel ? " (aborted)\n" : "\n");
  handle->cancel = 1;
  return NULL;
}

static void curl_req_xfer(CurlHandle *handle)
{
  if (handle->failed)
    {
      handle->cancel = 1;
      return;
    }
  if (!handle->thread)
    {
      handle->cancel = 0;
      handle->wr_index = 0;
      handle->rd_index = 0;
      handle->wr_abs = handle->rd_abs;
      if (DEBUG_CONNECTION)
	g_print("Starting connection %p at %d\n", handle, handle->wr_abs);
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
  gchar *url;
  CurlHandle *handle;
  VFSFile *file;
  if (!path || !mode)
    return NULL;

  url = g_strdup(path);

  file = g_new0(VFSFile, 1);

  handle = g_new0(CurlHandle, 1);
  handle->curl = curl_easy_init();
  handle->rd_index = 0;
  handle->wr_index = 0;
  handle->meta_abs = 0;
  handle->rd_abs = 0;
  handle->wr_abs = 0;
  handle->buffer_length = BUFFER_SIZE;
  handle->buffer = g_malloc(handle->buffer_length);
  handle->thread = NULL;
  handle->length = -1;
  handle->cancel = 0;
  handle->failed = 0;
  handle->no_data = 0;
  handle->stream_stack = NULL;

  curl_easy_setopt(handle->curl, CURLOPT_URL, url);
  curl_easy_setopt(handle->curl, CURLOPT_WRITEFUNCTION, curl_writecb);
  curl_easy_setopt(handle->curl, CURLOPT_WRITEDATA, handle);
  curl_easy_setopt(handle->curl, CURLOPT_HEADERDATA, handle);

  curl_easy_setopt(handle->curl, CURLOPT_CONNECTTIMEOUT, 3);

  curl_easy_setopt(handle->curl, CURLOPT_SSL_VERIFYPEER, 0);
  curl_easy_setopt(handle->curl, CURLOPT_SSL_VERIFYHOST, 0);

  curl_easy_setopt(handle->curl, CURLOPT_FOLLOWLOCATION, 1);

  {
    gboolean tmp = FALSE;
    ConfigDb *db;

    db = bmp_cfg_db_open();

    bmp_cfg_db_get_bool(db, NULL, "use_local_ip", &tmp);
    if (tmp == TRUE)
    {
      bmp_cfg_db_get_string(db, NULL, "local_ip", &handle->local_ip);
      curl_easy_setopt(handle->curl, CURLOPT_INTERFACE, handle->local_ip);
    }
    tmp = FALSE;

    bmp_cfg_db_get_bool(db, NULL, "use_proxy", &tmp);
    if (tmp == TRUE)
    {
      gint proxy_port = 0;

      bmp_cfg_db_get_string(db, NULL, "proxy_host", 
        &handle->proxy_info.proxy_host);
      bmp_cfg_db_get_int(db, NULL, "proxy_port", &proxy_port);

      curl_easy_setopt(handle->curl, CURLOPT_PROXY, handle->proxy_info.proxy_host);
      curl_easy_setopt(handle->curl, CURLOPT_PROXYPORT, proxy_port);

      tmp = FALSE;

      bmp_cfg_db_get_bool(db, NULL, "proxy_use_auth", &tmp);
      if (tmp == TRUE)
      {
        gchar *proxy_user = NULL, *proxy_pass = NULL;

        bmp_cfg_db_get_string(db, NULL, "proxy_user", &proxy_user);
        bmp_cfg_db_get_string(db, NULL, "proxy_pass", &proxy_pass);

        handle->proxy_info.proxy_auth = g_strdup_printf("%s:%s",
          proxy_user != NULL ? proxy_user : "", 
          proxy_pass != NULL ? proxy_pass : "");

        curl_easy_setopt(handle->curl, CURLOPT_PROXYUSERPWD, 
	  handle->proxy_info.proxy_auth);
      }
    }

    bmp_cfg_db_close(db);
  }

  {
    struct curl_slist *hdr = NULL;
    hdr = curl_slist_append(hdr, "Icy-MetaData: 1");
    hdr = curl_slist_append(hdr, "User-Agent: Audacious/" VERSION " (curl transport)");
    curl_easy_setopt(handle->curl, CURLOPT_HTTPHEADER, hdr);
  }

  //handle->download = vfs_fopen(FILENAME, "wb");

  file->handle = handle;
  file->base = &curl_const;

  if (DEBUG_OPEN_CLOSE)
    g_print("Open %s with curl => %p\n", url, handle);

  return file;
}

gint
curl_vfs_fclose_impl(VFSFile * file)
{
  gint ret = 0;
  if (file == NULL)
    return -1;
  if (DEBUG_OPEN_CLOSE)
    g_print("Close %p\n", file->handle);
  if (file->handle)
    {
      CurlHandle *handle = file->handle;
      if (DEBUG_CONNECTION)
	g_print("Cancel transfer\n");
      curl_req_no_xfer(handle);
      if (DEBUG_CONNECTION)
	g_print("Okay\n");

      g_free(handle->buffer);
      if (handle->name)
	g_free(handle->name);
      if (handle->stream_stack != NULL)
        g_slist_free(handle->stream_stack);
      curl_easy_cleanup(handle->curl);

      if (handle->local_ip != NULL)
        g_free(handle->local_ip);

      if (handle->proxy_info.proxy_host != NULL)
        g_free(handle->proxy_info.proxy_host);

      if (handle->proxy_info.proxy_auth != NULL)
        g_free(handle->proxy_info.proxy_auth);

      if (handle->download)
	{
	  vfs_fclose(handle->download);
	}

      g_free(handle);
    }
  return ret;
}

size_t
curl_vfs_fread_impl(gpointer ptr_,
		    size_t size,
		    size_t nmemb,
		    VFSFile * file)
{
  unsigned char *ptr = ptr_;
  CurlHandle *handle = file->handle;
  ssize_t sz = size * nmemb;
  ssize_t ret = 0;

  if (sz < 0)
    return 0;

//  g_print("Reading %d*%d=%d from %p\n", size, nmemb, sz, handle);

  /* check if there are ungetted chars that should be picked before the real fread */
  if ( handle->stream_stack != NULL )
  {
    guchar uc;
    while ( (ret < sz) && (handle->stream_stack != NULL) )
    {
      uc = GPOINTER_TO_INT(handle->stream_stack->data);
      handle->stream_stack = g_slist_delete_link( handle->stream_stack , handle->stream_stack );
      memcpy( ptr + ret , &uc , 1 );
      handle->rd_abs++;
      ret++;
    }
  }

  curl_req_xfer(handle);

  if (DEBUG_SEEK)
    check(handle);

  memset(ptr, '\0', sz);

  while (ret < sz)
    {
      ssize_t available;
      while (!(available = buf_available(handle)) && !handle->cancel)
	{
	  //g_print("Wait for data on %p\n", handle);
	  g_usleep(10000);
	}
      if (handle->cancel)
          return (ret / size);
      if (available > sz - ret)
	available = sz - ret;
      memcpy(ptr + ret, handle->buffer + handle->rd_index, available);

      handle->rd_index = 
	(handle->rd_index + available) % handle->buffer_length;
      if (handle->rd_abs < handle->meta_abs &&
	  handle->rd_abs + available >= handle->meta_abs)
	{
	  if (DEBUG_METADATA_REPORT)
	    g_print("New song: '%s'\n", handle->title);
	}
      handle->rd_abs += available;
      ret += available;
      if (!available)
	{
	  //g_print("EOF reading from %p\n", handle);
	  break;
	}
    }

//  g_print("Read %d from %p\n", ret, handle);

  return (size_t)(ret / size);
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
  CurlHandle *handle = (CurlHandle *) stream->handle;
  guchar uc;

  g_return_val_if_fail(handle != NULL, EOF);

  if ( handle->stream_stack != NULL ) /* check if some char was ungetc'ed before */
  {
    uc = GPOINTER_TO_INT(handle->stream_stack->data);
    handle->stream_stack = g_slist_delete_link( handle->stream_stack , handle->stream_stack );
    handle->rd_abs++;
    return uc;
  }
  else if (curl_vfs_fread_impl(&uc, 1, 1, stream) != 1)
    return EOF;
  return uc;
}

gint
curl_vfs_ungetc_impl(gint c, VFSFile *stream)
{
  CurlHandle *handle = (CurlHandle *) stream->handle;

  g_return_val_if_fail(handle != NULL, EOF);

  handle->stream_stack = g_slist_prepend( handle->stream_stack , GINT_TO_POINTER(c) );
  if ( handle->stream_stack != NULL )
  {
    handle->rd_abs--;
    return c;
  }
  else
    return EOF;
}

gint
curl_vfs_fseek_impl(VFSFile * file,
		    glong offset,
		    gint whence)
{
  size_t posn;
  CurlHandle *handle = file->handle;
  /* when a seek is requested, trash the stack of ungetted chars */
  if ( handle->stream_stack != NULL )
  {
    g_slist_free( handle->stream_stack );
    handle->stream_stack = NULL;
  }
  if (DEBUG_SEEK)
    g_print("Seek %p to %d %d\n", handle, offset, whence);
  if (whence == SEEK_END && handle->length == -1)
    {
      if (!handle->thread)
	{
	  // We need a HEAD to find out the length
	  handle->no_data = 1;
	  if (DEBUG_CONNECTION)
	    g_print("Request for head info\n");
	  curl_manage_request(handle);
	  if (DEBUG_CONNECTION)
	    g_print("Completed\n");
	  handle->no_data = 0;
	}
      else
	{
	  // Wait a bit?
	}
    }

  if (whence == SEEK_END && handle->length <= 0)
    {
      if (DEBUG_SEEK)
	g_print("Tried to seek to the end of a file with unknown length\n");
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
	  if (DEBUG_CONNECTION)
	    g_print("Stop transfer\n");
	  curl_req_no_xfer(handle);
	  if (DEBUG_CONNECTION)
	    g_print("Okay\n");
	}
      else
	{
	  if (DEBUG_CONNECTION)
	    g_print("Continue transfer\n");
	  curl_req_sync_xfer(handle, posn);
	}
    }

  if (DEBUG_SEEK)
    g_print("Seeked %p from %d to %d\n", handle, posn, handle->rd_abs);
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

gchar *
curl_vfs_metadata_impl(VFSFile * file, const gchar * field)
{
  CurlHandle *handle = file->handle;
  if (!strcmp(field, "stream-name") && handle->name != NULL)
    return g_strdup(handle->name);
  if (!strcmp(field, "track-name") && handle->title != NULL)
    return g_strdup(handle->title);
  if (!strcmp(field, "content-length"))
    return g_strdup_printf("%ld", handle->length);
  return NULL;
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
  curl_vfs_truncate_impl,
  curl_vfs_metadata_impl
};

VFSConstructor curl_https_const = {
  "https://",
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
  curl_vfs_truncate_impl,
  curl_vfs_metadata_impl
};

static void init(void)
{
  vfs_register_transport(&curl_const);
  vfs_register_transport(&curl_https_const);
}

static void cleanup(void)
{
#if 0
  vfs_unregister_transport(&curl_const);
  vfs_unregister_transport(&curl_https_const);
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
