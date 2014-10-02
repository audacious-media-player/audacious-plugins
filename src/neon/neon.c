/*
 *  A neon HTTP input plugin for Audacious
 *  Copyright (C) 2007 Ralf Ertzinger
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

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <glib.h>

#ifdef DEBUG
#define NEON_DEBUG
#endif

#include "neon.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include <ne_socket.h>
#include <ne_utils.h>
#include <ne_redirect.h>
#include <ne_request.h>
#include <ne_auth.h>

#include "debug.h"
#include "rb.h"
#include "cert_verification.h"

#define NEON_BUFSIZE        (128u*1024u)
#define NEON_NETBLKSIZE     (4096u)
#define NEON_ICY_BUFSIZE    (4096)
#define NEON_RETRY_COUNT 6

static bool_t neon_plugin_init (void)
{
    int ret = ne_sock_init ();

    if (ret != 0)
    {
        _ERROR ("Could not initialize neon library: %d\n", ret);
        return FALSE;
    }

    return TRUE;
}

static void neon_plugin_fini (void)
{
    ne_sock_exit ();
}

static struct neon_handle * handle_init (void)
{
    struct neon_handle * h = g_new0 (struct neon_handle, 1);

    pthread_mutex_init (& h->reader_status.mutex, NULL);
    pthread_cond_init (& h->reader_status.cond, NULL);
    h->reader_status.reading = FALSE;
    h->reader_status.status = NEON_READER_INIT;

    init_rb_with_lock (& h->rb, NEON_BUFSIZE, & h->reader_status.mutex);

    h->purl = g_new0 (ne_uri, 1);
    h->content_length = -1;

    return h;
}

static void handle_free (struct neon_handle * h)
{
    ne_uri_free (h->purl);
    g_free (h->purl);
    destroy_rb (& h->rb);

    pthread_mutex_destroy (& h->reader_status.mutex);
    pthread_cond_destroy (& h->reader_status.cond);

    g_free (h->icy_metadata.stream_name);
    g_free (h->icy_metadata.stream_title);
    g_free (h->icy_metadata.stream_url);
    g_free (h->icy_metadata.stream_contenttype);
    g_free (h->url);
    g_free (h);
}

static bool_t neon_strcmp (const char * str, const char * cmp)
{
    return ! g_ascii_strncasecmp (str, cmp, strlen (cmp));
}

static void add_icy (struct icy_metadata * m, char * name, char * value)
{
    if (neon_strcmp (name, "StreamTitle"))
    {
        _DEBUG ("Found StreamTitle: %s", value);
        g_free (m->stream_title);
        m->stream_title = g_strdup (value);
    }

    if (neon_strcmp (name, "StreamUrl"))
    {
        _DEBUG ("Found StreamUrl: %s", value);
        g_free (m->stream_url);
        m->stream_url = g_strdup (value);
    }
}

static void parse_icy (struct icy_metadata * m, char * metadata, size_t len)
{
    typedef enum
    {
        STATE_READ_NAME,
        STATE_WAIT_VALUE,
        STATE_READ_VALUE,
        STATE_WAIT_NAME,
    } TagReadState;

    TagReadState state = STATE_READ_NAME;

    char * p = metadata;
    char * tstart = metadata;
    size_t pos = 1;

    char name[NEON_ICY_BUFSIZE];
    char value[NEON_ICY_BUFSIZE];

    name[0] = 0;
    value[0] = 0;

    while (pos < len && p[0])
    {
        switch (state)
        {
        case STATE_READ_NAME:

            /* Reading tag name */
            if (p[0] == '=')
            {
                /* End of tag name. */
                p[0] = 0;
                g_strlcpy (name, tstart, NEON_ICY_BUFSIZE);
                _DEBUG ("Found tag name: %s", name);
                state = STATE_WAIT_VALUE;
            }

            break;

        case STATE_WAIT_VALUE:

            /* Waiting for start of value */
            if (p[0] == '\'')
            {
                /* Leading ' of value */
                tstart = p + 1;
                state = STATE_READ_VALUE;
                value[0] = 0;
            }

            break;

        case STATE_READ_VALUE:

            /* Reading value */
            if (p[0] == '\'' && p[1] == ';')
            {
                /* End of value */
                p[0] = 0;
                g_strlcpy (value, tstart, NEON_ICY_BUFSIZE);
                _DEBUG ("Found tag value: %s", value);
                add_icy (m, name, value);
                state = STATE_WAIT_NAME;
            }

            break;

        case STATE_WAIT_NAME:

            /* Waiting for next tag start */
            if (p[0] == ';')
            {
                /* Next tag name starts after this char */
                tstart = p + 1;
                state = STATE_READ_NAME;
                name[0] = 0;
                value[0] = 0;
            }

            break;
        }

        p ++;
        pos ++;
    }
}

