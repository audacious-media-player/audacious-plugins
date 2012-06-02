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

#include <stdint.h>
#include <string.h>

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

#include "config.h"
#include "debug.h"
#include "rb.h"
#include "cert_verification.h"

#define NEON_BUFSIZE        (128u*1024u)
#define NEON_NETBLKSIZE     (4096u)
#define NEON_ICY_BUFSIZE    (4096)
#define NEON_RETRY_COUNT 6

static gboolean neon_plugin_init(void) {

    gint ret;

    if (0 != (ret = ne_sock_init())) {
        _ERROR("Could not initialize neon library: %d\n", ret);
        return FALSE;
    }

    return TRUE;
}

/*
 * -----
 */

static void neon_plugin_fini(void) {
    ne_sock_exit();
}

static struct neon_handle* handle_init(void) {

    struct neon_handle* h;

    if (NULL == (h = g_new0(struct neon_handle, 1))) {
        _ERROR("Could not allocate memory for handle");
        return NULL;
    }

    h->reader_status.mutex = g_mutex_new();
    h->reader_status.cond = g_cond_new();
    h->reader_status.reading = FALSE;
    h->reader_status.status = NEON_READER_INIT;

    if (0 != init_rb_with_lock(&(h->rb), NEON_BUFSIZE, h->reader_status.mutex)) {
        _ERROR("Could not initialize buffer");
        g_free(h);
        return NULL;
    }

    h->purl = g_new0(ne_uri, 1);
    h->content_length = -1;

    return h;
}

/*
 * -----
 */

static void handle_free(struct neon_handle* h) {
    _DEBUG("<%p> freeing handle", h);

    ne_uri_free(h->purl);
    g_free(h->purl);
    destroy_rb(&h->rb);

    if (h->reader_status.mutex != NULL)
        g_mutex_free(h->reader_status.mutex);
    if (h->reader_status.cond != NULL)
        g_cond_free(h->reader_status.cond);

    g_free(h->icy_metadata.stream_name);
    g_free(h->icy_metadata.stream_title);
    g_free(h->icy_metadata.stream_url);
    g_free(h->icy_metadata.stream_contenttype);
    g_free(h->url);
    g_free(h);
}

/*
 * -----
 */
static gboolean neon_strcmp(const gchar *str, const gchar *cmp)
{
    return (g_ascii_strncasecmp(str, cmp, strlen(cmp)) == 0);
}


static void add_icy(struct icy_metadata* m, gchar* name, gchar* value) {
    if (neon_strcmp(name, "StreamTitle")) {
        _DEBUG("Found StreamTitle: %s", value);
        g_free(m->stream_title);
        m->stream_title = g_strdup(value);
    }

    if (neon_strcmp(name, "StreamUrl")) {
        _DEBUG("Found StreamUrl: %s", value);
        g_free(m->stream_url);
        m->stream_url = g_strdup(value);
    }
}

/*
 * -----
 */

static void parse_icy(struct icy_metadata* m, gchar* metadata, gsize len) {

    gchar* p;
    gchar* tstart;
    gchar name[NEON_ICY_BUFSIZE];
    gchar value[NEON_ICY_BUFSIZE];
    gint state;
    gsize pos;

    p = metadata;
    state = 1;
    pos = 0;
    name[0] = '\0';
    value[0] = '\0';
    tstart = metadata;
    while ((pos < len) && (*p != '\0')) {
        switch (state) {
            case 1:
                /*
                 * Reading tag name
                 */
                if ('=' == *p) {
                    /*
                     * End of tag name.
                     */
                    *p = '\0';
                    g_strlcpy(name, tstart, NEON_ICY_BUFSIZE);
                    _DEBUG("Found tag name: %s", name);
                    state = 2;
                }
                break;
            case 2:
                /*
                 * Waiting for start of value
                 */
                if ('\'' == *p) {
                    /*
                     * Leading ' of value
                     */
                    tstart = p + 1;
                    state = 3;
                    value[0] = '\0';
                }
                break;
            case 3:
                /*
                 * Reading value
                 */
                if ('\'' == *p && ';' == *(p+1)) {
                    /*
                     * End of value
                     */
                    *p = '\0';
                    g_strlcpy(value, tstart, NEON_ICY_BUFSIZE);
                    _DEBUG("Found tag value: %s", value);
                    add_icy(m, name, value);
                    state = 4;
                }
                break;
            case 4:
                /*
                 * Waiting for next tag start
                 */
                if (';' == *p) {
                    /*
                     * Next tag name starts after this char
                     */
                    tstart = p + 1;
                    state = 1;
                    name[0] = '\0';
                    value[0] = '\0';
                }
                break;
            default:
                /*
                 * This should not happen
                 */
                _ERROR("Invalid state while parsing metadata, metadata may be corrupted: %d", state);
                return;
        }
        p++;
        pos++;
    }
}

