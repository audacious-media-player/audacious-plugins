#include <audacious/vfs.h>
#include <audacious/plugin.h>

#include <ne_socket.h>
#include <ne_utils.h>
#include <ne_redirect.h>
#include <ne_request.h>
#include <ne_auth.h>

#include "debug.h"
#include "neon.h"
#include "rb.h"

#define NBUFSIZ (128u*1024u)
#define NETBLKSIZ (4096u)

DECLARE_PLUGIN(neon, init, fini)

VFSConstructor neon_http_const = {
    "http://",
    neon_vfs_fopen_impl,
    neon_vfs_fclose_impl,
    neon_vfs_fread_impl,
    neon_vfs_fwrite_impl,
    neon_vfs_getc_impl,
    neon_vfs_ungetc_impl,
    neon_vfs_fseek_impl,
    neon_vfs_rewind_impl,
    neon_vfs_ftell_impl,
    neon_vfs_feof_impl,
    neon_vfs_truncate_impl,
    neon_vfs_fsize_impl,
    neon_vfs_metadata_impl
};

/*
 * ========
 */

static struct neon_handle* handle_init(void) {

    struct neon_handle* h;

    _ENTER;

    if (NULL == (h = malloc(sizeof(struct neon_handle)))) {
        _ERROR("Could not allocate memory for handle");
        _LEAVE NULL;
    }

    if (0 != init_rb(&(h->rb), NBUFSIZ)) {
        _ERROR("Could not initialize buffer");
        free(h);
        _LEAVE NULL;
    }

    h->url = NULL;
    h->purl = &purl;
    memset(h->purl, 0, sizeof(ne_uri));
    h->session = NULL;
    h->request = NULL;
    h->redircount = 0;
    h->pos = 0;
    h->content_length = -1;
    h->can_ranges = FALSE;
    h->icy_metaint = 0;
    h->icy_metaleft = 0;
    h->icy_metadata.stream_name = NULL;
    h->icy_metadata.stream_title = NULL;
    h->icy_metadata.stream_url = NULL;
    h->reader = NULL;
    h->reader_status.mutex = g_mutex_new();
    h->reader_status.cond = g_cond_new();
    h->reader_status.reading = FALSE;
    h->reader_status.status = NEON_READER_INIT;
    h->eof = FALSE;

    _LEAVE h;
}

/*
 * -----
 */

static void handle_free(struct neon_handle* h) {

    _ENTER;

    ne_uri_free(h->purl);
    destroy_rb(&h->rb);
    free(h);

    _LEAVE;
}

/*
 * ----
 */

static void init(void) {

    int ret;

    _ENTER;

    if (0 != (ret = ne_sock_init())) {
        _ERROR("Could not initialize neon library: %d\n", ret);
        _LEAVE;
    }

    vfs_register_transport(&neon_http_const);

    if (0 != ne_has_support(NE_FEATURE_SSL)) {
        _DEBUG("neon compiled with thread-safe SSL, enabling https:// transport");
    }

    _LEAVE;
}

/*
 * -----
 */

static void fini(void) {

    _ENTER;

    ne_sock_exit();

    _LEAVE;
}

/*
 * -----
 */

static void add_icy(struct icy_metadata* m, gchar* name, gchar* value) { 

    _ENTER;

    if (0 == g_ascii_strncasecmp(name, "StreamTitle", 11)) {
        _DEBUG("Found StreamTitle: %s", value);
        if (NULL != m->stream_title) {
            free(m->stream_title);
        }
        m->stream_title = g_strdup(value);
    }

    if (0 == g_ascii_strncasecmp(name, "StreamUrl", 9)) {
        _DEBUG("Found StreamUrl: %s", value);
        if (NULL != m->stream_url) {
            free(m->stream_url);
        }
        m->stream_url = g_strdup(value);
    }

    _LEAVE;
}

/*
 * -----
 */

#define TAGSIZE 4096