static void kill_reader (struct neon_handle * h)
{
    _DEBUG ("Signaling reader thread to terminate");
    pthread_mutex_lock (& h->reader_status.mutex);
    h->reader_status.reading = FALSE;
    pthread_cond_broadcast (& h->reader_status.cond);
    pthread_mutex_unlock (& h->reader_status.mutex);

    _DEBUG ("Waiting for reader thread to die...");
    pthread_join (h->reader, NULL);
    _DEBUG ("Reader thread has died");
}

static int server_auth_callback (void * userdata, const char * realm,
 int attempt, char * username, char * password)
{
    struct neon_handle * h = userdata;

    if (! h->purl->userinfo || ! h->purl->userinfo[0])
    {
        _ERROR ("Authentication required, but no credentials set");
        return 1;
    }

    char * * authtok = g_strsplit (h->purl->userinfo, ":", 2);

    if (strlen (authtok[1]) > NE_ABUFSIZ - 1 || strlen (authtok[0]) > NE_ABUFSIZ - 1)
    {
        _ERROR ("Username/Password too long");
        g_strfreev (authtok);
        return 1;
    }

    g_strlcpy (username, authtok[0], NE_ABUFSIZ);
    g_strlcpy (password, authtok[1], NE_ABUFSIZ);

    _DEBUG ("Authenticating: Username: %s, Password: %s", username, password);

    g_strfreev (authtok);

    return attempt;
}

static void handle_headers (struct neon_handle * h)
{
    const char * name;
    const char * value;
    void * cursor = NULL;

    _DEBUG ("Header responses:");

    while ((cursor = ne_response_header_iterate (h->request, cursor, & name, & value)))
    {
        _DEBUG ("HEADER: %s: %s", name, value);

        if (neon_strcmp (name, "accept-ranges"))
        {
            /* The server advertises range capability. we need "bytes" */
            if (strstr (value, "bytes"))
            {
                _DEBUG ("server can_ranges");
                h->can_ranges = TRUE;
            }
        }
        else if (neon_strcmp (name, "content-length"))
        {
            /* The server sent us the content length. Parse and store. */
            char * endptr;
            long len = strtol (value, & endptr, 10);

            if (value[0] && ! endptr[0] && len >= 0)
            {
                /* Valid data. */
                _DEBUG ("Content length as advertised by server: %ld", len);
                h->content_length = len;
            }
            else
                _ERROR ("Invalid content length header: %s", value);
        }
        else if (neon_strcmp (name, "content-type"))
        {
            /* The server sent us a content type. Save it for later */
            _DEBUG ("Content-Type: %s", value);
            g_free (h->icy_metadata.stream_contenttype);
            h->icy_metadata.stream_contenttype = g_strdup (value);
        }
        else if (neon_strcmp (name, "icy-metaint"))
        {
            /* The server sent us a ICY metaint header. Parse and store. */
            char * endptr;
            long len = strtol (value, & endptr, 10);

            if (value[0] && ! endptr[0] && len > 0)
            {
                /* Valid data */
                _DEBUG ("ICY MetaInt as advertised by server: %ld", len);
                h->icy_metaint = len;
                h->icy_metaleft = len;
            }
            else
                _ERROR ("Invalid ICY MetaInt header: %s", value);
        }
        else if (neon_strcmp (name, "icy-name"))
        {
            /* The server sent us a ICY name. Save it for later */
            _DEBUG ("ICY stream name: %s", value);
            g_free (h->icy_metadata.stream_name);
            h->icy_metadata.stream_name = g_strdup (value);
        }
        else if (neon_strcmp (name, "icy-br"))
        {
            /* The server sent us a bitrate. We might want to use it. */
            _DEBUG ("ICY bitrate: %d", atoi (value));
            h->icy_metadata.stream_bitrate = atoi (value);
        }
    }
}

