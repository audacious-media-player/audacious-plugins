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
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/ringbuf.h>
#include <libaudcore/runtime.h>

#include <ne_auth.h>
#include <ne_session.h>
#include <ne_socket.h>
#include <ne_redirect.h>
#include <ne_request.h>
#include <ne_uri.h>
#include <ne_utils.h>

#include "cert_verification.h"

#define NEON_NETBLKSIZE     (4096)
#define NEON_ICY_BUFSIZE    (4096)
#define NEON_RETRY_COUNT 6

enum FillBufferResult {
    FILL_BUFFER_SUCCESS,
    FILL_BUFFER_ERROR,
    FILL_BUFFER_EOF
};

enum neon_reader_t {
    NEON_READER_INIT = 0,
    NEON_READER_RUN = 1,
    NEON_READER_ERROR,
    NEON_READER_EOF,
    NEON_READER_TERM
};

struct reader_status
{
    bool reading = false;
    neon_reader_t status = NEON_READER_INIT;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    reader_status ()
    {
        pthread_mutex_init (& mutex, nullptr);
        pthread_cond_init (& cond, nullptr);
    }

    ~reader_status ()
    {
        pthread_mutex_destroy (& mutex);
        pthread_cond_destroy (& cond);
    }
};

struct icy_metadata
{
    String stream_name;
    String stream_title;
    String stream_url;
    String stream_contenttype;
    int stream_bitrate = 0;
};

static const char * const neon_schemes[] = {"http", "https"};

class NeonTransport : public TransportPlugin
{
public:
    static constexpr PluginInfo info = {N_("Neon HTTP/HTTPS Plugin"), PACKAGE};

    constexpr NeonTransport () : TransportPlugin (info, neon_schemes) {}

    bool init ();
    void cleanup ();

    VFSImpl * fopen (const char * path, const char * mode, String & error);
};

EXPORT NeonTransport aud_plugin_instance;

bool NeonTransport::init ()
{
    int ret = ne_sock_init ();

    if (ret != 0)
    {
        AUDERR ("Could not initialize neon library: %d\n", ret);
        return false;
    }

    return true;
}

void NeonTransport::cleanup ()
{
    ne_sock_exit ();
}

class NeonFile : public VFSImpl
{
public:
    NeonFile (const char * url);
    ~NeonFile ();

    int open_handle (int64_t startbyte, String * error = nullptr);

protected:
    int64_t fread (void * ptr, int64_t size, int64_t nmemb);
    int fseek (int64_t offset, VFSSeekType whence);

    int64_t ftell ();
    int64_t fsize ();
    bool feof ();

    int64_t fwrite (const void * ptr, int64_t size, int64_t nmemb);
    int ftruncate (int64_t length);
    int fflush ();

    String get_metadata (const char * field);

private:
    String m_url;               /* The URL, as passed to us */
    ne_uri m_purl = ne_uri ();  /* The URL, parsed into a structure */

    unsigned char m_redircount = 0;     /* Redirect count for the opened URL */
    int64_t m_pos = 0;                  /* Current position in the stream
                                           (number of last byte delivered to the player) */
    int64_t m_content_start = 0;        /* Start position in the stream */
    int64_t m_content_length = -1;      /* Total content length, counting from
                                           content_start, if known. -1 if unknown */
    bool m_can_ranges = false;          /* true if the webserver advertised accept-range: bytes */
    int64_t m_icy_metaint = 0;          /* Interval in which the server will
                                           send metadata announcements. 0 if no announcments */
    int64_t m_icy_metaleft = 0;         /* Bytes left until the next metadata block */
    int m_icy_len = 0;                  /* Bytes in current metadata block */

    bool m_eof = false;

    RingBuf<char> m_rb;           /* Ringbuffer for our data */
    Index<char> m_icy_buf;        /* Buffer for ICY metadata */
    icy_metadata m_icy_metadata;  /* Current ICY metadata */

    ne_session * m_session = nullptr;
    ne_request * m_request = nullptr;

    pthread_t m_reader;
    reader_status m_reader_status;