/*
 * -----
 */

static void kill_reader(struct neon_handle* h) {
    if (NULL == h) {
        return;
    }

    _DEBUG("Signaling reader thread to terminate");
    g_mutex_lock(h->reader_status.mutex);
    h->reader_status.reading = FALSE;
    g_cond_signal(h->reader_status.cond);
    g_mutex_unlock(h->reader_status.mutex);

    _DEBUG("Waiting for reader thread to die...");
    g_thread_join(h->reader);
    _DEBUG("Reader thread has died");
    h->reader = NULL;
}

/*
 * -----
 */

static int server_auth_callback(void* userdata, const char* realm, int attempt, char* username, char* password) {

    struct neon_handle* h = (struct neon_handle*)userdata;
    gchar* authcpy;
    gchar** authtok;

    if ((NULL == h->purl->userinfo) || ('\0' == *(h->purl->userinfo))) {
        _ERROR("Authentication required, but no credentials set");
        return 1;
    }

    if (NULL == (authcpy = g_strdup(h->purl->userinfo))) {
        /*
         * No auth data
         */
        _ERROR("Could not allocate memory for authentication data");
        return 1;
    }

    authtok = g_strsplit(authcpy, ":", 2);
    if ((strlen(authtok[1]) > (NE_ABUFSIZ-1)) || (strlen(authtok[0]) > (NE_ABUFSIZ-1))) {
        _ERROR("Username/Password too long");
        g_strfreev(authtok);
        g_free(authcpy);
        return 1;
    }

    g_strlcpy(username, authtok[0], NE_ABUFSIZ);
    g_strlcpy(password, authtok[1], NE_ABUFSIZ);

    _DEBUG("Authenticating: Username: %s, Password: %s", username, password);

    g_strfreev(authtok);
    g_free(authcpy);

    return attempt;
}

/*
 * -----
 */

static void handle_headers(struct neon_handle* h) {

    const gchar* name;
    const gchar* value;
    void* cursor = NULL;
    long len;
    gchar* endptr;

    _DEBUG("Header responses:");
    while(NULL != (cursor = ne_response_header_iterate(h->request, cursor, &name, &value))) {
        _DEBUG("HEADER: %s: %s", name, value);
        if (neon_strcmp(name, "accept-ranges")) {
            /*
             * The server advertises range capability. we need "bytes"
             */
            if (NULL != g_strrstr(value, "bytes")) {
                _DEBUG("server can_ranges");
                h->can_ranges = TRUE;
            }

            continue;
        }

        if (neon_strcmp(name, "server")) {
            if (NULL != g_strrstr(value, "Twisted/")) {
                _DEBUG("ugh. this is a twisted server, and therefore cannot REALLY seek: %s", value);
                h->can_ranges = FALSE;
            }
        }

        if (neon_strcmp(name, "content-length")) {
            /*
             * The server sent us the content length. Parse and store.
             */
            len = strtol(value, &endptr, 10);
            if ((*value != '\0') && (*endptr == '\0') && (len >= 0)) {
                /*
                 * Valid data.
                 */
                _DEBUG("Content length as advertised by server: %ld", len);
                h->content_length = len;
            } else {
                _ERROR("Invalid content length header: %s", value);
            }

            continue;
        }

        if (neon_strcmp(name, "content-type")) {
            /*
             * The server sent us a content type. Save it for later
             */
            _DEBUG("Content-Type: %s", value);
            g_free(h->icy_metadata.stream_contenttype);
            h->icy_metadata.stream_contenttype = g_strdup(value);

            continue;
        }

        if (neon_strcmp(name, "icy-metaint")) {
            /*
             * The server sent us a ICY metaint header. Parse and store.
             */
            len = strtol(value, &endptr, 10);
            if ((*value != '\0') && (*endptr == '\0') && (len > 0)) {
                /*
                 * Valid data
                 */
                _DEBUG("ICY MetaInt as advertised by server: %ld", len);
                h->icy_metaint = len;
                h->icy_metaleft = len;
            } else {
                _ERROR("Invalid ICY MetaInt header: %s", value);
            }

            continue;
        }

        if (neon_strcmp(name, "icy-name")) {
            /*
             * The server sent us a ICY name. Save it for later
             */
            _DEBUG("ICY stream name: %s", value);
            g_free(h->icy_metadata.stream_name);
            h->icy_metadata.stream_name = g_strdup(value);
        }

        if (neon_strcmp(name, "icy-br")) {
            /*
             * The server sent us a bitrate. We might want to use it.
             */
            _DEBUG("ICY bitrate: %d", atoi(value));
            h->icy_metadata.stream_bitrate = atoi(value);
        }

        continue;
    }
}