static int neon_proxy_auth_cb (void * userdata, const char * realm, int attempt,
 char * username, char * password)
{
    char * value = aud_get_str (NULL, "proxy_user");
    g_strlcpy (username, value, NE_ABUFSIZ);
    str_unref (value);

    value = aud_get_str (NULL, "proxy_pass");
    g_strlcpy (password, value, NE_ABUFSIZ);
    str_unref (value);

    return attempt;
}

static int open_request (struct neon_handle * handle, uint64_t startbyte)
{
    int ret;
    const ne_status * status;
    ne_uri * rediruri;

    if (handle->purl->query && * (handle->purl->query))
    {
        SCONCAT3 (tmp, handle->purl->path, "?", handle->purl->query);
        handle->request = ne_request_create (handle->session, "GET", tmp);
    }
    else
        handle->request = ne_request_create (handle->session, "GET", handle->purl->path);

    if (startbyte > 0)
        ne_print_request_header (handle->request, "Range", "bytes=%"PRIu64"-", startbyte);

    ne_print_request_header (handle->request, "Icy-MetaData", "1");

    /* Try to connect to the server. */
    _DEBUG ("<%p> Connecting...", handle);
    ret = ne_begin_request (handle->request);
    status = ne_get_status (handle->request);
    _DEBUG ("<%p> Return: %d, Status: %d", handle, ret, status->code);

    if (ret == NE_OK)
    {
        switch (status->code)
        {
        case 401:
            /* Authorization required. Reconnect to authenticate */
            _DEBUG ("Reconnecting due to 401");
            ne_end_request (handle->request);
            ret = ne_begin_request (handle->request);
            break;

        case 301:
        case 302:
        case 303:
        case 307:
            /* Redirect encountered. Reconnect. */
            ne_end_request (handle->request);
            ret = NE_REDIRECT;
            break;

        case 407:
            /* Proxy auth required. Reconnect to authenticate */
            _DEBUG ("Reconnecting due to 407");
            ne_end_request (handle->request);
            ret = ne_begin_request (handle->request);
            break;
        }
    }

    switch (ret)
    {
    case NE_OK:
        if (status->code > 199 && status->code < 300)
        {
            /* URL opened OK */
            _DEBUG ("<%p> URL opened OK", handle);
            handle->content_start = startbyte;
            handle->pos = startbyte;
            handle_headers (handle);
            return 0;
        }

        break;

    case NE_REDIRECT:
        /* We hit a redirect. Handle it. */
        _DEBUG ("<%p> Redirect encountered", handle);
        handle->redircount += 1;
        rediruri = (ne_uri *) ne_redirect_location (handle->session);
        ne_request_destroy (handle->request);
        handle->request = NULL;

        if (! rediruri)
        {
            _ERROR ("<%p> Could not parse redirect response", (void *) handle);
            return -1;
        }

        ne_uri_free (handle->purl);
        ne_uri_copy (handle->purl, rediruri);
        return 1;
    }

    /* Something went wrong. */
    _ERROR ("<%p> Could not open URL: %d (%d)", (void *) handle, ret, status->code);

    if (ret)
        _ERROR ("<%p> neon error string: %s", (void *) handle, ne_get_error (handle->session));

    ne_request_destroy (handle->request);
    handle->request = NULL;
    return -1;
}

