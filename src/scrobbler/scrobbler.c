#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "fmt.h"
#include "plugin.h"
#include "scrobbler.h"
#include "settings.h"
#include <glib.h>

#include <audacious/drct.h>
#include <audacious/debug.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/md5.h>

#define SCROBBLER_CLI_ID "aud"
#define SCROBBLER_HS_WAIT 1800
#define SCROBBLER_SB_WAIT 10
#define SCROBBLER_VERSION "1.2"
#define SCROBBLER_IMPLEMENTATION "0.2"      /* This is the implementation, not the player version. */
#define SCROBBLER_SB_MAXLEN 1024
#define CACHE_SIZE 1024

/* Scrobblerbackend for xmms plugin, first draft */

static int  sc_hs_status,
        sc_hs_timeout,
        sc_hs_errors,
        sc_sb_errors,
        sc_bad_users,
        sc_submit_interval,
        sc_submit_timeout,
        sc_srv_res_size,
        sc_giveup,
        sc_major_error_present;

static char     *sc_submit_url, /* queue */
        *sc_np_url, /* np */
        *sc_hs_url, /* handshake url */
        *sc_session_id,
        *sc_username = NULL,
        *sc_password = NULL,
        *sc_challenge_hash,
        sc_response_hash[65535],
        *sc_srv_res,
        sc_curl_errbuf[CURL_ERROR_SIZE],
        *sc_major_error;

static void dump_queue();

/**** Queue stuff ****/

#define I_ARTIST(i) i->artist
#define I_TITLE(i) i->title
#define I_TIME(i) i->utctime
#define I_LEN(i) i->len
#define I_ALBUM(i) i->album

typedef struct {
    char *artist, *title, *album;
    int utctime, track, len;
    int timeplayed;
    int numtries;
    void *next;
} item_t;

static item_t *q_queue = NULL;
static item_t *q_queue_last = NULL;
static int q_nitems;
static item_t *np_item = NULL;

gchar *
xmms_urldecode_plain(const gchar * encoded_path)
{
    const gchar *cur, *ext;
    gchar *path, *tmp;
    gint realchar;

    if (!encoded_path)
        return NULL;

    cur = encoded_path;
    if (*cur == '/')
        while (cur[1] == '/')
            cur++;

    tmp = g_malloc0(strlen(cur) + 1);

    while ((ext = strchr(cur, '%')) != NULL) {
        strncat(tmp, cur, ext - cur);
        ext++;
        cur = ext + 2;
        if (!sscanf(ext, "%2x", &realchar)) {
            /*
             * Assume it is a literal '%'.  Several file
             * managers send unencoded file: urls on on
             * drag and drop.
             */
            realchar = '%';
            cur -= 2;
        }
        tmp[strlen(tmp)] = realchar;
    }

    path = g_strconcat(tmp, cur, NULL);
    g_free(tmp);
    return path;
}

static void q_item_free(item_t *item)
{
    if (item == NULL)
        return;
    curl_free(item->artist);
    curl_free(item->title);
    curl_free(item->album);
    free(item);
}

static item_t *q_additem(item_t *newitem)
{
    AUDDBG("Adding %s - %s to the queue\n", newitem->artist, newitem->title);

    q_nitems++;
    newitem->next = NULL;
    if (q_queue_last == NULL)
    {
        q_queue = q_queue_last = newitem;
    }
    else
    {
        q_queue_last->next = newitem;
        q_queue_last = newitem;
    }
    return newitem;
}

static item_t *create_item(Tuple *tuple, int len)
{
    item_t *item;
    gchar *album, *artist, *title;

    item = malloc(sizeof(item_t));

    artist = tuple_get_str(tuple, FIELD_ARTIST, NULL);
    item->artist = fmt_escape(artist);
    str_unref(artist);

    title = tuple_get_str(tuple, FIELD_TITLE, NULL);
    item->title = fmt_escape(title);
    str_unref(title);

    if (item->artist == NULL || item->title == NULL)
    {
        free (item);
        return NULL;
    }

    item->len = len;
    item->track = tuple_get_int(tuple, FIELD_TRACK_NUMBER, NULL);
    item->timeplayed = 0;
    item->utctime = time(NULL);

    album = tuple_get_str(tuple, FIELD_ALBUM, NULL);
    if (album)
    {
        item->album = fmt_escape(album);
        str_unref(album);
    }
    else
        item->album = fmt_escape("");

    item->next = NULL;

    return item;
}