/*
 * -----
 */

static int neon_proxy_auth_cb (void * userdata, const char * realm, int attempt,
 char * username, char * password)
{
    gchar * value = aud_get_string (NULL, "proxy_user");
    g_strlcpy (username, value, NE_ABUFSIZ);
    g_free (value);

    value = aud_get_string (NULL, "proxy_pass");
    g_strlcpy (password, value, NE_ABUFSIZ);
    g_free (value);

    return attempt;
}

/*
 * -----
 */

static int open_request(struct neon_handle* handle, gulong startbyte) {

    int ret;
    const ne_status* status;
    ne_uri* rediruri;

    g_return_val_if_fail(handle != NULL, -1);
    g_return_val_if_fail(handle->purl != NULL, -1);

    if (handle->purl->query && *(handle->purl->query)) {
        gchar *tmp = g_strdup_printf("%s?%s", handle->purl->path, handle->purl->query);
        handle->request = ne_request_create(handle->session, "GET", tmp);
        g_free(tmp);
    } else {
        handle->request = ne_request_create(handle->session, "GET", handle->purl->path);
    }

    if (0 < startbyte) {
        ne_print_request_header(handle->request, "Range", "bytes=%ld-", startbyte);
    }
    ne_print_request_header(handle->request, "Icy-MetaData", "1");

    /*
     * Try to connect to the server.
     */
    _DEBUG("<%p> Connecting...", handle);
    ret = ne_begin_request(handle->request);
    status = ne_get_status(handle->request);
    _DEBUG("<%p> Return: %d, Status: %d", handle, ret, status->code);
    if ((NE_OK == ret) && (401 == status->code)) {
        /*
         * Authorization required. Reconnect to
         * authenticate
         */
        _DEBUG("Reconnecting due to 401");
        ne_end_request(handle->request);
        ret = ne_begin_request(handle->request);
    }

    if ((NE_OK == ret) && ((301 == status->code) || (302 == status->code) || (303 == status->code) || (307 == status->code))) {
        /*
         * Redirect encountered. Reconnect.
         */
        ne_end_request(handle->request);
        ret = NE_REDIRECT;
    }

    if ((NE_OK == ret) && (407 == status->code)) {
        /*
         * Proxy auth required. Reconnect to authenticate
         */
        _DEBUG("Reconnecting due to 407");
        ne_end_request(handle->request);
        ret = ne_begin_request(handle->request);
    }

    switch (ret)
    {
        case NE_OK:
            if (status->code > 199 && status->code < 300)
            {
                /* URL opened OK */
                _DEBUG("<%p> URL opened OK", handle);
                handle->content_start = startbyte;
                handle->pos = startbyte;
                handle_headers(handle);
                return 0;
            }
            break;

        case NE_REDIRECT:
            /* We hit a redirect. Handle it. */
            _DEBUG("<%p> Redirect encountered", handle);
            handle->redircount += 1;
            rediruri = (ne_uri*)ne_redirect_location(handle->session);
            ne_request_destroy(handle->request);
            handle->request = NULL;

            if (NULL == rediruri) {
                _ERROR ("<%p> Could not parse redirect response", (void *)
                 handle);
                return -1;
            }
            ne_uri_free(handle->purl);
            ne_uri_copy(handle->purl, rediruri);
            return 1;
            break;
    }

    /* Something went wrong. */
    _ERROR ("<%p> Could not open URL: %d (%d)", (void *) handle, ret,
     status->code);

    if (ret)
        _ERROR ("<%p> neon error string: %s", (void *) handle,
         ne_get_error (handle->session));

    ne_request_destroy(handle->request);
    handle->request = NULL;
    return -1;
}