static int open_handle (struct neon_handle * handle, uint64_t startbyte)
{
    int ret;
    char * proxy_host = NULL;
    int proxy_port = 0;

    bool_t use_proxy = aud_get_bool (NULL, "use_proxy");
    bool_t use_proxy_auth = aud_get_bool (NULL, "use_proxy_auth");

    if (use_proxy)
    {
        proxy_host = aud_get_str (NULL, "proxy_host");
        proxy_port = aud_get_int (NULL, "proxy_port");
    }

    handle->redircount = 0;

    _DEBUG ("<%p> Parsing URL", handle);

    if (ne_uri_parse (handle->url, handle->purl) != 0)
    {
        _ERROR ("<%p> Could not parse URL '%s'", (void *) handle, handle->url);
        return -1;
    }

    while (handle->redircount < 10)
    {
        if (! handle->purl->port)
            handle->purl->port = ne_uri_defaultport (handle->purl->scheme);

        _DEBUG ("<%p> Creating session to %s://%s:%d", handle,
         handle->purl->scheme, handle->purl->host, handle->purl->port);
        handle->session = ne_session_create (handle->purl->scheme,
         handle->purl->host, handle->purl->port);
        ne_redirect_register (handle->session);
        ne_add_server_auth (handle->session, NE_AUTH_BASIC, server_auth_callback, (void *) handle);
        ne_set_session_flag (handle->session, NE_SESSFLAG_ICYPROTO, 1);
        ne_set_session_flag (handle->session, NE_SESSFLAG_PERSIST, 0);
        ne_set_connect_timeout (handle->session, 10);
        ne_set_read_timeout (handle->session, 10);
        ne_set_useragent (handle->session, "Audacious/" PACKAGE_VERSION);

        if (use_proxy)
        {
            _DEBUG ("<%p> Using proxy: %s:%d", handle, proxy_host, proxy_port);
            ne_session_proxy (handle->session, proxy_host, proxy_port);

            if (use_proxy_auth)
            {
                _DEBUG ("<%p> Using proxy authentication", handle);
                ne_add_proxy_auth (handle->session, NE_AUTH_BASIC,
                 neon_proxy_auth_cb, (void *) handle);
            }
        }

        if (! strcmp ("https", handle->purl->scheme))
        {
            ne_ssl_trust_default_ca (handle->session);
            ne_ssl_set_verify (handle->session,
             neon_vfs_verify_environment_ssl_certs, handle->session);
        }

        _DEBUG ("<%p> Creating request", handle);
        ret = open_request (handle, startbyte);

        if (! ret)
        {
            str_unref (proxy_host);
            return 0;
        }

        if (ret == -1)
        {
            ne_session_destroy (handle->session);
            handle->session = NULL;
            str_unref (proxy_host);
            return -1;
        }

        _DEBUG ("<%p> Following redirect...", handle);
        ne_session_destroy (handle->session);
        handle->session = NULL;
    }

    /* If we get here, our redirect count exceeded */
    _ERROR ("<%p> Redirect count exceeded for URL %s", (void *) handle, handle->url);

    str_unref (proxy_host);
    return 1;
}

typedef enum
{
    FILL_BUFFER_SUCCESS,
    FILL_BUFFER_ERROR,
    FILL_BUFFER_EOF
} FillBufferResult;