static item_t *q_put(Tuple *tuple, int t, int len)
{
    item_t *item;

    if ((item = create_item (tuple, len)) == NULL)
        return NULL;

    item->timeplayed = len;
    item->utctime = t;

    return q_additem(item);
}

static item_t *set_np(Tuple *tuple, int len)
{
    if (np_item != NULL)
        q_item_free (np_item);

    if ((np_item = create_item (tuple, len)) != NULL)
        AUDDBG ("Tracking now-playing track: %s - %s\n", np_item->artist,
         np_item->title);

    return np_item;
}

#if 0
static item_t *q_peek(void)
{
    if (q_nitems == 0)
        return NULL;
    return q_queue;
}
#endif

static item_t *q_peekall(int rewind)
{
    static item_t *citem = NULL;
    item_t *temp_item;

    if (rewind) {
        citem = q_queue;
        return NULL;
    }

    temp_item = citem;

    if(citem != NULL)
        citem = citem->next;

    return temp_item;
}

static int q_get(void)
{
    item_t *item;

    if (q_nitems == 0)
        return 0;

    item = q_queue;

    if(item == NULL)
        return 0;

    q_nitems--;
    q_queue = q_queue->next;

    AUDDBG("Removing %s - %s from queue\n", item->artist, item->title);

    q_item_free(item);

    if (q_nitems == 0)
    {
        q_queue_last = NULL;
        return 0;
    }

    return -1;
}

static void q_free(void)
{
    while (q_get());
}


/* isn't there better way for that? --desowin */
gboolean sc_timeout(gpointer data)
{
    if (np_item)
    {
        if (aud_drct_get_playing() && !aud_drct_get_paused())
            np_item->timeplayed+=1;

        /*
         * Check our now-playing track to see if it should go into the queue
         */
        if (((np_item->timeplayed >= (np_item->len / 2)) ||
            (np_item->timeplayed >= 240)))
        {
            AUDDBG("submitting!!!\n");

            q_additem(np_item);
            np_item = NULL;
            dump_queue();
        }
    }

    return TRUE;
}

/* Error functions */

static void sc_throw_error(char *errortxt)
{
    sc_major_error_present = 1;
    if(sc_major_error == NULL)
        sc_major_error = strdup(errortxt);

    return;
}

int sc_catch_error(void)
{
    return sc_major_error_present;
}

char *sc_fetch_error(void)
{
    return sc_major_error;
}

void sc_clear_error(void)
{
    sc_major_error_present = 0;
    if(sc_major_error != NULL)
        free(sc_major_error);
    sc_major_error = NULL;

    return;
}

static size_t sc_store_res(void *ptr, size_t size,
        size_t nmemb,
        void *stream __attribute__((unused)))
{
    int len = size * nmemb;

    sc_srv_res = realloc(sc_srv_res, sc_srv_res_size + len + 1);
    memcpy(sc_srv_res + sc_srv_res_size,
            ptr, len);
    sc_srv_res_size += len;
    return len;
}

static void sc_free_res(void)
{
    if(sc_srv_res != NULL)
        free(sc_srv_res);
    sc_srv_res = NULL;
    sc_srv_res_size = 0;
}