/*
 * -----
 */

static gint open_handle(struct neon_handle* handle, gulong startbyte) {

    gint ret;
    gchar* proxy_host = NULL;
    guint proxy_port = 0;

    gboolean use_proxy = aud_get_bool (NULL, "use_proxy");
    gboolean proxy_use_auth = aud_get_bool (NULL, "proxy_use_auth");

    if (use_proxy)
    {
        proxy_host = aud_get_string (NULL, "proxy_host");
        proxy_port = aud_get_int (NULL, "proxy_port");
    }

    handle->redircount = 0;

    _DEBUG("<%p> Parsing URL", handle);
    if (0 != ne_uri_parse(handle->url, handle->purl)) {
        _ERROR ("<%p> Could not parse URL '%s'", (void *) handle, handle->url);
        return -1;
    }

    while (handle->redircount < 10) {

        if (0 == handle->purl->port) {
            handle->purl->port = ne_uri_defaultport(handle->purl->scheme);
        }

        _DEBUG("<%p> Creating session to %s://%s:%d", handle, handle->purl->scheme, handle->purl->host, handle->purl->port);
        handle->session = ne_session_create(handle->purl->scheme, handle->purl->host, handle->purl->port);
        ne_redirect_register(handle->session);
        ne_add_server_auth(handle->session, NE_AUTH_BASIC, server_auth_callback, (void *)handle);
        ne_set_session_flag(handle->session, NE_SESSFLAG_ICYPROTO, 1);
        ne_set_session_flag(handle->session, NE_SESSFLAG_PERSIST, 0);

#ifdef HAVE_NE_SET_CONNECT_TIMEOUT
        ne_set_connect_timeout(handle->session, 10);
#endif

        ne_set_read_timeout(handle->session, 10);
        ne_set_useragent(handle->session, "Audacious/" PACKAGE_VERSION );

        if (use_proxy) {
            _DEBUG("<%p> Using proxy: %s:%d", handle, proxy_host, proxy_port);
            ne_session_proxy(handle->session, proxy_host, proxy_port);

            if (proxy_use_auth) {
                _DEBUG("<%p> Using proxy authentication", handle);
                ne_add_proxy_auth(handle->session, NE_AUTH_BASIC, neon_proxy_auth_cb, (void *)handle);
            }
        }

        if (! strcmp("https", handle->purl->scheme)) {
            ne_ssl_trust_default_ca(handle->session);
            ne_ssl_set_verify(handle->session, neon_vfs_verify_environment_ssl_certs, handle->session);
        }

        _DEBUG("<%p> Creating request", handle);
        ret = open_request(handle, startbyte);

        if (ret == 0)
        {
            g_free (proxy_host);
            return 0;
        }
        else if (ret == -1)
        {
            ne_session_destroy(handle->session);
            handle->session = NULL;
            g_free (proxy_host);
            return -1;
        }

        _DEBUG("<%p> Following redirect...", handle);
        ne_session_destroy(handle->session);
        handle->session = NULL;
    }

    /*
     * If we get here, our redirect count exceeded
     */

    _ERROR ("<%p> Redirect count exceeded for URL %s", (void *) handle,
     handle->url);

    g_free (proxy_host);
    return 1;
}

/*
 * -----
 */

static gint fill_buffer(struct neon_handle* h) {

    gssize bsize;
    gchar buffer[NEON_NETBLKSIZE];
    gssize to_read;

    bsize = free_rb(&h->rb);
    to_read = MIN(bsize, NEON_NETBLKSIZE);

    if (0 >= (bsize = ne_read_response_block(h->request, buffer, to_read))) {
        if (0 == bsize) {
            _DEBUG("<%p> End of file encountered", h);
            return 1;
        } else {
            _ERROR ("<%p> Error while reading from the network", (void *) h);
            ne_request_destroy(h->request);
            h->request = NULL;
            return -1;
        }
    }

    _DEBUG("<%p> Read %d bytes of %d", h, (gint) bsize, (gint) to_read);

    if (0 != write_rb(&(h->rb), buffer, bsize)) {
        _ERROR ("<%p> Error putting data into buffer", (void *) h);
        return -1;
    }

    return 0;
}