static FillBufferResult fill_buffer (struct neon_handle * h)
{
    int bsize = free_rb (& h->rb);
    int to_read = MIN (bsize, NEON_NETBLKSIZE);

    char buffer[NEON_NETBLKSIZE];

    bsize = ne_read_response_block (h->request, buffer, to_read);

    if (! bsize)
    {
        _DEBUG ("<%p> End of file encountered", h);
        return FILL_BUFFER_EOF;
    }

    if (bsize < 0)
    {
        _ERROR ("<%p> Error while reading from the network", (void *) h);
        ne_request_destroy (h->request);
        h->request = NULL;
        return FILL_BUFFER_ERROR;
    }

    _DEBUG ("<%p> Read %d bytes of %d", h, bsize, to_read);

    write_rb (& h->rb, buffer, bsize);

    return FILL_BUFFER_SUCCESS;
}

static void * reader_thread (void * data)
{
    struct neon_handle * h = data;

    pthread_mutex_lock (& h->reader_status.mutex);

    while (h->reader_status.reading)
    {
        /* Hit the network only if we have more than NEON_NETBLKSIZE of free buffer */
        if (NEON_NETBLKSIZE < free_rb_locked (& h->rb))
        {
            pthread_mutex_unlock (& h->reader_status.mutex);

            FillBufferResult ret = fill_buffer (h);

            pthread_mutex_lock (& h->reader_status.mutex);

            /* Wake up main thread if it is waiting. */
            pthread_cond_broadcast (& h->reader_status.cond);

            if (ret == FILL_BUFFER_ERROR)
            {
                _ERROR ("<%p> Error while reading from the network. "
                        "Terminating reader thread", (void *) h);
                h->reader_status.status = NEON_READER_ERROR;
                pthread_mutex_unlock (& h->reader_status.mutex);
                return NULL;
            }
            else if (ret == FILL_BUFFER_EOF)
            {
                _DEBUG ("<%p> EOF encountered while reading from the network. "
                        "Terminating reader thread", (void *) h);
                h->reader_status.status = NEON_READER_EOF;
                pthread_mutex_unlock (& h->reader_status.mutex);
                return NULL;
            }
        }
        else
        {
            /* Not enough free space in the buffer.
             * Sleep until the main thread wakes us up. */
            pthread_cond_wait (& h->reader_status.cond, & h->reader_status.mutex);
        }
    }

    _DEBUG ("<%p> Reader thread terminating gracefully", h);
    h->reader_status.status = NEON_READER_TERM;
    pthread_mutex_unlock (& h->reader_status.mutex);

    return NULL;
}

void * neon_vfs_fopen_impl (const char * path, const char * mode)
{
    struct neon_handle * handle = handle_init ();

    _DEBUG ("<%p> Trying to open '%s' with neon", (void *) handle, path);

    handle->url = g_strdup (path);

    if (open_handle (handle, 0) != 0)
    {
        _ERROR ("<%p> Could not open URL", (void *) handle);
        handle_free (handle);
        return NULL;
    }

    return handle;
}

int neon_vfs_fclose_impl (VFSFile * file)
{
    struct neon_handle * h = vfs_get_handle (file);

    if (h->reader_status.reading)
        kill_reader (h);

    if (h->request)
        ne_request_destroy (h->request);
    if (h->session)
        ne_session_destroy (h->session);

    handle_free (h);

    return 0;
}