static int sc_parse_hs_res(void)
{
    char *interval;

    if (!sc_srv_res_size) {
        AUDDBG("No reply from server\n");
        return -1;
    }
    if (sc_srv_res == NULL) {
        AUDDBG("Reply is NULL, size=%d\n", sc_srv_res_size);
        return -1;
    }
    *(sc_srv_res + sc_srv_res_size) = 0;

    AUDDBG("reply is: %s\n", sc_srv_res);
    if (!strncmp(sc_srv_res, "OK\n", 3)) {
        gchar *scratch = g_strdup(sc_srv_res);
        gchar **split = g_strsplit(scratch, "\n", 5);

        g_free(scratch);

        sc_session_id = g_strdup(split[1]);
        sc_np_url = g_strdup(split[2]);
        sc_submit_url = g_strdup(split[3]);

        g_strfreev(split);
        return 0;
    }
    if (!strncmp(sc_srv_res, "FAILED ", 7)) {
        interval = strstr(sc_srv_res, "INTERVAL");

        /* Throwing a major error, just in case */
        /* sc_throw_error(fmt_vastr("%s", sc_srv_res));
           sc_hs_errors++; */
        AUDDBG("error: %s\n", sc_srv_res);

        return -1;
    }

    if (!strncmp(sc_srv_res, "UPDATE ", 7)) {
        interval = strstr(sc_srv_res, "INTERVAL");
        if(!interval)
        {
            AUDDBG("missing INTERVAL\n");
        }
        else
        {
            *(interval - 1) = 0;
            sc_submit_interval = strtol(interval + 8, NULL, 10);
        }

        sc_submit_url = strchr(strchr(sc_srv_res, '\n') + 1, '\n') + 1;
        *(sc_submit_url - 1) = 0;
        sc_submit_url = strdup(sc_submit_url);
        sc_challenge_hash = strchr(sc_srv_res, '\n') + 1;
        *(sc_challenge_hash - 1) = 0;
        sc_challenge_hash = strdup(sc_challenge_hash);

        /* Throwing major error. Need to alert client to update. */
        sc_throw_error(fmt_vastr("Please update Audacious.\n"
            "Update available at: http://audacious-media-player.org"));
        AUDDBG("update client: %s\n", sc_srv_res + 7);

        /*
         * Russ isn't clear on whether we can submit with a not-updated
         * client.  Neither is RJ.  I use what we did before.
         */
        sc_giveup = -1;
        return -1;
    }
    if (!strncmp(sc_srv_res, "UPTODATE\n", 9)) {
        sc_bad_users = 0;

        interval = strstr(sc_srv_res, "INTERVAL");
        if (!interval) {
            AUDDBG("missing INTERVAL\n");
            /*
             * This is probably a bad thing, but Russ seems to
             * think its OK to assume that an UPTODATE response
             * may not have an INTERVAL...  We return -1 anyway.
             */
            return -1;
        }
        else
        {
            *(interval - 1) = 0;
            sc_submit_interval = strtol(interval + 8, NULL, 10);
        }

        sc_submit_url = strchr(strchr(sc_srv_res, '\n') + 1, '\n') + 1;
        *(sc_submit_url - 1) = 0;
        sc_submit_url = strdup(sc_submit_url);
        sc_challenge_hash = strchr(sc_srv_res, '\n') + 1;
        *(sc_challenge_hash - 1) = 0;
        sc_challenge_hash = strdup(sc_challenge_hash);

        return 0;
    }
    if(!strncmp(sc_srv_res, "BADAUTH", 7)) {
        /* Throwing major error. */
        sc_throw_error("Incorrect username/password.\n"
                "Please fix in configuration.");
        AUDDBG("incorrect username/password\n");

        interval = strstr(sc_srv_res, "INTERVAL");
        if(!interval)
        {
            AUDDBG("missing INTERVAL\n");
        }
        else
        {
            *(interval - 1) = 0;
            sc_submit_interval = strtol(interval + 8, NULL, 10);
        }

        return -1;
    }

    AUDDBG("unknown server-reply '%s'\n", sc_srv_res);
    return -1;
}

static unsigned char *md5_string(char *pass, int len)
{
    aud_md5state_t md5state;
    static unsigned char md5pword[16];

    aud_md5_init(&md5state);
    aud_md5_append(&md5state, (unsigned const char *)pass, len);
    aud_md5_finish(&md5state, md5pword);

    return md5pword;
}