/*
 * -----
 */

static gpointer reader_thread(void* data) {

    struct neon_handle* h = (struct neon_handle*)data;
    gint ret;

    g_mutex_lock(h->reader_status.mutex);

    while(h->reader_status.reading) {

        /*
         * Hit the network only if we have more than NEON_NETBLKSIZE of free buffer
         */
        if (NEON_NETBLKSIZE < free_rb_locked(&h->rb)) {
            g_mutex_unlock(h->reader_status.mutex);

            ret = fill_buffer(h);

            g_mutex_lock(h->reader_status.mutex);

            /* Wake up main thread if it is waiting. */
            g_cond_signal (h->reader_status.cond);

            if (-1 == ret) {
                /*
                 * Error encountered while reading from the network.
                 * Set the error flag and terminate the
                 * reader thread.
                 */
                _ERROR ("<%p> Error while reading from the network. "
                 "Terminating reader thread", (void *) h);
                h->reader_status.status = NEON_READER_ERROR;
                g_mutex_unlock(h->reader_status.mutex);
                return NULL;
            } else if (1 == ret) {
                /*
                 * EOF encountered while reading from the
                 * network. Set the EOF status and exit.
                 */
                _DEBUG("<%p> EOF encountered while reading from the network. Terminating reader thread", h);
                h->reader_status.status = NEON_READER_EOF;
                g_mutex_unlock(h->reader_status.mutex);
                return NULL;
            }
        } else {
            /*
             * Not enough free space in the buffer.
             * Sleep until the main thread wakes us up.
             */
            g_cond_wait(h->reader_status.cond, h->reader_status.mutex);
        }
    }

    _DEBUG("<%p> Reader thread terminating gracefully", h);
    h->reader_status.status = NEON_READER_TERM;
    g_mutex_unlock(h->reader_status.mutex);

    return NULL;
}


/*
 * -----
 */

void * neon_vfs_fopen_impl (const gchar * path, const gchar * mode)
{
    struct neon_handle* handle;

    _DEBUG("Trying to open '%s' with neon", path);

    if (NULL == (handle = handle_init())) {
        _ERROR("Could not allocate memory for neon handle");
        return NULL;
    }

    _DEBUG("Allocated new handle: %p", handle);

    handle->url = g_strdup (path);

    if (0 != open_handle(handle, 0)) {
        _ERROR ("<%p> Could not open URL", (void *) handle);
        handle_free(handle);
        return NULL;
    }

    return handle;
}

/*
 * ----
 */

gint neon_vfs_fclose_impl(VFSFile* file) {

    struct neon_handle* h = (struct neon_handle *)vfs_get_handle (file);

    if (NULL != h->reader) {
        kill_reader(h);
    }

    _DEBUG("<%p> Destroying request", h);
    if (NULL != h->request) {
        ne_request_destroy(h->request);
    }

    _DEBUG("<%p> Destroying session", h);
    if (NULL != h->session) {
        ne_session_destroy(h->session);
    }

    handle_free(h);

    return 0;
}

/*
 * -----
 */