static int64_t neon_fread_real (void * ptr, int64_t size, int64_t nmemb, VFSFile * file)
{
    struct neon_handle * h = vfs_get_handle (file);

    if (! h->request)
    {
        _ERROR ("<%p> No request to read from, seek gone wrong?", (void *) h);
        return 0;
    }

    if (! size || ! nmemb || h->eof)
        return 0;

    /* If the buffer is empty, wait for the reader thread to fill it. */
    pthread_mutex_lock (& h->reader_status.mutex);

    for (int retries = 0; retries < NEON_RETRY_COUNT; retries ++)
    {
        if (used_rb_locked (& h->rb) / size > 0 || ! h->reader_status.reading ||
         h->reader_status.status != NEON_READER_RUN)
            break;

        pthread_cond_broadcast (& h->reader_status.cond);
        pthread_cond_wait (& h->reader_status.cond, & h->reader_status.mutex);
    }

    pthread_mutex_unlock (& h->reader_status.mutex);

    if (! h->reader_status.reading)
    {
        if (h->reader_status.status != NEON_READER_EOF || h->content_length != -1)
        {
            /* There is no reader thread yet. Read the first bytes from
             * the network ourselves, and then fire up the reader thread
             * to keep the buffer filled up. */
            _DEBUG ("<%p> Doing initial buffer fill", h);
            FillBufferResult ret = fill_buffer (h);

            if (ret == FILL_BUFFER_ERROR)
            {
                _ERROR ("<%p> Error while reading from the network", (void *) h);
                return 0;
            }

            /* We have some data in the buffer now.
             * Start the reader thread if we did not reach EOF during
             * the initial fill */
            pthread_mutex_lock (& h->reader_status.mutex);

            if (ret == FILL_BUFFER_SUCCESS)
            {
                h->reader_status.reading = TRUE;
                _DEBUG ("<%p> Starting reader thread", h);
                pthread_create (& h->reader, NULL, reader_thread, h);
                h->reader_status.status = NEON_READER_RUN;
            }
            else if (ret == FILL_BUFFER_EOF)
            {
                _DEBUG ("<%p> No reader thread needed (stream has reached EOF during fill)", h);
                h->reader_status.reading = FALSE;
                h->reader_status.status = NEON_READER_EOF;
            }

            pthread_mutex_unlock (& h->reader_status.mutex);
        }
    }
    else
    {
        /* There already is a reader thread. Look if it is in good shape. */
        pthread_mutex_lock (& h->reader_status.mutex);

        switch (h->reader_status.status)
        {
        case NEON_READER_INIT:
        case NEON_READER_RUN:
            /* All is well, nothing to be done. */
            break;

        case NEON_READER_ERROR:
            /* A reader error happened. Log it, and treat it like an EOF
             * condition, by falling through to the NEON_READER_EOF codepath. */
            _DEBUG ("<%p> NEON_READER_ERROR happened. Terminating reader thread and marking EOF.", h);
            h->reader_status.status = NEON_READER_EOF;
            pthread_mutex_unlock (& h->reader_status.mutex);

            if (h->reader_status.reading)
                kill_reader (h);

            pthread_mutex_lock (& h->reader_status.mutex);

        case NEON_READER_EOF:
            /* If there still is data in the buffer, carry on.
             * If not, terminate the reader thread and return 0. */
            if (! used_rb_locked (& h->rb))
            {
                _DEBUG ("<%p> Reached end of stream", h);
                pthread_mutex_unlock (& h->reader_status.mutex);

                if (h->reader_status.reading)
                    kill_reader (h);

                h->eof = TRUE;
                return 0;
            }

            break;

        case NEON_READER_TERM:
            /* The reader thread terminated gracefully, most likely on our own request.
             * We should not get here. */
            g_warn_if_reached ();
            pthread_mutex_unlock (& h->reader_status.mutex);
            return 0;
        }

        pthread_mutex_unlock (& h->reader_status.mutex);
    }

    /* Deliver data from the buffer */
    if (! used_rb (& h->rb))
    {
        /* The buffer is still empty, we can deliver no data! */
        _ERROR ("<%p> Buffer still underrun, fatal.", (void *) h);
        return 0;
    }

    int belem = used_rb (& h->rb) / size;

    if (h->icy_metaint)
    {
        if (! h->icy_metaleft)
        {
            /* The next data in the buffer is a ICY metadata announcement.
             * Get the length byte */
            unsigned char icy_metalen;
            read_rb (& h->rb, & icy_metalen, 1);

            /*  We need enough data in the buffer to
             * a) Read the complete ICY metadata block
             * b) deliver at least one byte to the reader */
            _DEBUG ("<%p> Expecting %d bytes of ICY metadata", h, (icy_metalen*16));

            if (used_rb (& h->rb) - icy_metalen * 16 < size)
            {
                /* There is not enough data. We do not have much choice at this point,
                 * so we'll deliver the metadata as normal data to the reader and
                 * hope for the best. */
                _ERROR ("<%p> Buffer underrun when reading metadata. Expect "
                 "audio degradation", (void *) h);
                h->icy_metaleft = h->icy_metaint + icy_metalen * 16;
            }
            else
            {
                /* Grab the metadata from the buffer and send it to the parser */
                char icy_metadata[NEON_ICY_BUFSIZE];
                read_rb (& h->rb, icy_metadata, icy_metalen * 16);
                parse_icy (& h->icy_metadata, icy_metadata, icy_metalen * 16);
                h->icy_metaleft = h->icy_metaint;
            }
        }

        /* The maximum number of bytes we can deliver is determined
         * by the number of bytes left until the next metadata announcement */
        belem = MIN (used_rb (& h->rb), h->icy_metaleft) / size;
    }

    nmemb = MIN (belem, nmemb);
    read_rb (& h->rb, ptr, nmemb * size);

    /* Signal the network thread to continue reading */
    pthread_mutex_lock (& h->reader_status.mutex);

    if (h->reader_status.status == NEON_READER_EOF)
    {
        if (! used_rb_locked (& h->rb))
        {
            _DEBUG ("<%p> stream EOF reached and buffer empty", h);
            h->eof = TRUE;
        }
    }
    else
        pthread_cond_broadcast (& h->reader_status.cond);

    pthread_mutex_unlock (& h->reader_status.mutex);

    h->pos += nmemb * size;
    h->icy_metaleft -= nmemb * size;

    return nmemb;
}