static void hexify(char *pass, int len)
{
    char *bp = sc_response_hash;
    char hexchars[] = "0123456789abcdef";
    int i;

    memset(sc_response_hash, 0, sizeof(sc_response_hash));

    for(i = 0; i < len; i++) {
        *(bp++) = hexchars[(pass[i] >> 4) & 0x0f];
        *(bp++) = hexchars[pass[i] & 0x0f];
    }
    *bp = 0;

    return;
}

static int sc_parse_sb_res(void);
static GStaticMutex submit_mutex = G_STATIC_MUTEX_INIT;

gpointer sc_curl_perform_thread(gpointer data)
{
    CURL *curl = (CURL *) data;

    g_static_mutex_lock(&submit_mutex);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (sc_parse_sb_res()) {
        sc_sb_errors++;
        sc_free_res();
        AUDDBG("Retrying in %d secs, %d elements in queue\n",
                    sc_submit_interval, q_nitems);

        g_static_mutex_unlock(&submit_mutex);

        g_thread_exit(NULL);
        return NULL;
    }

    g_static_mutex_unlock(&submit_mutex);

    sc_free_res();
    g_thread_exit(NULL);
    return NULL;
}

void sc_curl_perform(CURL *curl)
{
    GError **error = NULL;

    g_thread_create(sc_curl_perform_thread, curl, FALSE, error);
}

static int sc_handshake(void)
{
    int status;
    char buf[65535];
    CURL *curl;
    time_t ts = time(NULL);
    char *auth_tmp;
    char *auth;

    auth_tmp = g_strdup_printf("%s%"PRId64, sc_password, (gint64) ts);
    auth = (char *)md5_string(auth_tmp, strlen(auth_tmp));
    g_free(auth_tmp);
    hexify(auth, 16);
    auth_tmp = g_strdup(sc_response_hash);

    g_snprintf(buf, sizeof(buf), "%s/?hs=true&p=%s&c=%s&v=%s&u=%s&t=%"PRId64"&a=%s",
            sc_hs_url, SCROBBLER_VERSION,
            SCROBBLER_CLI_ID, SCROBBLER_IMPLEMENTATION, sc_username, (gint64) ts,
            auth_tmp);
    g_free(auth_tmp);

    curl = curl_easy_init();
    setup_proxy(curl);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_URL, buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            sc_store_res);
    memset(sc_curl_errbuf, 0, sizeof(sc_curl_errbuf));
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, sc_curl_errbuf);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
    status = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    sc_hs_timeout = time(NULL) + SCROBBLER_HS_WAIT;

    if (status) {
        AUDDBG("curl error: %s\n", sc_curl_errbuf);
        sc_hs_errors++;
        sc_free_res();
        return -1;
    }

    if (sc_parse_hs_res()) {
        sc_hs_errors++;
        sc_free_res();
        return -1;
    }

    if (sc_challenge_hash != NULL) {
        aud_md5state_t md5state;
        unsigned char md5pword[16];

        aud_md5_init(&md5state);
        /*AUDDBG("Pass Hash: %s", sc_password));*/
        aud_md5_append(&md5state, (unsigned const char *)sc_password,
                strlen(sc_password));
        /*AUDDBG("Challenge Hash: %s", sc_challenge_hash));*/
        aud_md5_append(&md5state, (unsigned const char *)sc_challenge_hash,
                strlen(sc_challenge_hash));
        aud_md5_finish(&md5state, md5pword);
        hexify((char*)md5pword, sizeof(md5pword));
        /*AUDDBG("Response Hash: %s", sc_response_hash));*/
    }

    sc_hs_errors = 0;
    sc_hs_status = 1;

    sc_free_res();

    AUDDBG("submiturl: %s - interval: %d\n",
                sc_submit_url, sc_submit_interval);

    return 0;
}