static gint64 neon_fread_real (void * ptr_, gint64 size, gint64 nmemb,
 VFSFile * file)
{
    struct neon_handle* h = (struct neon_handle*)vfs_get_handle (file);
    gint belem;
    gint relem;
    gint ret;
    gchar icy_metadata[NEON_ICY_BUFSIZE];
    guchar icy_metalen;
    gint retries;

    if (NULL == h->request) {
        _ERROR ("<%p> No request to read from, seek gone wrong?", (void *) h);
        return 0;
    }

    if (h->eof)
        return 0;

    /* If the buffer is empty, wait for the reader thread to fill it. */
    g_mutex_lock (h->reader_status.mutex);

    for (retries = 0; retries < NEON_RETRY_COUNT; retries ++)
    {
        if (used_rb_locked (& h->rb) / size > 0 || h->reader == NULL ||
         h->reader_status.status != NEON_READER_RUN)
            break;

        g_cond_signal (h->reader_status.cond);
        g_cond_wait (h->reader_status.cond, h->reader_status.mutex);
    }

    g_mutex_unlock (h->reader_status.mutex);

    if (NULL == h->reader) {
        if ((NEON_READER_EOF != h->reader_status.status) ||
            ((NEON_READER_EOF == h->reader_status.status) && (h->content_length != -1))) {
            /*
             * There is no reader thread yet. Read the first bytes from
             * the network ourselves, and then fire up the reader thread
             * to keep the buffer filled up.
             */
            _DEBUG("<%p> Doing initial buffer fill", h);
            ret = fill_buffer (h);

            if (-1 == ret) {
                _ERROR ("<%p> Error while reading from the network", (void *) h);
                return 0;
            } else if (1 == ret) {
                _DEBUG("<%p> EOF during initial read", h);
            }

            /*
             * We have some data in the buffer now.
             * Start the reader thread if we did not reach EOF during
             * the initial fill
             */
            g_mutex_lock(h->reader_status.mutex);
            if (0 == ret) {
                h->reader_status.reading = TRUE;
                _DEBUG("<%p> Starting reader thread", h);
                if (NULL == (h->reader = g_thread_create(reader_thread, h, TRUE, NULL))) {
                    h->reader_status.reading = FALSE;
                    g_mutex_unlock(h->reader_status.mutex);
                    _ERROR ("<%p> Error creating reader thread!", (void *) h);
                    return 0;
                }
                h->reader_status.status = NEON_READER_RUN;
            } else {
                _DEBUG("<%p> No reader thread needed (stream has reached EOF during fill)", h);
                h->reader_status.reading = FALSE;
                h->reader_status.status = NEON_READER_EOF;
            }
            g_mutex_unlock(h->reader_status.mutex);
        }
    } else {
        /*
         * There already is a reader thread. Look if it is in good
         * shape.
         */
        g_mutex_lock(h->reader_status.mutex);

        switch (h->reader_status.status) {
            case NEON_READER_INIT:
            case NEON_READER_RUN:
                /*
                 * All is well, nothing to be done.
                 */
                break;
            case NEON_READER_ERROR:
                /*
                 * A reader error happened. Log it, and treat it like an EOF condition, by falling through
                 * to the NEON_READER_EOF codepath.  --nenolod
                 */
                _DEBUG("<%p> NEON_READER_ERROR happened. Terminating reader thread and marking EOF.", h);
                h->reader_status.status = NEON_READER_EOF;
                g_mutex_unlock(h->reader_status.mutex);

                if (NULL != h->reader)
                    kill_reader(h);

                g_mutex_lock(h->reader_status.mutex);

            case NEON_READER_EOF:
                /*
                 * If there still is data in the buffer, carry on.
                 * If not, terminate the reader thread and return 0.
                 */
                if (0 == used_rb_locked(&h->rb)) {
                    _DEBUG("<%p> Reached end of stream", h);
                    g_mutex_unlock(h->reader_status.mutex);

                    if (NULL != h->reader)
                        kill_reader(h);

                    h->eof = TRUE;
                    return 0;
                }
                break;
            case NEON_READER_TERM:
                /*
                 * The reader thread terminated gracefully, most
                 * likely on our own request.
                 * We should not get here.
                 */
                _ERROR ("<%p> Reader thread terminated and fread() called. How "
                 "did we get here?", (void *) h);
                g_mutex_unlock(h->reader_status.mutex);
                kill_reader(h);
                return 0;
        }
        g_mutex_unlock(h->reader_status.mutex);
    }

    /*
     * Deliver data from the buffer
     */
    if (0 == used_rb(&h->rb)) {
        /*
         * The buffer is still empty, we can deliver no data!
         */
        _ERROR ("<%p> Buffer still underrun, fatal.", (void *) h);
        return 0;
    }

    if (0 != h->icy_metaint) {
        if (0 == h->icy_metaleft) {
            /*
             * The next data in the buffer is a ICY metadata announcement.
             * Get the length byte
             */
            read_rb(&h->rb, &icy_metalen, 1);

            /*
             * We need enough data in the buffer to
             * a) Read the complete ICY metadata block
             * b) deliver at least one byte to the reader
             */
            _DEBUG("<%p> Expecting %d bytes of ICY metadata", h, (icy_metalen*16));

            if ((free_rb(&h->rb)-(icy_metalen*16)) < size) {
                /* There is not enough data. We do not have much choice at this point,
                 * so we'll deliver the metadata as normal data to the reader and
                 * hope for the best.
                 */
                _ERROR ("<%p> Buffer underrun when reading metadata. Expect "
                 "audio degradation", (void *) h);
                h->icy_metaleft = h->icy_metaint + (icy_metalen*16);
            } else {
                /*
                 * Grab the metadata from the buffer and send it to the parser
                 */
                read_rb(&h->rb, icy_metadata, (icy_metalen*16));
                parse_icy(&h->icy_metadata, icy_metadata, (icy_metalen*16));
                h->icy_metaleft = h->icy_metaint;
            }
        }

        /*
         * The maximum number of bytes we can deliver is determined
         * by the number of bytes left until the next metadata announcement
         */
        belem = MIN(used_rb(&h->rb), h->icy_metaleft) / size;
    } else {
        belem = used_rb(&h->rb) / size;
    }

    relem = MIN(belem, nmemb);
    read_rb(&h->rb, ptr_, relem*size);

    /*
     * Signal the network thread to continue reading
     */
    g_mutex_lock(h->reader_status.mutex);
    if (NEON_READER_EOF == h->reader_status.status) {
        if (0 == free_rb_locked(&h->rb)) {
            _DEBUG("<%p> stream EOF reached and buffer empty", h);
            h->eof = TRUE;
        }
    }
    else
        g_cond_signal(h->reader_status.cond);

    g_mutex_unlock(h->reader_status.mutex);

    h->pos += (relem*size);
    h->icy_metaleft -= (relem*size);

    return relem;
}