    void kill_reader ();
    int server_auth (const char * realm, int attempt, char * username, char * password);
    void handle_headers ();
    int open_request (int64_t startbyte, String * error);
    FillBufferResult fill_buffer ();
    void reader ();
    int64_t try_fread (void * ptr, int64_t size, int64_t nmemb, bool & data_read);

    static int server_auth_callback (void * data, const char * realm, int attempt,
     char * username, char * password)
        { return ((NeonFile *) data)->server_auth (realm, attempt, username, password); }

    static void * reader_thread (void * data)
        { ((NeonFile *) data)->reader (); return nullptr; }
};

NeonFile::NeonFile (const char * url) :
    m_url (url)
{
    int buffer_kb = aud_get_int (nullptr, "net_buffer_kb");
    m_rb.alloc (1024 * aud::clamp (buffer_kb, 16, 1024));
}

NeonFile::~NeonFile ()
{
    if (m_reader_status.reading)
        kill_reader ();

    if (m_request)
        ne_request_destroy (m_request);
    if (m_session)
        ne_session_destroy (m_session);

    ne_uri_free (& m_purl);
}

static bool neon_strcmp (const char * str, const char * cmp)
{
    return ! g_ascii_strncasecmp (str, cmp, strlen (cmp));
}

static void add_icy (struct icy_metadata * m, const char * name, const char * value)
{
    if (neon_strcmp (name, "StreamTitle"))
    {
        AUDDBG ("Found StreamTitle: %s\n", value);
        m->stream_title = String (str_to_utf8 (value, -1));
    }

    if (neon_strcmp (name, "StreamUrl"))
    {
        AUDDBG ("Found StreamUrl: %s\n", value);
        m->stream_url = String (str_to_utf8 (value, -1));
    }
}