/* neon_fread_real will do only a partial read if the buffer underruns, so we
 * must call it repeatedly until we have read the full request. */
int64_t neon_vfs_fread_impl (void * buffer, int64_t size, int64_t count, VFSFile * handle)
{
    size_t total = 0, new;

    _DEBUG ("<%p> fread %d x %d", (void *) handle, (int) size, (int) count);

    while (count > 0 && (new = neon_fread_real (buffer, size, count, handle)) > 0)
    {
        buffer = (char *) buffer + size * new;
        total += new;
        count -= new;
    }

    _DEBUG ("<%p> fread = %d", (void *) handle, (int) total);

    return total;
}

int64_t neon_vfs_fwrite_impl (const void * ptr, int64_t size, int64_t nmemb, VFSFile * file)
{
    _ERROR ("<%p> NOT IMPLEMENTED", (void *) vfs_get_handle (file));

    return 0;
}

int64_t neon_vfs_ftell_impl (VFSFile * file)
{
    struct neon_handle * h = vfs_get_handle (file);

    _DEBUG ("<%p> Current file position: %ld", h, h->pos);

    return h->pos;
}

bool_t neon_vfs_feof_impl (VFSFile * file)
{
    struct neon_handle * h = vfs_get_handle (file);

    _DEBUG ("<%p> EOF status: %s", h, h->eof ? "TRUE" : "FALSE");

    return h->eof;
}

int neon_vfs_truncate_impl (VFSFile * file, int64_t size)
{
    _ERROR ("<%p> NOT IMPLEMENTED", (void *) vfs_get_handle (file));

    return 0;
}