/* neon_fread_real will do only a partial read if the buffer underruns, so we
 * must call it repeatedly until we have read the full request. */
gint64 neon_vfs_fread_impl (void * buffer, gint64 size, gint64 count,
 VFSFile * handle)
{
    gsize total = 0, new;

    _DEBUG ("<%p> fread %d x %d", (void *) handle, (gint) size, (gint) count);

    while (count > 0 && (new = neon_fread_real (buffer, size, count, handle)) > 0)
    {
        buffer = (gchar *) buffer + size * new;
        total += new;
        count -= new;
    }

    _DEBUG ("<%p> fread = %d", (void *) handle, (gint) total);

    return total;
}

/*
 * -----
 */

gint64 neon_vfs_fwrite_impl (const void * ptr, gint64 size, gint64 nmemb,
 VFSFile * file)
{
    _ERROR ("<%p> NOT IMPLEMENTED", (void *) vfs_get_handle (file));

    return 0;
}

/*
 * -----
 */

gint neon_vfs_getc_impl(VFSFile* file) {
  unsigned char c;

    if (1 != neon_vfs_fread_impl(&c, 1, 1, file)) {
        _ERROR ("<%p> Could not getc()!", (void *) vfs_get_handle (file));
        return -1;
    }

    return c;
}

/*
 * -----
 */

gint neon_vfs_ungetc_impl(gint c, VFSFile* stream) {
    _ERROR ("<%p> NOT IMPLEMENTED", (void *) vfs_get_handle (stream));

    return 0;
}

gint64 neon_vfs_ftell_impl (VFSFile * file)
{
    struct neon_handle* h = (struct neon_handle *)vfs_get_handle (file);

    _DEBUG("<%p> Current file position: %ld", h, h->pos);

    return h->pos;
}

/*
 * -----
 */

gboolean neon_vfs_feof_impl(VFSFile* file) {

    struct neon_handle* h = (struct neon_handle*)vfs_get_handle (file);

    _DEBUG("<%p> EOF status: %s", h, h->eof?"TRUE":"FALSE");

    return h->eof;
}

/*
 * -----
 */

gint neon_vfs_truncate_impl (VFSFile * file, gint64 size)
{
    _ERROR ("<%p> NOT IMPLEMENTED", (void *) vfs_get_handle (file));

    return 0;
}

/*
 * -----
 */