static void parse_icy (struct icy_metadata * m, char * metadata, int len)
{
    enum TagReadState
    {
        STATE_READ_NAME,
        STATE_WAIT_VALUE,
        STATE_READ_VALUE,
        STATE_WAIT_NAME,
    };

    TagReadState state = STATE_READ_NAME;

    char * p = metadata;
    char * tstart = metadata;
    int pos = 1;

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

void NeonFile::kill_reader ()
{
    AUDDBG ("Signaling reader thread to terminate\n");
    pthread_mutex_lock (& m_reader_status.mutex);
    m_reader_status.reading = false;
    pthread_cond_broadcast (& m_reader_status.cond);
    pthread_mutex_unlock (& m_reader_status.mutex);

    AUDDBG ("Waiting for reader thread to die...\n");
    pthread_join (m_reader, nullptr);
    AUDDBG ("Reader thread has died\n");
}

int NeonFile::server_auth (const char * realm, int attempt, char * username, char * password)
{
    if (! m_purl.userinfo || ! m_purl.userinfo[0])
    {
        AUDERR ("Authentication required, but no credentials set\n");
        return 1;
    }

    char * * authtok = g_strsplit (m_purl.userinfo, ":", 2);

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

void NeonFile::handle_headers ()
{
    const char * name;
    const char * value;
    void * cursor = nullptr;

    AUDDBG ("Header responses:\n");

    while ((cursor = ne_response_header_iterate (m_request, cursor, & name, & value)))
    {
        AUDDBG ("HEADER: %s: %s\n", name, value);

        if (neon_strcmp (name, "accept-ranges"))
        {
            /* The server advertises range capability. we need "bytes" */
            if (strstr (value, "bytes"))
            {
                AUDDBG ("server can_ranges\n");
                m_can_ranges = true;
            }
        }
        else if (neon_strcmp (name, "content-length"))
        {
            /* The server sent us the content length. Parse and store. */
            char * endptr;
            int64_t len = strtoll (value, & endptr, 10);

            if (value[0] && ! endptr[0] && len >= 0)
            {
                /* Valid data. */
                AUDDBG ("Content length as advertised by server: %" PRId64 "\n", len);
                m_content_length = len;
            }
            else
                AUDERR ("Invalid content length header: %s\n", value);
        }
        else if (neon_strcmp (name, "content-type"))
        {
            /* The server sent us a content type. Save it for later */
            AUDDBG ("Content-Type: %s\n", value);
            m_icy_metadata.stream_contenttype = String (str_to_utf8 (value, -1));
        }
        else if (neon_strcmp (name, "icy-metaint"))
        {
            /* The server sent us a ICY metaint header. Parse and store. */
            char * endptr;
            int64_t len = strtoll (value, & endptr, 10);

            if (value[0] && ! endptr[0] && len > 0)
            {
                /* Valid data */
                AUDDBG ("ICY MetaInt as advertised by server: %" PRId64 "\n", len);
                m_icy_metaint = len;
                m_icy_metaleft = len;
            }
            else
                AUDERR ("Invalid ICY MetaInt header: %s\n", value);
        }
        else if (neon_strcmp (name, "icy-name"))
        {
            /* The server sent us a ICY name. Save it for later */
            AUDDBG ("ICY stream name: %s\n", value);
            m_icy_metadata.stream_name = String (value);
        }
        else if (neon_strcmp (name, "icy-br"))
        {
            /* The server sent us a bitrate. We might want to use it. */
            AUDDBG ("ICY bitrate: %d\n", atoi (value));
            m_icy_metadata.stream_bitrate = atoi (value);
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

int NeonFile::open_request (int64_t startbyte, String * error)
{
    int ret;
    const ne_status * status;
    ne_uri * rediruri;

    if (m_purl.query && * (m_purl.query))
    {
        StringBuf tmp = str_concat ({m_purl.path, "?", m_purl.query});
        m_request = ne_request_create (m_session, "GET", tmp);
    }
    else
        m_request = ne_request_create (m_session, "GET", m_purl.path);

    if (startbyte > 0)
        ne_add_request_header (m_request, "Range", str_printf ("bytes=%" PRIu64 "-", startbyte));

    ne_add_request_header (m_request, "Icy-MetaData", "1");

    /* Try to connect to the server. */
    AUDDBG ("<%p> Connecting...\n", this);
    ret = ne_begin_request (m_request);
    status = ne_get_status (m_request);
    AUDDBG ("<%p> Return: %d, Status: %d\n", this, ret, status->code);

    if (ret == NE_OK)
    {
        switch (status->code)
        {
        case 401:
            /* Authorization required. Reconnect to authenticate */
            AUDDBG ("Reconnecting due to 401\n");
            ne_end_request (m_request);
            ret = ne_begin_request (m_request);
            break;

        case 301:
        case 302:
        case 303:
        case 307:
            /* Redirect encountered. Reconnect. */
            ne_end_request (m_request);
            ret = NE_REDIRECT;
            break;

        case 407:
            /* Proxy auth required. Reconnect to authenticate */
            AUDDBG ("Reconnecting due to 407\n");
            ne_end_request (m_request);
            ret = ne_begin_request (m_request);
            break;
        }
    }

    switch (ret)
    {
    case NE_OK:
        if (status->code > 199 && status->code < 300)
        {
            /* URL opened OK */
            AUDDBG ("<%p> URL opened OK\n", this);
            m_content_start = startbyte;
            m_pos = startbyte;
            handle_headers ();
            return 0;
        }

        break;

    case NE_REDIRECT:
        /* We hit a redirect. Handle it. */
        AUDDBG ("<%p> Redirect encountered\n", this);
        m_redircount += 1;
        rediruri = (ne_uri *) ne_redirect_location (m_session);
        ne_request_destroy (m_request);
        m_request = nullptr;

        if (! rediruri)
        {
            if (error)
                * error = String (_("Error parsing redirect"));

            AUDERR ("<%p> Could not parse redirect response\n", this);
            return -1;
        }

        ne_uri_free (& m_purl);
        ne_uri_copy (& m_purl, rediruri);
        return 1;
    }

    /* Something went wrong. */
    const char * ne_error = ne_get_error (m_session);
    if (error)
        * error = String (ne_error ? ne_error : _("Unknown HTTP error"));

    AUDERR ("<%p> Could not open URL: %d (%d)\n", this, ret, status->code);

    if (ne_error)
        AUDERR ("<%p> neon error string: %s\n", this, ne_error);

    ne_request_destroy (m_request);
    m_request = nullptr;
    return -1;
}

int NeonFile::open_handle (int64_t startbyte, String * error)
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

    m_redircount = 0;

    AUDDBG ("<%p> Parsing URL\n", this);

    if (ne_uri_parse (m_url, & m_purl) != 0)
    {
        if (error)
            * error = String (_("Error parsing URL"));

        AUDERR ("<%p> Could not parse URL '%s'\n", this, (const char *) m_url);
        return -1;
    }

    while (m_redircount < 10)
    {
        if (! m_purl.port)
            m_purl.port = ne_uri_defaultport (m_purl.scheme);

        AUDDBG ("<%p> Creating session to %s://%s:%d\n", this,
         m_purl.scheme, m_purl.host, m_purl.port);
        m_session = ne_session_create (m_purl.scheme,
         m_purl.host, m_purl.port);
        ne_redirect_register (m_session);
        ne_add_server_auth (m_session, NE_AUTH_BASIC, server_auth_callback, this);
        ne_set_session_flag (m_session, NE_SESSFLAG_ICYPROTO, 1);
        ne_set_session_flag (m_session, NE_SESSFLAG_PERSIST, 0);
        ne_set_connect_timeout (m_session, 10);
        ne_set_read_timeout (m_session, 10);
        ne_set_useragent (m_session, "Audacious/" PACKAGE_VERSION);

        if (use_proxy)
        {
            AUDDBG ("<%p> Using proxy: %s:%d\n", this, (const char *) proxy_host, proxy_port);
            ne_session_proxy (m_session, proxy_host, proxy_port);

            if (use_proxy_auth)
            {
                AUDDBG ("<%p> Using proxy authentication\n", this);
                ne_add_proxy_auth (m_session, NE_AUTH_BASIC,
                 neon_proxy_auth_cb, (void *) this);
            }
        }

        if (! strcmp ("https", m_purl.scheme))
        {
            ne_ssl_trust_default_ca (m_session);
            ne_ssl_set_verify (m_session,
             neon_vfs_verify_environment_ssl_certs, m_session);
        }

        AUDDBG ("<%p> Creating request\n", this);
        ret = open_request (startbyte, error);

        if (! ret)
            return 0;

        if (ret == -1)
        {
            ne_session_destroy (m_session);
            m_session = nullptr;
            return -1;
        }

        AUDDBG ("<%p> Following redirect...\n", this);
        ne_session_destroy (m_session);
        m_session = nullptr;
    }

    /* If we get here, our redirect count exceeded */
    if (error)
        * error = String (_("Too many redirects"));

    AUDERR ("<%p> Redirect count exceeded for URL %s\n", this, (const char *) m_url);
    return 1;
}

FillBufferResult NeonFile::fill_buffer ()
{
    char buffer[NEON_NETBLKSIZE];
    int to_read;

    pthread_mutex_lock (& m_reader_status.mutex);
    to_read = aud::min (m_rb.space (), NEON_NETBLKSIZE);
    pthread_mutex_unlock (& m_reader_status.mutex);

    int bsize = ne_read_response_block (m_request, buffer, to_read);

    if (! bsize)
    {
        AUDDBG ("<%p> End of file encountered\n", this);
        return FILL_BUFFER_EOF;
    }

    if (bsize < 0)
    {
        AUDERR ("<%p> Error while reading from the network\n", this);
        ne_request_destroy (m_request);
        m_request = nullptr;
        return FILL_BUFFER_ERROR;
    }

    AUDDBG ("<%p> Read %d bytes of %d\n", this, bsize, to_read);

    pthread_mutex_lock (& m_reader_status.mutex);
    m_rb.copy_in (buffer, bsize);
    pthread_mutex_unlock (& m_reader_status.mutex);

    return FILL_BUFFER_SUCCESS;
}

void NeonFile::reader ()
{
    pthread_mutex_lock (& m_reader_status.mutex);

    while (m_reader_status.reading)
    {
        /* Hit the network only if we have more than NEON_NETBLKSIZE of free buffer */
        if (m_rb.space () > NEON_NETBLKSIZE)
        {
            pthread_mutex_unlock (& m_reader_status.mutex);

            FillBufferResult ret = fill_buffer ();

            pthread_mutex_lock (& m_reader_status.mutex);

            /* Wake up main thread if it is waiting. */
            pthread_cond_broadcast (& m_reader_status.cond);

            if (ret == FILL_BUFFER_ERROR)
            {
                AUDERR ("<%p> Error while reading from the network. "
                        "Terminating reader thread\n", this);
                m_reader_status.status = NEON_READER_ERROR;
                pthread_mutex_unlock (& m_reader_status.mutex);
                return;
            }
            else if (ret == FILL_BUFFER_EOF)
            {
                AUDDBG ("<%p> EOF encountered while reading from the network. "
                        "Terminating reader thread\n", this);
                m_reader_status.status = NEON_READER_EOF;
                pthread_mutex_unlock (& m_reader_status.mutex);
                return;
            }
        }
        else
        {
            /* Not enough free space in the buffer.
             * Sleep until the main thread wakes us up. */
            pthread_cond_wait (& m_reader_status.cond, & m_reader_status.mutex);
        }
    }

    AUDDBG ("<%p> Reader thread terminating gracefully\n", this);
    m_reader_status.status = NEON_READER_TERM;
    pthread_mutex_unlock (& m_reader_status.mutex);
}

VFSImpl * NeonTransport::fopen (const char * path, const char * mode, String & error)
{
    NeonFile * file = new NeonFile (path);

    AUDDBG ("<%p> Trying to open '%s' with neon\n", file, path);

    if (file->open_handle (0, & error) != 0)
    {
        AUDERR ("<%p> Could not open URL\n", file);
        delete file;
        return nullptr;
    }

    return file;
}

int64_t NeonFile::try_fread (void * ptr, int64_t size, int64_t nmemb, bool & data_read)
{
    if (! m_request)
    {
        AUDERR ("<%p> No request to read from, seek gone wrong?\n", this);
        return 0;
    }

    if (! size || ! nmemb || m_eof)
        return 0;

    /* If the buffer is empty, wait for the reader thread to fill it. */
    pthread_mutex_lock (& m_reader_status.mutex);

    for (int retries = 0; retries < NEON_RETRY_COUNT; retries ++)
    {
        if (m_rb.len () / size > 0 || ! m_reader_status.reading ||
         m_reader_status.status != NEON_READER_RUN)
            break;

        pthread_cond_broadcast (& m_reader_status.cond);
        pthread_cond_wait (& m_reader_status.cond, & m_reader_status.mutex);
    }

    pthread_mutex_unlock (& m_reader_status.mutex);

    if (! m_reader_status.reading)
    {
        if (m_reader_status.status != NEON_READER_EOF || m_content_length != -1)
        {
            /* There is no reader thread yet. Read the first bytes from
             * the network ourselves, and then fire up the reader thread
             * to keep the buffer filled up. */
            AUDDBG ("<%p> Doing initial buffer fill\n", this);
            FillBufferResult ret = fill_buffer ();

            if (ret == FILL_BUFFER_ERROR)
            {
                AUDERR ("<%p> Error while reading from the network\n", this);
                return 0;
            }

            /* We have some data in the buffer now.
             * Start the reader thread if we did not reach EOF during
             * the initial fill */
            pthread_mutex_lock (& m_reader_status.mutex);

            if (ret == FILL_BUFFER_SUCCESS)
            {
                m_reader_status.reading = true;
                AUDDBG ("<%p> Starting reader thread\n", this);
                pthread_create (& m_reader, nullptr, reader_thread, this);
                m_reader_status.status = NEON_READER_RUN;
            }
            else if (ret == FILL_BUFFER_EOF)
            {
                AUDDBG ("<%p> No reader thread needed (stream has reached EOF during fill)\n", this);
                m_reader_status.reading = false;
                m_reader_status.status = NEON_READER_EOF;
            }

            pthread_mutex_unlock (& m_reader_status.mutex);
        }
    }
    else
    {
        /* There already is a reader thread. Look if it is in good shape. */
        pthread_mutex_lock (& m_reader_status.mutex);

        switch (m_reader_status.status)
        {
        case NEON_READER_INIT:
        case NEON_READER_RUN:
            /* All is well, nothing to be done. */
            break;

        case NEON_READER_ERROR:
            /* A reader error happened. Log it, and treat it like an EOF
             * condition, by falling through to the NEON_READER_EOF codepath. */
            AUDDBG ("<%p> NEON_READER_ERROR happened. Terminating reader thread and marking EOF.\n", this);
            m_reader_status.status = NEON_READER_EOF;
            pthread_mutex_unlock (& m_reader_status.mutex);

            if (m_reader_status.reading)
                kill_reader ();

            pthread_mutex_lock (& m_reader_status.mutex);

        case NEON_READER_EOF:
            /* If there still is data in the buffer, carry on.
             * If not, terminate the reader thread and return 0. */
            if (! m_rb.len ())
            {
                AUDDBG ("<%p> Reached end of stream\n", this);
                pthread_mutex_unlock (& m_reader_status.mutex);

                if (m_reader_status.reading)
                    kill_reader ();

                m_eof = true;
                return 0;
            }

            break;

        case NEON_READER_TERM:
            /* The reader thread terminated gracefully, most likely on our own request.
             * We should not get here. */
            g_warn_if_reached ();
            pthread_mutex_unlock (& m_reader_status.mutex);
            return 0;
        }

        pthread_mutex_unlock (& m_reader_status.mutex);
    }

    /* Deliver data from the buffer */
    pthread_mutex_lock (& m_reader_status.mutex);

    if (m_rb.len ())
        data_read = true;
    else
    {
        /* The buffer is still empty, we can deliver no data! */
        AUDERR ("<%p> Buffer still underrun, fatal.\n", this);
        pthread_mutex_unlock (& m_reader_status.mutex);
        return 0;
    }

    int64_t belem = m_rb.len () / size;

    if (m_icy_metaint)
    {
        if (! m_icy_metaleft)
        {
            if (! m_icy_len)
            {
                /* The next data in the buffer is a ICY metadata announcement.
                 * Get the length byte */
                m_icy_len = 16 * (unsigned char) m_rb.head ();
                m_rb.pop ();

                AUDDBG ("<%p> Expecting %d bytes of ICY metadata\n", this, m_icy_len);
            }

            if (m_icy_buf.len () < m_icy_len)
                m_rb.move_out (m_icy_buf, -1, aud::min (m_icy_len - m_icy_buf.len (), m_rb.len ()));

            if (m_icy_buf.len () >= m_icy_len)
            {
                /* Grab the metadata from the buffer and send it to the parser */
                parse_icy (& m_icy_metadata, m_icy_buf.begin (), m_icy_buf.len ());

                /* Reset countdown to next announcement */
                m_icy_buf.clear ();
                m_icy_len = 0;
                m_icy_metaleft = m_icy_metaint;
            }
        }

        /* The maximum number of bytes we can deliver is determined
         * by the number of bytes left until the next metadata announcement */
        belem = aud::min ((int64_t) m_rb.len (), m_icy_metaleft) / size;
    }

    nmemb = aud::min (belem, nmemb);
    m_rb.move_out ((char *) ptr, nmemb * size);

    /* Signal the network thread to continue reading */
    if (m_reader_status.status == NEON_READER_EOF)
    {
        if (! m_rb.len ())
        {
            AUDDBG ("<%p> stream EOF reached and buffer empty\n", this);
            m_eof = true;
        }
    }
    else
        pthread_cond_broadcast (& m_reader_status.cond);

    pthread_mutex_unlock (& m_reader_status.mutex);

    m_pos += nmemb * size;
    m_icy_metaleft -= nmemb * size;

    return nmemb;
}

/* try_fread will do only a partial read if the buffer underruns, so we
 * must call it repeatedly until we have read the full request. */
int64_t NeonFile::fread (void * buffer, int64_t size, int64_t count)
{
    int64_t total = 0;

    AUDDBG ("<%p> fread %d x %d\n", this, (int) size, (int) count);

    while (count > 0)
    {
        bool data_read = false;
        int64_t part = try_fread (buffer, size, count, data_read);
        if (! data_read)
            break;

        buffer = (char *) buffer + size * part;
        total += part;
        count -= part;
    }

    AUDDBG ("<%p> fread = %d\n", this, (int) total);

    return total;
}

int64_t NeonFile::fwrite (const void * ptr, int64_t size, int64_t nmemb)
{
    AUDERR ("<%p> NOT IMPLEMENTED\n", this);

    return 0;
}

int64_t NeonFile::ftell ()
{
    AUDDBG ("<%p> Current file position: %" PRId64 "\n", this, m_pos);

    return m_pos;
}

bool NeonFile::feof ()
{
    AUDDBG ("<%p> EOF status: %s\n", this, m_eof ? "true" : "false");

    return m_eof;
}

int NeonFile::ftruncate (int64_t size)
{
    AUDERR ("<%p> NOT IMPLEMENTED\n", this);

    return 0;
}

int NeonFile::fflush ()
{
    return 0; /* no-op */
}

int NeonFile::fseek (int64_t offset, VFSSeekType whence)
{
    AUDDBG ("<%p> Seek requested: offset %" PRId64 ", whence %d\n", this, offset, whence);

    /* To seek to a non-zero offset, two things must be satisfied:
     * - the server must advertise a content-length
     * - the server must advertise accept-ranges: bytes */
    if ((whence != VFS_SEEK_SET || offset) && (m_content_length < 0 || ! m_can_ranges))
    {
        AUDDBG ("<%p> Can not seek due to server restrictions\n", this);
        return -1;
    }

    int64_t content_length = m_content_length + m_content_start;
    int64_t newpos;

    switch (whence)
    {
    case VFS_SEEK_SET:
        newpos = offset;
        break;

    case VFS_SEEK_CUR:
        newpos = m_pos + offset;
        break;

    case VFS_SEEK_END:
        if (offset == 0)
        {
            m_pos = content_length;
            m_eof = true;
            return 0;
        }

        newpos = content_length + offset;
        break;

    default:
        AUDERR ("<%p> Invalid whence specified\n", this);
        return -1;
    }

    AUDDBG ("<%p> Position to seek to: %" PRId64 ", current: %" PRId64 "\n", this, newpos, m_pos);

    if (newpos < 0)
    {
        AUDERR ("<%p> Can not seek before start of stream\n", this);
        return -1;
    }

    if (newpos && newpos >= content_length)
    {
        AUDERR ("<%p> Can not seek beyond end of stream (%" PRId64 " >= %"
         PRId64 "\n", this, newpos, content_length);
        return -1;
    }

    if (newpos == m_pos)
        return 0;

    /* To seek to the new position we have to
     * - stop the current reader thread, if there is one
     * - destroy the current request
     * - dump all data currently in the ringbuffer
     * - create a new request starting at newpos */
    if (m_reader_status.reading)
        kill_reader ();

    if (m_request)
    {
        ne_request_destroy (m_request);
        m_request = nullptr;
    }

    if (m_session)
    {
        ne_session_destroy (m_session);
        m_session = nullptr;
    }

    m_rb.discard ();
    m_icy_buf.clear ();
    m_icy_len = 0;

    if (open_handle (newpos) != 0)
    {
        AUDERR ("<%p> Error while creating new request!\n", this);
        return -1;
    }

    /* Things seem to have worked. The next read request will start
     * the reader thread again. */
    m_eof = false;

    return 0;
}

String NeonFile::get_metadata (const char * field)
{
    AUDDBG ("<%p> Field name: %s\n", this, field);

    if (! strcmp (field, "track-name") && m_icy_metadata.stream_title)
        return m_icy_metadata.stream_title;

    if (! strcmp (field, "stream-name") && m_icy_metadata.stream_name)
        return m_icy_metadata.stream_name;

    if (! strcmp (field, "content-type") && m_icy_metadata.stream_contenttype)
        return m_icy_metadata.stream_contenttype;

    if (! strcmp (field, "content-bitrate"))
        return String (int_to_str (m_icy_metadata.stream_bitrate * 1000));

    return String ();
}

int64_t NeonFile::fsize ()
{
    if (m_content_length < 0)
    {
        AUDDBG ("<%p> Unknown content length\n", this);
        return -1;
    }

    return m_content_start + m_content_length;
}