static void parse_icy(struct icy_metadata* m, gchar* metadata, int len) {

    gchar* p;
    gchar* tstart;
    gchar* tend;
    gchar name[TAGSIZE];
    gchar value[TAGSIZE];
    int state;
    int pos;

    _ENTER;

    p = metadata;
    state = 1;
    pos = 0;
    name[0] = '\0';
    value[0] = '\0';
    tstart = metadata;
    tend = metadata;
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
                    g_strlcpy(name, tstart, TAGSIZE);
                    _DEBUG("Found tag name: %s", name);
                    state = 2;
                } else {
                    tend = p;
                };
                break;
            case 2:
                /*
                 * Waiting for start of value
                 */
                if ('\'' == *p) {
                    /*
                     * Leading ' of value
                     */
                    tend = tstart = p + 1;
                    state = 3;
                    value[0] = '\0';
                }
                break;
            case 3:
                /*
                 * Reading value
                 */
                if ('\'' == *p) {
                    /*
                     * End of value
                     */
                    *p = '\0';
                    g_strlcpy(value, tstart, TAGSIZE);
                    _DEBUG("Found tag value: %s", value);
                    add_icy(m, name, value);
                    state = 4;
                } else {
                    tend = p;
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
                    tend = tstart = p + 1;
                    state = 1;
                    name[0] = '\0';
                    value[0] = '\0';
                }
                break;
        }
        p++;
        pos++;
    }

    _LEAVE;
}

/*
 * -----
 */

