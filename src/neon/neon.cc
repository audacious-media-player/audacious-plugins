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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "neon.h"

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>

#include <ne_socket.h>
#include <ne_utils.h>
#include <ne_redirect.h>
#include <ne_request.h>
#include <ne_auth.h>

#include "rb.h"
#include "cert_verification.h"

#define NEON_BUFSIZE        (128*1024)
#define NEON_NETBLKSIZE     (4096)
#define NEON_ICY_BUFSIZE    (4096)
#define NEON_RETRY_COUNT 6

static bool neon_plugin_init (void)
{
    int ret = ne_sock_init ();

    if (ret != 0)
    {
        AUDERR ("Could not initialize neon library: %d\n", ret);
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

    pthread_mutex_init (& h->reader_status.mutex, nullptr);
    pthread_cond_init (& h->reader_status.cond, nullptr);
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

static bool neon_strcmp (const char * str, const char * cmp)
{
    return ! g_ascii_strncasecmp (str, cmp, strlen (cmp));
}

static void add_icy (struct icy_metadata * m, char * name, char * value)
{
    if (neon_strcmp (name, "StreamTitle"))
    {
        AUDDBG ("Found StreamTitle: %s\n", value);
        g_free (m->stream_title);
        m->stream_title = g_strdup (value);
    }

    if (neon_strcmp (name, "StreamUrl"))
    {
        AUDDBG ("Found StreamUrl: %s\n", value);
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
                AUDDBG ("Found tag name: %s\n", name);
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
                AUDDBG ("Found tag value: %s\n", value);
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
    AUDDBG ("Signaling reader thread to terminate\n");
    pthread_mutex_lock (& h->reader_status.mutex);
    h->reader_status.reading = FALSE;
    pthread_cond_broadcast (& h->reader_status.cond);
    pthread_mutex_unlock (& h->reader_status.mutex);

    AUDDBG ("Waiting for reader thread to die...\n");
    pthread_join (h->reader, nullptr);
    AUDDBG ("Reader thread has died\n");
}

static int server_auth_callback (void * userdata, const char * realm,
 int attempt, char * username, char * password)
{
    struct neon_handle * h = (neon_handle *) userdata;

    if (! h->purl->userinfo || ! h->purl->userinfo[0])
    {
        AUDERR ("Authentication required, but no credentials set\n");
        return 1;
    }

    char * * authtok = g_strsplit (h->purl->userinfo, ":", 2);

    if (strlen (authtok[1]) > NE_ABUFSIZ - 1 || strlen (authtok[0]) > NE_ABUFSIZ - 1)
    {
        AUDERR ("Username/Password too long\n");
        g_strfreev (authtok);
        return 1;
    }

    g_strlcpy (username, authtok[0], NE_ABUFSIZ);
    g_strlcpy (password, authtok[1], NE_ABUFSIZ);

    AUDDBG ("Authenticating: Username: %s, Password: %s\n", username, password);

    g_strfreev (authtok);

    return attempt;
}

static void handle_headers (struct neon_handle * h)
{
    const char * name;
    const char * value;
    void * cursor = nullptr;

    AUDDBG ("Header responses:\n");

    while ((cursor = ne_response_header_iterate (h->request, cursor, & name, & value)))
    {
        AUDDBG ("HEADER: %s: %s\n", name, value);

        if (neon_strcmp (name, "accept-ranges"))
        {
            /* The server advertises range capability. we need "bytes" */
            if (strstr (value, "bytes"))
            {
                AUDDBG ("server can_ranges\n");
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
                AUDDBG ("Content length as advertised by server: %ld\n", len);
                h->content_length = len;
            }
            else
                AUDERR ("Invalid content length header: %s\n", value);
        }
        else if (neon_strcmp (name, "content-type"))
        {
            /* The server sent us a content type. Save it for later */
            AUDDBG ("Content-Type: %s\n", value);
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
                AUDDBG ("ICY MetaInt as advertised by server: %ld\n", len);
                h->icy_metaint = len;
                h->icy_metaleft = len;
            }
            else
                AUDERR ("Invalid ICY MetaInt header: %s\n", value);
        }
        else if (neon_strcmp (name, "icy-name"))
        {
            /* The server sent us a ICY name. Save it for later */
            AUDDBG ("ICY stream name: %s\n", value);
            g_free (h->icy_metadata.stream_name);
            h->icy_metadata.stream_name = g_strdup (value);
        }
        else if (neon_strcmp (name, "icy-br"))
        {
            /* The server sent us a bitrate. We might want to use it. */
            AUDDBG ("ICY bitrate: %d\n", atoi (value));
            h->icy_metadata.stream_bitrate = atoi (value);
        }
    }
}

static int neon_proxy_auth_cb (void * userdata, const char * realm, int attempt,
 char * username, char * password)
{
    String value = aud_get_str (nullptr, "proxy_user");
    g_strlcpy (username, value, NE_ABUFSIZ);

    value = aud_get_str (nullptr, "proxy_pass");
    g_strlcpy (password, value, NE_ABUFSIZ);

    return attempt;
}

static int open_request (struct neon_handle * handle, uint64_t startbyte)
{
    int ret;
    const ne_status * status;
    ne_uri * rediruri;

    if (handle->purl->query && * (handle->purl->query))
    {
        StringBuf tmp = str_concat ({handle->purl->path, "?", handle->purl->query});
        handle->request = ne_request_create (handle->session, "GET", tmp);
    }
    else
        handle->request = ne_request_create (handle->session, "GET", handle->purl->path);

    if (startbyte > 0)
        ne_print_request_header (handle->request, "Range", "bytes=%" PRIu64 "-", startbyte);

    ne_print_request_header (handle->request, "Icy-MetaData", "1");

    /* Try to connect to the server. */
    AUDDBG ("<%p> Connecting...\n", handle);
    ret = ne_begin_request (handle->request);
    status = ne_get_status (handle->request);
    AUDDBG ("<%p> Return: %d, Status: %d\n", handle, ret, status->code);

    if (ret == NE_OK)
    {
        switch (status->code)
        {
        case 401:
            /* Authorization required. Reconnect to authenticate */
            AUDDBG ("Reconnecting due to 401\n");
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
            AUDDBG ("Reconnecting due to 407\n");
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
            AUDDBG ("<%p> URL opened OK\n", handle);
            handle->content_start = startbyte;
            handle->pos = startbyte;
            handle_headers (handle);
            return 0;
        }

        break;

    case NE_REDIRECT:
        /* We hit a redirect. Handle it. */
        AUDDBG ("<%p> Redirect encountered\n", handle);
        handle->redircount += 1;
        rediruri = (ne_uri *) ne_redirect_location (handle->session);
        ne_request_destroy (handle->request);
        handle->request = nullptr;

        if (! rediruri)
        {
            AUDERR ("<%p> Could not parse redirect response\n", (void *) handle);
            return -1;
        }

        ne_uri_free (handle->purl);
        ne_uri_copy (handle->purl, rediruri);
        return 1;
    }

    /* Something went wrong. */
    AUDERR ("<%p> Could not open URL: %d (%d)\n", (void *) handle, ret, status->code);

    if (ret)
        AUDERR ("<%p> neon error string: %s\n", (void *) handle, ne_get_error (handle->session));

    ne_request_destroy (handle->request);
    handle->request = nullptr;
    return -1;
}

static int open_handle (struct neon_handle * handle, uint64_t startbyte)
{
    int ret;
    String proxy_host;
    int proxy_port = 0;

    bool use_proxy = aud_get_bool (nullptr, "use_proxy");
    bool use_proxy_auth = aud_get_bool (nullptr, "use_proxy_auth");

    if (use_proxy)
    {
        proxy_host = aud_get_str (nullptr, "proxy_host");
        proxy_port = aud_get_int (nullptr, "proxy_port");
    }

    handle->redircount = 0;

    AUDDBG ("<%p> Parsing URL\n", handle);

    if (ne_uri_parse (handle->url, handle->purl) != 0)
    {
        AUDERR ("<%p> Could not parse URL '%s'\n", (void *) handle, handle->url);
        return -1;
    }

    while (handle->redircount < 10)
    {
        if (! handle->purl->port)
            handle->purl->port = ne_uri_defaultport (handle->purl->scheme);

        AUDDBG ("<%p> Creating session to %s://%s:%d\n", handle,
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
            AUDDBG ("<%p> Using proxy: %s:%d\n", handle, (const char *) proxy_host, proxy_port);
            ne_session_proxy (handle->session, proxy_host, proxy_port);

            if (use_proxy_auth)
            {
                AUDDBG ("<%p> Using proxy authentication\n", handle);
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

        AUDDBG ("<%p> Creating request\n", handle);
        ret = open_request (handle, startbyte);

        if (! ret)
            return 0;

        if (ret == -1)
        {
            ne_session_destroy (handle->session);
            handle->session = nullptr;
            return -1;
        }

        AUDDBG ("<%p> Following redirect...\n", handle);
        ne_session_destroy (handle->session);
        handle->session = nullptr;
    }

    /* If we get here, our redirect count exceeded */
    AUDERR ("<%p> Redirect count exceeded for URL %s\n", (void *) handle, handle->url);

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
    int to_read = aud::min (bsize, NEON_NETBLKSIZE);

    char buffer[NEON_NETBLKSIZE];

    bsize = ne_read_response_block (h->request, buffer, to_read);

    if (! bsize)
    {
        AUDDBG ("<%p> End of file encountered\n", h);
        return FILL_BUFFER_EOF;
    }

    if (bsize < 0)
    {
        AUDERR ("<%p> Error while reading from the network\n", (void *) h);
        ne_request_destroy (h->request);
        h->request = nullptr;
        return FILL_BUFFER_ERROR;
    }

    AUDDBG ("<%p> Read %d bytes of %d\n", h, bsize, to_read);

    write_rb (& h->rb, buffer, bsize);

    return FILL_BUFFER_SUCCESS;
}

static void * reader_thread (void * data)
{
    struct neon_handle * h = (neon_handle *) data;

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
                AUDERR ("<%p> Error while reading from the network. "
                        "Terminating reader thread\n", (void *) h);
                h->reader_status.status = NEON_READER_ERROR;
                pthread_mutex_unlock (& h->reader_status.mutex);
                return nullptr;
            }
            else if (ret == FILL_BUFFER_EOF)
            {
                AUDDBG ("<%p> EOF encountered while reading from the network. "
                        "Terminating reader thread\n", (void *) h);
                h->reader_status.status = NEON_READER_EOF;
                pthread_mutex_unlock (& h->reader_status.mutex);
                return nullptr;
            }
        }
        else
        {
            /* Not enough free space in the buffer.
             * Sleep until the main thread wakes us up. */
            pthread_cond_wait (& h->reader_status.cond, & h->reader_status.mutex);
        }
    }

    AUDDBG ("<%p> Reader thread terminating gracefully\n", h);
    h->reader_status.status = NEON_READER_TERM;
    pthread_mutex_unlock (& h->reader_status.mutex);

    return nullptr;
}

void * neon_vfs_fopen_impl (const char * path, const char * mode)
{
    struct neon_handle * handle = handle_init ();

    AUDDBG ("<%p> Trying to open '%s' with neon\n", (void *) handle, path);

    handle->url = g_strdup (path);

    if (open_handle (handle, 0) != 0)
    {
        AUDERR ("<%p> Could not open URL\n", (void *) handle);
        handle_free (handle);
        return nullptr;
    }

    return handle;
}

int neon_vfs_fclose_impl (VFSFile * file)
{
    struct neon_handle * h = (neon_handle *) vfs_get_handle (file);

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
    struct neon_handle * h = (neon_handle *) vfs_get_handle (file);

    if (! h->request)
    {
        AUDERR ("<%p> No request to read from, seek gone wrong?\n", (void *) h);
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
            AUDDBG ("<%p> Doing initial buffer fill\n", h);
            FillBufferResult ret = fill_buffer (h);

            if (ret == FILL_BUFFER_ERROR)
            {
                AUDERR ("<%p> Error while reading from the network\n", (void *) h);
                return 0;
            }

            /* We have some data in the buffer now.
             * Start the reader thread if we did not reach EOF during
             * the initial fill */
            pthread_mutex_lock (& h->reader_status.mutex);

            if (ret == FILL_BUFFER_SUCCESS)
            {
                h->reader_status.reading = TRUE;
                AUDDBG ("<%p> Starting reader thread\n", h);
                pthread_create (& h->reader, nullptr, reader_thread, h);
                h->reader_status.status = NEON_READER_RUN;
            }
            else if (ret == FILL_BUFFER_EOF)
            {
                AUDDBG ("<%p> No reader thread needed (stream has reached EOF during fill)\n", h);
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
            AUDDBG ("<%p> NEON_READER_ERROR happened. Terminating reader thread and marking EOF.\n", h);
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
                AUDDBG ("<%p> Reached end of stream\n", h);
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
        AUDERR ("<%p> Buffer still underrun, fatal.\n", (void *) h);
        return 0;
    }

    int64_t belem = used_rb (& h->rb) / size;

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
            AUDDBG ("<%p> Expecting %d bytes of ICY metadata\n", h, (icy_metalen*16));

            if (free_rb (& h->rb) - icy_metalen * 16 < size)
            {
                /* There is not enough data. We do not have much choice at this point,
                 * so we'll deliver the metadata as normal data to the reader and
                 * hope for the best. */
                AUDERR ("<%p> Buffer underrun when reading metadata. Expect "
                 "audio degradation\n", (void *) h);
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
        belem = aud::min ((int64_t) used_rb (& h->rb), h->icy_metaleft) / size;
    }

    nmemb = aud::min (belem, nmemb);
    read_rb (& h->rb, ptr, nmemb * size);

    /* Signal the network thread to continue reading */
    pthread_mutex_lock (& h->reader_status.mutex);

    if (h->reader_status.status == NEON_READER_EOF)
    {
        if (! free_rb_locked (& h->rb))
        {
            AUDDBG ("<%p> stream EOF reached and buffer empty\n", h);
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
    size_t total = 0, part;

    AUDDBG ("<%p> fread %d x %d\n", (void *) handle, (int) size, (int) count);

    while (count > 0 && (part = neon_fread_real (buffer, size, count, handle)) > 0)
    {
        buffer = (char *) buffer + size * part;
        total += part;
        count -= part;
    }

    AUDDBG ("<%p> fread = %d\n", (void *) handle, (int) total);

    return total;
}

int64_t neon_vfs_fwrite_impl (const void * ptr, int64_t size, int64_t nmemb, VFSFile * file)
{
    AUDERR ("<%p> NOT IMPLEMENTED\n", (void *) vfs_get_handle (file));

    return 0;
}

int64_t neon_vfs_ftell_impl (VFSFile * file)
{
    struct neon_handle * h = (neon_handle *) vfs_get_handle (file);

    AUDDBG ("<%p> Current file position: %ld\n", h, h->pos);

    return h->pos;
}

bool neon_vfs_feof_impl (VFSFile * file)
{
    struct neon_handle * h = (neon_handle *) vfs_get_handle (file);

    AUDDBG ("<%p> EOF status: %s\n", h, h->eof ? "TRUE" : "FALSE");

    return h->eof;
}

int neon_vfs_truncate_impl (VFSFile * file, int64_t size)
{
    AUDERR ("<%p> NOT IMPLEMENTED\n", (void *) vfs_get_handle (file));

    return 0;
}

int neon_vfs_fseek_impl (VFSFile * file, int64_t offset, VFSSeekType whence)
{
    struct neon_handle * h = (neon_handle *) vfs_get_handle (file);

    AUDDBG ("<%p> Seek requested: offset %ld, whence %d\n", h, offset, whence);

    /* To seek to a non-zero offset, two things must be satisfied:
     * - the server must advertise a content-length
     * - the server must advertise accept-ranges: bytes */
    if ((whence != VFS_SEEK_SET || offset) && (h->content_length < 0 || ! h->can_ranges))
    {
        AUDDBG ("<%p> Can not seek due to server restrictions\n", h);
        return -1;
    }

    int64_t content_length = h->content_length + h->content_start;
    int64_t newpos;

    switch (whence)
    {
    case VFS_SEEK_SET:
        newpos = offset;
        break;

    case VFS_SEEK_CUR:
        newpos = h->pos + offset;
        break;

    case VFS_SEEK_END:
        if (offset == 0)
        {
            h->pos = content_length;
            h->eof = TRUE;
            return 0;
        }

        newpos = content_length + offset;
        break;

    default:
        AUDERR ("<%p> Invalid whence specified\n", (void *) h);
        return -1;
    }

    AUDDBG ("<%p> Position to seek to: %ld, current: %ld\n", h, newpos, h->pos);

    if (newpos < 0)
    {
        AUDERR ("<%p> Can not seek before start of stream\n", (void *) h);
        return -1;
    }

    if (newpos && newpos >= content_length)
    {
        AUDERR ("<%p> Can not seek beyond end of stream (%ld >= %ld)\n",
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
        h->request = nullptr;
    }

    if (h->session)
    {
        ne_session_destroy (h->session);
        h->session = nullptr;
    }

    reset_rb (& h->rb);

    if (open_handle (h, newpos) != 0)
    {
        AUDERR ("<%p> Error while creating new request!\n", (void *) h);
        return -1;
    }

    /* Things seem to have worked. The next read request will start
     * the reader thread again. */
    h->eof = FALSE;

    return 0;
}

String neon_vfs_metadata_impl (VFSFile * file, const char * field)
{
    struct neon_handle * h = (neon_handle *) vfs_get_handle (file);

    AUDDBG ("<%p> Field name: %s\n", h, field);

    if (! strcmp (field, "track-name") && h->icy_metadata.stream_title)
        return String (str_to_utf8 (h->icy_metadata.stream_title));

    if (! strcmp (field, "stream-name") && h->icy_metadata.stream_name)
        return String (str_to_utf8 (h->icy_metadata.stream_name));

    if (! strcmp (field, "content-type") && h->icy_metadata.stream_contenttype)
        return String (str_to_utf8 (h->icy_metadata.stream_contenttype));

    if (! strcmp (field, "content-bitrate"))
        return String (int_to_str (h->icy_metadata.stream_bitrate * 1000));

    return String ();
}

int64_t neon_vfs_fsize_impl (VFSFile * file)
{
    struct neon_handle * h = (neon_handle *) vfs_get_handle (file);

    if (h->content_length < 0)
    {
        AUDDBG ("<%p> Unknown content length\n", h);
        return -1;
    }

    return h->content_start + h->content_length;
}

static const char * const neon_schemes[] = {"http", "https", nullptr};

static const VFSConstructor constructor = {
    neon_vfs_fopen_impl,
    neon_vfs_fclose_impl,
    neon_vfs_fread_impl,
    neon_vfs_fwrite_impl,
    neon_vfs_fseek_impl,
    neon_vfs_ftell_impl,
    neon_vfs_feof_impl,
    neon_vfs_truncate_impl,
    neon_vfs_fsize_impl,
    neon_vfs_metadata_impl
};

#define AUD_PLUGIN_NAME        N_("Neon HTTP/HTTPS Plugin")
#define AUD_TRANSPORT_SCHEMES  neon_schemes
#define AUD_PLUGIN_INIT        neon_plugin_init
#define AUD_PLUGIN_CLEANUP     neon_plugin_fini
#define AUD_TRANSPORT_VTABLE   & constructor

#define AUD_DECLARE_TRANSPORT
#include <libaudcore/plugin-declare.h>