static int sc_parse_sb_res(void)
{
    char *ch, *ch2;

    if (!sc_srv_res_size) {
        AUDDBG("No response from server\n");
        return -1;
    }
    if (sc_srv_res == NULL) {
        AUDDBG("Reply is NULL, size=%d\n", sc_srv_res_size);
        return -1;
    }
    *(sc_srv_res + sc_srv_res_size) = 0;

    AUDDBG("message: %s\n", sc_srv_res);
    if (!strncmp(sc_srv_res, "OK", 2)) {
        if ((ch = strstr(sc_srv_res, "INTERVAL"))) {
            sc_submit_interval = strtol(ch + 8, NULL, 10);
            AUDDBG("got new interval: %d\n",
                        sc_submit_interval);
        }

        AUDDBG("submission ok: %s\n", sc_srv_res);

        return 0;
    }

    if (!strncmp(sc_srv_res, "BADAUTH", 7)) {
        if ((ch = strstr(sc_srv_res, "INTERVAL"))) {
            sc_submit_interval = strtol(ch + 8, NULL, 10);
            AUDDBG("got new interval: %d\n",
                        sc_submit_interval);
        }

        AUDDBG("incorrect username/password\n");

        sc_giveup = 0;

        /*
         * We obviously aren't authenticated.  The server might have
         * lost our handshake status though, so let's try
         * re-handshaking...  This might not be proper.
         * (we don't give up)
         */
        sc_hs_status = 0;

        if(sc_challenge_hash != NULL)
            free(sc_challenge_hash);
        if(sc_submit_url != NULL)
            free(sc_submit_url);

        sc_challenge_hash = sc_submit_url = NULL;
        sc_bad_users++;

        if(sc_bad_users > 2)
        {
            AUDDBG("3 BADAUTH returns on submission. Halting "
                "submissions until login fixed.\n");
            sc_throw_error("Incorrect username/password.\n"
                "Please fix in configuration.");
        }

        return -1;
    }

    if(!strncmp(sc_srv_res, "BADSESSION", 10)) {
        AUDDBG("Invalid session, re-handshaking\n");

        sc_free_res();
        sc_handshake();

        return -1;
    }

    if (!strncmp(sc_srv_res, "FAILED", 6))  {
        if ((ch = strstr(sc_srv_res, "INTERVAL"))) {
            sc_submit_interval = strtol(ch + 8, NULL, 10);
            AUDDBG("got new interval: %d\n",
                        sc_submit_interval);
        }

        /* This could be important. (Such as FAILED - Get new plugin) */
        /*sc_throw_error(fmt_vastr("%s", sc_srv_res));*/

        AUDDBG("%s\n", sc_srv_res);

        return -1;
    }

    if (!strncmp(sc_srv_res, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">", 50)) {
        ch = strstr(sc_srv_res, "<TITLE>");
        ch2 = strstr(sc_srv_res, "</TITLE>");
        if (ch != NULL && ch2 != NULL) {
            ch += strlen("<TITLE>");
            *ch2 = '\0';

            AUDDBG("HTTP Error (%d): '%s'\n",
                     atoi(ch), ch + 4);
//          *ch2 = '<'; // needed? --yaz
        }

        return -1;
    }

    AUDDBG("unknown server-reply %s\n", sc_srv_res);

    return -1;
}

static gchar *sc_itemtag(char c, int n, char *str)
{
    static char buf[SCROBBLER_SB_MAXLEN];
    g_snprintf(buf, SCROBBLER_SB_MAXLEN, "&%c[%d]=%s", c, n, str);
    return buf;
}

#define cfa(f, l, n, v) \
curl_formadd(f, l, CURLFORM_COPYNAME, n, \
        CURLFORM_PTRCONTENTS, v, CURLFORM_END)