static void kill_reader(struct neon_handle* h) {

    _ENTER;

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

static int auth_callback(void* userdata, const char* realm, int attempt, char* username, char* password) {

    struct neon_handle* h = (struct neon_handle*)userdata;
    gchar* authcpy;
    gchar** authtok;

    _ENTER;

    if ((NULL == h->purl->userinfo) || ('\0' == *(h->purl->userinfo))) {
        _ERROR("Authentication required, but no credentials set");
        _LEAVE 1;
    }

    if (NULL == (authcpy = g_strdup(h->purl->userinfo))) {
        /*
         * No auth data
         */
        _ERROR("Could not allocate memory for authentication data");
        _LEAVE 1;
    }

    authtok = g_strsplit(authcpy, ":", 2);
    if ((strlen(authtok[1]) > (NE_ABUFSIZ-1)) || (strlen(authtok[0]) > (NE_ABUFSIZ-1))) {
        _ERROR("Username/Password too long");
        g_strfreev(authtok);
        free(authcpy);
        _LEAVE 1;
    }

    strncpy(username, authtok[0], NE_ABUFSIZ);
    strncpy(password, authtok[1], NE_ABUFSIZ);

    _DEBUG("Authenticating: Username: %s, Password: %s", username, password);

    g_strfreev(authtok);
    free(authcpy);

    _LEAVE attempt;
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

    _ENTER;

    _DEBUG("Header responses:");
    while(NULL != (cursor = ne_response_header_iterate(h->request, cursor, &name, &value))) {
        _DEBUG("HEADER: %s: %s", name, value);
        if (0 == g_ascii_strncasecmp("accept-ranges", name, 13)) {
            /*
             * The server advertises range capability. we need "bytes"
             */
            if (NULL != g_strrstr(value, "bytes")) {
                _DEBUG("server can_ranges");
                h->can_ranges = TRUE;
            }
        }

        if (0 == g_ascii_strncasecmp("content-length", name, 14)) {
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
        }

        if (0 == g_ascii_strncasecmp("icy-metaint", name, 11)) {
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
        }

        if (0 == g_ascii_strncasecmp("icy-name", name, 8)) {
            /*
             * The server sent us a ICY name. Save it for later
             */
            _DEBUG("ICY stream name: %s", value);
            if (NULL != h->icy_metadata.stream_name) {
                free(h->icy_metadata.stream_name);
            }
            h->icy_metadata.stream_name = g_strdup(value);
        }
    }

    _LEAVE;
}

/*
 * -----
 */

static int open_request(struct neon_handle* handle, unsigned long startbyte) {

    int ret;
    const ne_status* status;

    _ENTER;

    handle->request = ne_request_create(handle->session, "GET", handle->purl->path);
    ne_print_request_header(handle->request, "Range", "bytes=%ld-", startbyte);
    ne_print_request_header(handle->request, "Icy-MetaData", "1");

    /*
     * Try to connect to the server.
     */
    _DEBUG("Connecting...");
    ret = ne_begin_request(handle->request);
    status = ne_get_status(handle->request);
    if ((NE_OK == ret) && (401 == status->code)) {
        /*
         * Authorization required. Reconnect to
         * authenticate
         */
        _DEBUG("Reconnecting due to 401");
        ne_end_request(handle->request);
        ret = ne_begin_request(handle->request);
    }

    switch (ret) {
        case NE_OK:
            /* URL opened OK */
            _DEBUG("URL opened OK");
            handle->content_start = startbyte;
            handle->pos = startbyte;
            handle_headers(handle);
            _LEAVE 0;
            break;

        case NE_REDIRECT:
            /* We hit a redirect. Handle it. */
            _DEBUG("Redirect encountered");
            handle->redircount += 1;
            handle->purl = ne_redirect_location(handle->session);
            ne_request_destroy(handle->request);
            if (NULL == handle->purl) {
                _ERROR("Could not parse redirect response");
                _LEAVE -1;
            }
            _LEAVE 1;
            break;

        default:
            /* Something went wrong. */
            _ERROR("Could not open URL: %d", ret);
            if (1 == ret) {
                _ERROR("neon error string: %s", ne_get_error(handle->session));
            }
            ne_request_destroy(handle->request);
            _LEAVE -1;
            break;
    }
}

/*
 * -----
 */

static int open_handle(struct neon_handle* handle, unsigned long startbyte) {

    int ret;

    _ENTER;

    handle->redircount = 0;

    _DEBUG("Parsing URL");
    if (0 != ne_uri_parse(handle->url, handle->purl)) {
        _ERROR("Could not parse URL '%s'", handle->url);
        _LEAVE -1;
    }

    if (0 == handle->purl->port) {
        handle->purl->port = 80;
    }

    while (handle->redircount < 10) {

        _DEBUG("Creating session");
        handle->session = ne_session_create(handle->purl->scheme, handle->purl->host, handle->purl->port);
        ne_add_server_auth(handle->session, NE_AUTH_BASIC, auth_callback, (void *)handle);
        ne_set_session_flag(handle->session, NE_SESSFLAG_ICYPROTO, 1);
        ne_set_session_flag(handle->session, NE_SESSFLAG_PERSIST, 0);
        ne_set_connect_timeout(handle->session, 10);
        ne_set_read_timeout(handle->session, 10);
        ne_set_useragent(handle->session, "Audacious/1.4.0");
        ne_redirect_register(handle->session);

        _DEBUG("Creating request");
        ret = open_request(handle, startbyte);

        if (0 == ret) {
            _LEAVE 0;
        } else if (-1 == ret) {
            ne_session_destroy(handle->session);
            _LEAVE -1;
        }
    }

    /*
     * If we get here, our redirect count exceeded
     */

    _ERROR("Redirect count exceeded for URL");

    _LEAVE 1;
}

/*
 * -----
 */

static int fill_buffer(struct neon_handle* h) {

    ssize_t bsize;
    char buffer[NETBLKSIZ];
    ssize_t to_read;

    _ENTER;

    bsize = free_rb(&h->rb);
    to_read = MIN(bsize, NETBLKSIZ);

    _DEBUG("%d bytes free in buffer, trying to read %d bytes max", bsize, to_read);

    _DEBUG("Reading from the network....");
    if (0 >= (bsize = ne_read_response_block(h->request, buffer, to_read))) {
        if (0 == bsize) {
            _DEBUG("End of file encountered");
            _LEAVE 1;
        } else {
            _ERROR("Error while reading from the network");
            _LEAVE -1;
        }
    }
    _DEBUG("Read %d bytes from the network", bsize);

    if (0 != write_rb(&(h->rb), buffer, bsize)) {
        _ERROR("Error putting data into buffer");
        _LEAVE -1;
    }

    _LEAVE 0;
}

/*
 * -----
 */

static int fill_buffer_limit(struct neon_handle* h, unsigned int maxfree) {

    ssize_t bfree;
    int ret;

    _ENTER;

    bfree = free_rb(&h->rb);
    _DEBUG("Filling buffer up to max %d bytes free, %d bytes free now", maxfree, bfree);

    while (bfree > maxfree) {
        ret = fill_buffer(h);
        if (-1 == ret) {
            _ERROR("Error while filling buffer");
            _LEAVE ret;
        } else if (1 == ret) {
            /*
             * EOF while filling the buffer. Return what we have.
             */
            _LEAVE 0;
        }

        bfree = free_rb(&h->rb);
    }

    _LEAVE 0;
}

/*
 * -----
 */

static gpointer reader_thread(void* data) {

    struct neon_handle* h = (struct neon_handle*)data;
    int ret;

    _ENTER;

    g_mutex_lock(h->reader_status.mutex);

    while(h->reader_status.reading) {
        g_mutex_unlock(h->reader_status.mutex);

        /*
         * Hit the network only if we have more than NETBLKSIZ of free buffer
         */
        if (NETBLKSIZ < free_rb(&h->rb)) {

            _DEBUG("Filling buffer...");
            ret = fill_buffer(h);

            g_mutex_lock(h->reader_status.mutex);
            if (-1 == ret) {
                /*
                 * Error encountered while reading from the network.
                 * Set the error flag and terminate the
                 * reader thread.
                 */
                _DEBUG("Error while reading from the network. Terminating reader thread");
                h->reader_status.status = NEON_READER_ERROR;
                g_mutex_unlock(h->reader_status.mutex);
                _LEAVE NULL;
            } else if (1 == ret) {
                /*
                 * EOF encountered while reading from the
                 * network. Set the EOF status and exit.
                 */
                _DEBUG("EOF encountered while reading from the network. Terminating reader thread");
                h->reader_status.status = NEON_READER_EOF;
                g_mutex_unlock(h->reader_status.mutex);
                _LEAVE NULL;
            }

            /*
             * So we actually got some data out of the stream.
             */
            _DEBUG("Network read succeeded");
        } else {
            /*
             * Not enough free space in the buffer.
             * Sleep until the main thread wakes us up.
             */
            g_mutex_lock(h->reader_status.mutex);
            if (h->reader_status.reading) {
                _DEBUG("Reader thread going to sleep");
                g_cond_wait(h->reader_status.cond, h->reader_status.mutex);
                _DEBUG("Reader thread woke up");
            } else {
                /*
                 * Main thread has ordered termination of this thread.
                 * Leave the loop.
                 */
                break;
            }
        }
    }

    _DEBUG("Reader thread terminating gracefully");
    h->reader_status.status = NEON_READER_TERM;
    g_mutex_unlock(h->reader_status.mutex);

    _LEAVE NULL;
}

/*
 * -----
 */

VFSFile* neon_vfs_fopen_impl(const gchar* path, const gchar* mode) {

    VFSFile* file;
    struct neon_handle* handle;

    _ENTER;

    _DEBUG("Trying to open '%s' with neon", path);

    if (NULL == (file = malloc(sizeof(VFSFile)))) {
        _ERROR("Could not allocate memory for filehandle");
        _LEAVE NULL;
    }

    if (NULL == (handle = handle_init())) {
        _ERROR("Could not allocate memory for neon handle");
        free(file);
        _LEAVE NULL;
    }

    if (NULL == (handle->url = strdup(path))) {
        _ERROR("Could not copy URL string");
        handle_free(handle);
        free(file);
        _LEAVE NULL;
    }

    if (0 != open_handle(handle, 0)) {
        _ERROR("Could not open URL");
        handle_free(handle);
        free(file);
        _LEAVE NULL;
    }

    file->handle = handle;
    file->base = &neon_http_const;

    _LEAVE file;
}

/*
 * ----
 */

gint neon_vfs_fclose_impl(VFSFile* file) {

    struct neon_handle* h = (struct neon_handle *)file->handle;

    _ENTER;

    if (NULL != h->reader) {
        kill_reader(h);
    }

    _DEBUG("Destroying request");
    if (NULL != h->request) {
        ne_request_destroy(h->request);
    }

    _DEBUG("Destroying session");
    ne_session_destroy(h->session);

    handle_free(h);

    _LEAVE 0;
}

/*
 * -----
 */

size_t neon_vfs_fread_impl(gpointer ptr_, size_t size, size_t nmemb, VFSFile* file) {

    struct neon_handle* h = (struct neon_handle*)file->handle;
    int belem;
    int relem;
    int ret;
    char icy_metadata[4096];
    unsigned char icy_metalen;

    _ENTER;

    if (NULL == h->request) {
        _ERROR("No request to read from, seek gone wrong?");
        _LEAVE 0;
    }

    _DEBUG("Requesting %d elements of %d bytes size each (%d bytes total), to be stored at %p",
            nmemb, size, (nmemb*size), ptr_);

    /*
     * Look how much data is in the buffer
     */
    belem = used_rb(&h->rb) / size;

    if ((NULL != h->reader) && (0 == belem)) {
        /*
         * There is a reader thread, but the buffer is empty.
         * If we are running normally we will have to rebuffer.
         * Kill the reader thread and restart.
         */
        g_mutex_lock(h->reader_status.mutex);
        if (NEON_READER_RUN == h->reader_status.status) {
            g_mutex_unlock(h->reader_status.mutex);
            _ERROR("Buffer underrun, trying rebuffering");
            kill_reader(h);

            /*
             * We have to check if the reader terminated gracefully
             * again
             */
            if (NEON_READER_TERM != h->reader_status.status) {
                /*
                 * Reader thread did not terminate gracefully.
                 */
                _LEAVE 0;
            }
        } else {
            g_mutex_unlock(h->reader_status.mutex);
        }
    }

    if (NULL == h->reader) {
        /*
         * There is no reader thread yet. Read the first bytes from
         * the network ourselves, and then fire up the reader thread
         * to keep the buffer filled up.
         */
        _DEBUG("Doing initial buffer fill");
        ret = fill_buffer_limit(h, NBUFSIZ/2);

        if (-1 == ret) {
            _ERROR("Error while reading from the network");
            _LEAVE 0;
        } else if (1 == ret) {
            _ERROR("EOF during initial read");
            _LEAVE 0;
        }

        /*
         * We have some data in the buffer now.
         * Start the reader thread.
         */
        h->reader_status.reading = TRUE;
        if (NULL == (h->reader = g_thread_create(reader_thread, h, TRUE, NULL))) {
            h->reader_status.reading = FALSE;
            _ERROR("Error creating reader thread!");
            _LEAVE 0;
        }
        g_mutex_lock(h->reader_status.mutex);
        h->reader_status.status = NEON_READER_RUN;
        g_mutex_unlock(h->reader_status.mutex);
    } else {
        /*
         * There already is a reader thread. Look if it is in good
         * shape.
         */
        g_mutex_lock(h->reader_status.mutex);
        _DEBUG("Reader thread status: %d", h->reader_status.status);
        switch (h->reader_status.status) {
            case NEON_READER_INIT:
            case NEON_READER_RUN:
                /*
                 * All is well, nothing to be done.
                 */
                break;
            case NEON_READER_EOF:
                /*
                 * If there still is data in the buffer, carry on.
                 * If not, terminate the reader thread and return 0.
                 */
                if (0 == used_rb(&h->rb)) {
                    _DEBUG("Reached end of stream");
                    g_mutex_unlock(h->reader_status.mutex);
                    kill_reader(h);
                    h->eof = TRUE;
                    _LEAVE 0;
                }
                break;
            case NEON_READER_ERROR:
                /* Terminate the reader and return 0 */
                g_mutex_unlock(h->reader_status.mutex);
                kill_reader(h);
                _LEAVE 0;
                break;
            case NEON_READER_TERM:
                /*
                 * The reader thread terminated gracefully, most
                 * likely on our own request.
                 * We should not get here.
                 */
                _ERROR("Reader thread terminated and fread() called. How did we get here?");
                g_mutex_unlock(h->reader_status.mutex);
                kill_reader(h);
                _LEAVE 0;
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
        _ERROR("Buffer still underrun, fatal.");
        _LEAVE 0;
    }

    if (0 != h->icy_metaint) {
        _DEBUG("%ld bytes left before next ICY metadata announcement", h->icy_metaleft);
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
            _DEBUG("Expecting %d bytes of ICY metadata", (icy_metalen*16));

            if ((free_rb(&h->rb)-(icy_metalen*16)) < size) {
                /* There is not enough data. We do not have much choice at this point,
                 * so we'll deliver the metadata as normal data to the reader and
                 * hope for the best.
                 */
                _ERROR("Buffer underrun when reading metadata. Expect audio degradation");
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
        belem = h->icy_metaleft / size;
    } else {
        belem = used_rb(&h->rb) / size;
    }

    relem = MIN(belem, nmemb);
    _DEBUG("%d elements of returnable data in the buffer", belem);
    read_rb(&h->rb, ptr_, relem*size);

    /*
     * Signal the network thread to continue reading
     */
    _DEBUG("Waking up reader thread");
    g_mutex_lock(h->reader_status.mutex);
    g_cond_signal(h->reader_status.cond);
    g_mutex_unlock(h->reader_status.mutex);

    h->pos += (relem*size);
    h->icy_metaleft -= (relem*size);

    _DEBUG("Returning %d elements", relem);

    _LEAVE relem;
}


/*
 * -----
 */

size_t neon_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile* file) {

    _ENTER;

    _ERROR("NOT IMPLEMENTED");

    _LEAVE 0;
}

/*
 * -----
 */

gint neon_vfs_getc_impl(VFSFile* file) {

    gchar c;

    _ENTER;

    if (1 != neon_vfs_fread_impl(&c, 1, 1, file)) {
        _ERROR("Could not getc()!");
        _LEAVE -1;
    }

    _LEAVE c;
}

/*
 * -----
 */

gint neon_vfs_ungetc_impl(gint c, VFSFile* stream) {

    _ENTER;

    _ERROR("NOT IMPLEMENTED");

    _LEAVE 0;
}

/*
 * -----
 */

void neon_vfs_rewind_impl(VFSFile* file) {

    _ENTER;

    (void)neon_vfs_fseek_impl(file, 0L, SEEK_SET);

    _LEAVE;
}

/*
 * -----
 */

glong neon_vfs_ftell_impl(VFSFile* file) {

    struct neon_handle* h = (struct neon_handle *)file->handle;

    _ENTER;

    _DEBUG("Current file position: %ld", h->pos);

    _LEAVE h->pos;
}

/*
 * -----
 */

gboolean neon_vfs_feof_impl(VFSFile* file) {

    struct neon_handle* h = (struct neon_handle*)file->handle;

    _ENTER;

    _LEAVE h->eof;
}

/*
 * -----
 */

gint neon_vfs_truncate_impl(VFSFile* file, glong size) {

    _ENTER;

    _ERROR("NOT IMPLEMENTED");

    _LEAVE 0;
}

/*
 * -----
 */

gint neon_vfs_fseek_impl(VFSFile* file, glong offset, gint whence) {

    struct neon_handle* h = (struct neon_handle*)file->handle;
    long newpos;
    long content_length;

    _ENTER;

    _DEBUG("Seek requested: offset %ld, whence %d", offset, whence);
    /*
     * Two things must be satisfied for us to be able to seek:
     * - the server must advertise a content-length
     * - the server must advertise accept-ranges: bytes
     */
    if ((-1 == h->content_length) || !h->can_ranges) {
        _DEBUG("Can not seek due to server restrictions");
        _LEAVE -1;
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
            newpos = content_length + offset;
            break;
        default:
            _ERROR("Invalid whence specified");
            _LEAVE -1;
    }

    _DEBUG("Position to seek to: %ld, current: %ld", newpos, h->pos);
    if (0 > newpos) {
        _ERROR("Can not seek before start of stream");
        _LEAVE -1;
    }

    if (newpos > content_length) {
        _ERROR("Can not seek beyond end of stream");
        _LEAVE -1;
    }

    if (newpos == h->pos) {
        _LEAVE 0;
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

    ne_request_destroy(h->request);
    ne_session_destroy(h->session);
    reset_rb(&h->rb);

    if (0 != open_handle(h, newpos)) {
        /*
         * Something went wrong while creating the new request.
         * There is not much we can do now, we'll set the request
         * to NULL, so that fread() will error out on the next
         * read request
         */
        _ERROR("Error while creating new request!");
        h->request = NULL;
        _LEAVE -1;
    }

    /*
     * Things seem to have worked. The next read request will start
     * the reader thread again.
     */

    _LEAVE 0;
}

/*
 * -----
 */

gchar *neon_vfs_metadata_impl(VFSFile* file, const gchar* field) {

    struct neon_handle* h = (struct neon_handle*)file->handle;

    _ENTER;

    _DEBUG("Field name: %s", field);

    if (0 == g_ascii_strncasecmp(field, "track-name", 10)) {
        _LEAVE g_strdup(h->icy_metadata.stream_title);
    }

    if (0 == g_ascii_strncasecmp(field, "stream-name", 11)) {
        _LEAVE g_strdup(h->icy_metadata.stream_name);
    }

    _LEAVE NULL;
}

/*
 * -----
 */

off_t neon_vfs_fsize_impl(VFSFile* file) {

    struct neon_handle* h = (struct neon_handle*)file->handle;

    _ENTER;

    if (-1 == h->content_length) {
        _DEBUG("Unknown content length");
        _LEAVE 0;
    }

    _LEAVE (h->content_start + h->content_length);
}