int neon_vfs_fseek_impl (VFSFile * file, int64_t offset, int whence)
{
    struct neon_handle * h = vfs_get_handle (file);

    _DEBUG ("<%p> Seek requested: offset %ld, whence %d", h, offset, whence);

    /* To seek to a non-zero offset, two things must be satisfied:
     * - the server must advertise a content-length
     * - the server must advertise accept-ranges: bytes */
    if ((whence != SEEK_SET || offset) && (h->content_length < 0 || ! h->can_ranges))
    {
        _DEBUG ("<%p> Can not seek due to server restrictions", h);
        return -1;
    }

    int64_t content_length = h->content_length + h->content_start;
    int64_t newpos;

    switch (whence)
    {
    case SEEK_SET:
        newpos = offset;
        break;

    case SEEK_CUR:
        newpos = h->pos + offset;
        break;

    case SEEK_END:
        if (offset == 0)
        {
            h->pos = content_length;
            h->eof = TRUE;
            return 0;
        }

        newpos = content_length + offset;
        break;

    default:
        _ERROR ("<%p> Invalid whence specified", (void *) h);
        return -1;
    }

    _DEBUG ("<%p> Position to seek to: %ld, current: %ld", h, newpos, h->pos);

    if (newpos < 0)
    {
        _ERROR ("<%p> Can not seek before start of stream", (void *) h);
        return -1;
    }

    if (newpos && newpos >= content_length)
    {
        _ERROR ("<%p> Can not seek beyond end of stream (%ld >= %ld)",
         (void *) h, (long) newpos, (long) content_length);
        return -1;
    }

    if (newpos == h->pos)
        return 0;

    /* To seek to the new position we have to
     * - stop the current reader thread, if there is one
     * - destroy the current request
     * - dump all data currently in the ringbuffer
     * - create a new request starting at newpos */
    if (h->reader_status.reading)
        kill_reader (h);

    if (h->request)
    {
        ne_request_destroy (h->request);
        h->request = NULL;
    }

    if (h->session)
    {
        ne_session_destroy (h->session);
        h->session = NULL;
    }

    reset_rb (& h->rb);

    if (open_handle (h, newpos) != 0)
    {
        _ERROR ("<%p> Error while creating new request!", (void *) h);
        return -1;
    }

    /* Things seem to have worked. The next read request will start
     * the reader thread again. */
    h->eof = FALSE;

    return 0;
}

char * neon_vfs_metadata_impl (VFSFile * file, const char * field)
{
    struct neon_handle * h = vfs_get_handle (file);

    _DEBUG ("<%p> Field name: %s", h, field);

    if (! strcmp (field, "track-name") && h->icy_metadata.stream_title)
        return str_to_utf8 (h->icy_metadata.stream_title, -1);

    if (! strcmp (field, "stream-name") && h->icy_metadata.stream_name)
        return str_to_utf8 (h->icy_metadata.stream_name, -1);

    if (! strcmp (field, "content-type") && h->icy_metadata.stream_contenttype)
        return str_to_utf8 (h->icy_metadata.stream_contenttype, -1);

    if (! strcmp (field, "content-bitrate"))
        return int_to_str (h->icy_metadata.stream_bitrate * 1000);

    return NULL;
}

int64_t neon_vfs_fsize_impl (VFSFile * file)
{
    struct neon_handle * h = vfs_get_handle (file);

    if (h->content_length < 0)
    {
        _DEBUG ("<%p> Unknown content length", h);
        return -1;
    }

    return h->content_start + h->content_length;
}

static const char * const neon_schemes[] = {"http", "https", NULL};

static VFSConstructor constructor =
{
    .vfs_fopen_impl = neon_vfs_fopen_impl,
    .vfs_fclose_impl = neon_vfs_fclose_impl,
    .vfs_fread_impl = neon_vfs_fread_impl,
    .vfs_fwrite_impl = neon_vfs_fwrite_impl,
    .vfs_fseek_impl = neon_vfs_fseek_impl,
    .vfs_ftell_impl = neon_vfs_ftell_impl,
    .vfs_feof_impl = neon_vfs_feof_impl,
    .vfs_ftruncate_impl = neon_vfs_truncate_impl,
    .vfs_fsize_impl = neon_vfs_fsize_impl,
    .vfs_get_metadata_impl = neon_vfs_metadata_impl
};

AUD_TRANSPORT_PLUGIN
(
    .name = N_("Neon HTTP/HTTPS Plugin"),
    .domain = PACKAGE,
    .schemes = neon_schemes,
    .init = neon_plugin_init,
    .cleanup = neon_plugin_fini,
    .vtable = & constructor
)