gint neon_vfs_fseek_impl (VFSFile * file, gint64 offset, gint whence)
{
    struct neon_handle* h = (struct neon_handle*)vfs_get_handle (file);
    glong newpos;
    glong content_length;

    _DEBUG("<%p> Seek requested: offset %ld, whence %d", h, offset, whence);
    /*
     * Two things must be satisfied for us to be able to seek:
     * - the server must advertise a content-length
     * - the server must advertise accept-ranges: bytes
     */
    if ((-1 == h->content_length) || !h->can_ranges) {
        _DEBUG("<%p> Can not seek due to server restrictions", h);
        return -1;
    }

    content_length = h->content_length + h->content_start;

    switch (whence) {
        case SEEK_SET:
            newpos = offset;
            break;
        case SEEK_CUR:
            newpos = h->pos + offset;
            break;
        case SEEK_END:
            if (offset == 0) {
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

    _DEBUG("<%p> Position to seek to: %ld, current: %ld", h, newpos, h->pos);
    if (0 > newpos) {
        _ERROR ("<%p> Can not seek before start of stream", (void *) h);
        return -1;
    }

    if (newpos >= content_length) {
        _ERROR ("<%p> Can not seek beyond end of stream (%ld >= %ld)", (void *)
         h, newpos, content_length);
        return -1;
    }

    if (newpos == h->pos) {
        return 0;
    }

    /*
     * To seek to the new position we have to
     * - stop the current reader thread, if there is one
     * - destroy the current request
     * - dump all data currently in the ringbuffer
     * - create a new request starting at newpos
     */
    if (NULL != h->reader) {
        /*
         * There may be a thread still running.
         */
        kill_reader(h);
    }

    if (NULL != h->request) {
        ne_request_destroy(h->request);
    }
    if (NULL != h->session) {
        ne_session_destroy(h->session);
        h->session = NULL;
    }
    reset_rb(&h->rb);

    if (0 != open_handle(h, newpos)) {
        /*
         * Something went wrong while creating the new request.
         * There is not much we can do now, we'll set the request
         * to NULL, so that fread() will error out on the next
         * read request
         */
        _ERROR ("<%p> Error while creating new request!", (void *) h);
        h->request = NULL;
        return -1;
    }

    /*
     * Things seem to have worked. The next read request will start
     * the reader thread again.
     */
    h->eof = FALSE;

    return 0;
}

void neon_vfs_rewind_impl(VFSFile* file) {
    (void)neon_vfs_fseek_impl(file, 0L, SEEK_SET);
}

/*
 * -----
 */

gchar *neon_vfs_metadata_impl(VFSFile* file, const gchar* field) {

    struct neon_handle* h = (struct neon_handle*)vfs_get_handle (file);

    _DEBUG("<%p> Field name: %s", h, field);

    if (! strcmp (field, "track-name"))
        return str_to_utf8 (h->icy_metadata.stream_title);
    if (! strcmp (field, "stream-name"))
        return str_to_utf8 (h->icy_metadata.stream_name);
    if (! strcmp (field, "content-type"))
        return str_to_utf8 (h->icy_metadata.stream_contenttype);
    if (! strcmp (field, "content-bitrate"))
        return g_strdup_printf ("%d", h->icy_metadata.stream_bitrate * 1000);

    return NULL;
}

/*
 * -----
 */

gint64 neon_vfs_fsize_impl (VFSFile * file)
{
    struct neon_handle* h = (struct neon_handle*)vfs_get_handle (file);

    if (-1 == h->content_length) {
        _DEBUG("<%p> Unknown content length", h);
        return -1;
    }

    return (h->content_start + h->content_length);
}

static const gchar * const neon_schemes[] = {"http", "https", NULL};

static VFSConstructor constructor = {
 .vfs_fopen_impl = neon_vfs_fopen_impl,
 .vfs_fclose_impl = neon_vfs_fclose_impl,
 .vfs_fread_impl = neon_vfs_fread_impl,
 .vfs_fwrite_impl = neon_vfs_fwrite_impl,
 .vfs_getc_impl = neon_vfs_getc_impl,
 .vfs_ungetc_impl = neon_vfs_ungetc_impl,
 .vfs_fseek_impl = neon_vfs_fseek_impl,
 .vfs_rewind_impl = neon_vfs_rewind_impl,
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