static int sc_generateentry(GString *submission)
{
    int i;
    item_t *item;

    i = 0;

    q_peekall(1);

    while ((item = q_peekall(0)) && i < 10)
    {
        /*
         * We assume now that all tracks in the queue should be submitted.
         * The check occurs in the sc_timeout() function now.
         */

        if (!item) {
            AUDDBG("item = NULL :(\n");
            return i;
        }

                g_string_append(submission,sc_itemtag('a',i,I_ARTIST(item)));
                g_string_append(submission,sc_itemtag('t',i,I_TITLE(item)));
                gchar *tmp = g_strdup_printf("%d",I_LEN(item));
                g_string_append(submission,sc_itemtag('l',i,tmp));
                g_free(tmp);
                tmp = g_strdup_printf("%d",I_TIME(item));
                g_string_append(submission,sc_itemtag('i',i,tmp));
                g_free(tmp);
                g_string_append(submission,sc_itemtag('m',i,""));
                g_string_append(submission,sc_itemtag('b',i,I_ALBUM(item)));
                g_string_append(submission,sc_itemtag('o',i,"P"));
                tmp = g_strdup_printf("%d",item->track);
                g_string_append(submission,sc_itemtag('n',i,tmp));
                g_free(tmp);
                g_string_append(submission,sc_itemtag('r',i,""));

        AUDDBG("a[%d]=%s t[%d]=%s l[%d]=%d i[%d]=%d b[%d]=%s\n",
                i, I_ARTIST(item),
                i, I_TITLE(item),
                i, I_LEN(item),
                i, I_TIME(item),
                i, I_ALBUM(item));
        i++;
    }

    return i;
}

static int sc_submit_np(Tuple *tuple)
{
    CURL *curl;
    /* struct HttpPost *post = NULL , *last = NULL; */
    static gchar entry[16384];

    curl = curl_easy_init();
    setup_proxy(curl);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_URL, sc_np_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            sc_store_res);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    /*cfa(&post, &last, "debug", "failed");*/

    char *artist = tuple_get_str(tuple, FIELD_ARTIST, NULL);
    char *field_artist = fmt_escape(artist);
    str_unref(artist);

    char *title = tuple_get_str(tuple, FIELD_TITLE, NULL);
    char *field_title = fmt_escape(title);
    str_unref(title);

    char *album = tuple_get_str(tuple, FIELD_ALBUM, NULL);
    char *field_album = album ? fmt_escape(album) : fmt_escape("");
    str_unref(album);

    snprintf(entry, 16384, "s=%s&a=%s&t=%s&b=%s&l=%d&n=%d&m=", sc_session_id,
        field_artist,
        field_title,
        field_album,
        tuple_get_int(tuple, FIELD_LENGTH, NULL) / 1000,
        tuple_get_int(tuple, FIELD_TRACK_NUMBER, NULL));
        curl_free(field_artist);
        curl_free(field_title);
        curl_free(field_album);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *) entry);
    memset(sc_curl_errbuf, 0, sizeof(sc_curl_errbuf));
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, sc_curl_errbuf);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, SCROBBLER_SB_WAIT);
    sc_curl_perform(curl);

    return 0;
}

static int sc_submitentry(gchar *entry)
{
    CURL *curl;
    /* struct HttpPost *post = NULL , *last = NULL; */
    static gchar sub[16384];

    curl = curl_easy_init();
    setup_proxy(curl);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_URL, sc_submit_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            sc_store_res);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    /*cfa(&post, &last, "debug", "failed");*/

    /*AUDDBG("Response Hash: %s", sc_response_hash));*/
    snprintf(sub, 16384, "s=%s%s", sc_session_id, entry);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *) sub);
    memset(sc_curl_errbuf, 0, sizeof(sc_curl_errbuf));
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, sc_curl_errbuf);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, SCROBBLER_SB_WAIT);
    sc_curl_perform(curl);

    return 0;
}

static void sc_handlequeue(GMutex *mutex)
{
    GString *submitentry;
    int nsubmit;
    int wait;
    int i;

    AUDDBG("handle queue\n");

    if(sc_submit_timeout < time(NULL) && sc_bad_users < 3)
    {
        submitentry = g_string_new("");

        AUDDBG("ok to handle queue!\n");

        g_mutex_lock(mutex);

        nsubmit = sc_generateentry(submitentry);

        g_mutex_unlock(mutex);

        if (nsubmit > 0)
        {
            AUDDBG("Number submitting: %d\n", nsubmit);
            AUDDBG("Submission: %s\n", submitentry->str);

            if(!sc_submitentry(submitentry->str))
            {
                g_mutex_lock(mutex);

                AUDDBG("Clearing out %d item(s) from the queue\n", nsubmit);
                for (i=0; i<nsubmit; i++)
                {
                    q_get();
                }

                /*
                 * This should make sure that the queue doesn't
                 * get submitted multiple times on a nasty
                 * segfault...
                 */
                dump_queue();

                g_mutex_unlock(mutex);

                sc_sb_errors = 0;
            }
            if(sc_sb_errors)
            {
                /* Dump queue */
                g_mutex_lock(mutex);
                dump_queue();
                g_mutex_unlock(mutex);

                if(sc_sb_errors < 5)
                    /* Retry after 1 min */
                    wait = 60;
                else
                    wait = /* sc_submit_interval + */
                        ( ((sc_sb_errors - 5) < 7) ?
                        (60 << (sc_sb_errors-5)) :
                        7200 );

                sc_submit_timeout = time(NULL) + wait;

                AUDDBG("Error while submitting. "
                    "Retrying after %d seconds.\n", wait);
            }
        }

        g_string_free(submitentry, TRUE);
    }
}

static void read_cache(void)
{
    int i=0;
    item_t *item;

    gchar * path = g_strconcat (aud_get_path (AUD_PATH_USER_DIR), "/scrobblerqueue.txt", NULL);

    if (! g_file_test (path, G_FILE_TEST_EXISTS))
        return;

    AUDDBG ("Opening %s\n", path);

    gchar* cache;
    gchar** values;
    gchar** entry;
    g_file_get_contents (path, & cache, NULL, NULL);
    values = g_strsplit(cache, "\n", 0);
    g_free (path);

    int x;
    for (x=0; values[x] && strlen(values[x]); x++) {
        entry = g_strsplit(values[x], "\t", 0);
        if (entry[0] && entry[1] && entry[2] && entry[3] && entry[4] && entry[6]) {
            char *artist, *title, *album;
            int t, len, track;

            artist = g_strdup(entry[0]);
            album = g_strdup(entry[1]);
            title = g_strdup(entry[2]);
            track = atoi(entry[3]);
            len = atoi(entry[4]);
            t = atoi(entry[6]);

            /*
             * All entries in the queue should have "L" as the sixth field now, but
             * we'll continue to check the value anyway, for backwards-compatibility reasons.
             */
            if (!strncmp(entry[5], "L", 1)) {
                Tuple *tuple = tuple_new();
                gchar* string_value;
                string_value = xmms_urldecode_plain(artist);
                tuple_copy_str(tuple, FIELD_ARTIST, NULL, string_value);
                g_free(string_value);
                string_value = xmms_urldecode_plain(title);
                tuple_copy_str(tuple, FIELD_TITLE, NULL, string_value);
                g_free(string_value);
                string_value = xmms_urldecode_plain(album);
                tuple_copy_str(tuple, FIELD_ALBUM, NULL, string_value);
                g_free(string_value);
                tuple_set_int(tuple, FIELD_TRACK_NUMBER, NULL, track);
                item = q_put(tuple, t, len);

                tuple_unref(tuple);

                if (item != NULL)
                    AUDDBG ("a[%d]=%s t[%d]=%s l[%d]=%d i[%d]=%d b[%d]=%s\n", i,
                     I_ARTIST (item), i, I_TITLE (item), i, I_LEN (item), i,
                     I_TIME (item), i, I_ALBUM (item));
            }

            free(artist);
            free(title);
            free(album);
            i++;
        }
        g_strfreev(entry);
    }
    g_strfreev(values);
    g_free(cache);
    AUDDBG("Done loading cache.\n");
}

static void dump_queue(void)
{
    FILE *fd;
    item_t *item;

    /*AUDDBG("Entering dump_queue();");*/

    gchar * home;
    if (!(home = getenv("HOME")))
    {
        AUDDBG("No HOME directory found. Cannot dump queue.\n");
        return;
    }

    gchar * path = g_strconcat (aud_get_path (AUD_PATH_USER_DIR), "/scrobblerqueue.txt", NULL);

    if (! (fd = fopen (path, "w")))
    {
        AUDDBG("Failure opening %s\n", path);
        g_free (path);
        return;
    }

    AUDDBG("Opening %s\n", path);
    g_free (path);

    q_peekall(1);

    /*
     * The sixth field used to be "L" for tracks which were valid submissions,
     * and "S" for tracks which were still playing, and shouldn't be submitted
     * yet.  We only store valid submissions in the queue, now, but the "L" value
     * is left in-place for backwards-compatibility reasons.  It should probably
     * be removed at some point.
     */
    while ((item = q_peekall(0))) {
        fprintf(fd, "%s\t%s\t%s\t%d\t%d\t%s\t%d\n",
                    I_ARTIST(item),
                    I_ALBUM(item),
                    I_TITLE(item),
                    item->track,
                    I_LEN(item),
                    "L",
                    I_TIME(item));
    }

    fclose(fd);
}

/* This was made public */

void sc_cleaner(void)
{
    if(sc_submit_url != NULL)
        free(sc_submit_url);
    if(sc_username != NULL)
        free(sc_username);
    if(sc_password != NULL)
        free(sc_password);
    if(sc_challenge_hash != NULL)
        free(sc_challenge_hash);
    if(sc_srv_res != NULL)
        free(sc_srv_res);
    if(sc_major_error != NULL)
        free(sc_major_error);
    dump_queue();
    q_free();
    AUDDBG("scrobbler shutting down\n");
}

static void sc_checkhandshake(void)
{
    int wait;

    if (!sc_username || !sc_password)
        return;

    if (sc_hs_status)
        return;
    if (sc_hs_timeout < time(NULL))
    {
        sc_handshake();

        if(sc_hs_errors)
        {
            if(sc_hs_errors < 5)
                /* Retry after 60 seconds */
                wait = 60;
            else
                wait = /* sc_submit_interval + */
                    ( ((sc_hs_errors - 5) < 7) ?
                    (60 << (sc_hs_errors-5)) :
                    7200 );
            sc_hs_timeout = time(NULL) + wait;
            AUDDBG("Error while handshaking. Retrying "
                "after %d seconds.\n", wait);
        }
    }
}

/**** Public *****/

/* Called at session startup*/

void sc_init(char *uname, char *pwd, char *url)
{
    sc_hs_status = sc_hs_timeout = sc_hs_errors = sc_submit_timeout =
        sc_srv_res_size = sc_giveup = sc_major_error_present =
        sc_bad_users = sc_sb_errors = 0;
    sc_submit_interval = 1;

    sc_submit_url = sc_username = sc_password = sc_srv_res =
        sc_challenge_hash = sc_major_error = NULL;
    sc_username = strdup(uname);
    sc_password = strdup(pwd);
    if (url)
        sc_hs_url = strdup(url);
    else
        sc_hs_url = strdup(LASTFM_HS_URL);
    read_cache();
    AUDDBG("scrobbler starting up\n");
}

void sc_addentry(GMutex *mutex, Tuple *tuple, int len)
{
    g_mutex_lock(mutex);

    sc_submit_np(tuple);
    set_np(tuple, len);

    /*
     * This will help make sure the queue will be saved on a nasty
     * segfault...
     */
    dump_queue();

    g_mutex_unlock(mutex);
}

/* Call periodically from the plugin */
int sc_idle(GMutex *mutex)
{
    sc_checkhandshake();
    if (sc_hs_status)
        sc_handlequeue(mutex);

    return sc_giveup;
}
