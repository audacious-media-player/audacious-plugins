#include <pthread.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <curl/curl.h>
#include <stdio.h>
#include "fmt.h"
#include "md5.h"
#include "scrobbler.h"
#include "config.h"
#include "settings.h"
#include <glib.h>

#include <audacious/plugin.h>
#include <audacious/util.h>

#define SCROBBLER_HS_URL "http://post.audioscrobbler.com"
#define SCROBBLER_CLI_ID "aud"
#define SCROBBLER_HS_WAIT 1800
#define SCROBBLER_SB_WAIT 10
#define SCROBBLER_VERSION "1.2"
#define SCROBBLER_IMPLEMENTATION "0.2"		/* This is the implementation, not the player version. */
#define SCROBBLER_SB_MAXLEN 1024
#define CACHE_SIZE 1024

/* Scrobblerbackend for xmms plugin, first draft */

static int	sc_hs_status,
		sc_hs_timeout,
		sc_hs_errors,
		sc_sb_errors,
		sc_bad_users,
		sc_submit_interval,
		sc_submit_timeout,
		sc_srv_res_size,
		sc_giveup,
		sc_major_error_present;

static char 	*sc_submit_url,	/* queue */
		*sc_np_url,	/* np */
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

/* isn't there better way for that? --desowin */
gboolean sc_timeout(gpointer data) {
    if (q_queue_last && audacious_drct_get_playing())
        q_queue_last->timeplayed+=1;

    return TRUE;
}

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

static item_t *q_put(Tuple *tuple, int t, int len)
{
	item_t *item;
	const gchar *album;

	item = malloc(sizeof(item_t));

	item->artist = fmt_escape(aud_tuple_get_string(tuple, FIELD_ARTIST, NULL));
	item->title = fmt_escape(aud_tuple_get_string(tuple, FIELD_TITLE, NULL));
	item->len = len;
	item->track = aud_tuple_get_int(tuple, FIELD_TRACK_NUMBER, NULL);
        if (t == -1) { /* now playing song */
            item->timeplayed = 0;
            item->utctime = time(NULL);
        } else { /* item from queue */
            item->timeplayed = len;
            item->utctime = t;
        }

	album = aud_tuple_get_string(tuple, FIELD_ALBUM, NULL);
	if (album)
		item->album = fmt_escape((char*) album);
	else
		item->album = fmt_escape("");

	q_nitems++;

	item->next = NULL;

	if(q_queue_last == NULL)
		q_queue = q_queue_last = item;
	else
	{
        	q_queue_last->next = item;
		q_queue_last = item;
	}

	return item;
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

static int q_len(void)
{
	return q_nitems;
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
		pdebug("No reply from server", DEBUG);
		return -1;
	}
	*(sc_srv_res + sc_srv_res_size) = 0;

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
		pdebug(fmt_vastr("error: %s", sc_srv_res), DEBUG);

		return -1;
	}

	if (!strncmp(sc_srv_res, "UPDATE ", 7)) {
		interval = strstr(sc_srv_res, "INTERVAL");
		if(!interval)
		{
			pdebug("missing INTERVAL", DEBUG);
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
		pdebug(fmt_vastr("update client: %s", sc_srv_res + 7), DEBUG);

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
			pdebug("missing INTERVAL", DEBUG);
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
		pdebug("incorrect username/password", DEBUG);

		interval = strstr(sc_srv_res, "INTERVAL");
		if(!interval)
		{
			pdebug("missing INTERVAL", DEBUG);
		}
		else
		{
			*(interval - 1) = 0;
			sc_submit_interval = strtol(interval + 8, NULL, 10);
		}

		return -1;
	}

	pdebug(fmt_vastr("unknown server-reply '%s'", sc_srv_res), DEBUG);
	return -1;
}

static unsigned char *md5_string(char *pass, int len)
{
	md5_state_t md5state;
	static unsigned char md5pword[16];
		
	md5_init(&md5state);
	md5_append(&md5state, (unsigned const char *)pass, len);
	md5_finish(&md5state, md5pword);

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

static int sc_handshake(void)
{
	int status;
	char buf[65535];
	CURL *curl;
	time_t ts = time(NULL);
	char *auth_tmp;
	char *auth;

	auth_tmp = g_strdup_printf("%s%ld", sc_password, ts);
	auth = (char *)md5_string(auth_tmp, strlen(auth_tmp));
	g_free(auth_tmp);
	hexify(auth, strlen(auth));
	auth_tmp = g_strdup(sc_response_hash);

	g_snprintf(buf, sizeof(buf), "%s/?hs=true&p=%s&c=%s&v=%s&u=%s&t=%ld&a=%s",
			SCROBBLER_HS_URL, SCROBBLER_VERSION,
			SCROBBLER_CLI_ID, SCROBBLER_IMPLEMENTATION, sc_username, time(NULL),
			auth_tmp);
	g_free(auth_tmp);

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
			sc_store_res);
	memset(sc_curl_errbuf, 0, sizeof(sc_curl_errbuf));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, sc_curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	sc_hs_timeout = time(NULL) + SCROBBLER_HS_WAIT;

	if (status) {
		pdebug(sc_curl_errbuf, DEBUG);
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
		md5_state_t md5state;
		unsigned char md5pword[16];
		
		md5_init(&md5state);
		/*pdebug(fmt_vastr("Pass Hash: %s", sc_password), DEBUG);*/
		md5_append(&md5state, (unsigned const char *)sc_password,
				strlen(sc_password));
		/*pdebug(fmt_vastr("Challenge Hash: %s", sc_challenge_hash), DEBUG);*/
		md5_append(&md5state, (unsigned const char *)sc_challenge_hash,
				strlen(sc_challenge_hash));
		md5_finish(&md5state, md5pword);
		hexify((char*)md5pword, sizeof(md5pword));
		/*pdebug(fmt_vastr("Response Hash: %s", sc_response_hash), DEBUG);*/
	}

	sc_hs_errors = 0;
	sc_hs_status = 1;

	sc_free_res();

	pdebug(fmt_vastr("submiturl: %s - interval: %d", 
				sc_submit_url, sc_submit_interval), DEBUG);

	return 0;
}

static int sc_parse_sb_res(void)
{
	char *ch, *ch2;

	if (!sc_srv_res_size) {
		pdebug("No response from server", DEBUG);
		return -1;
	}
	*(sc_srv_res + sc_srv_res_size) = 0;

	if (!strncmp(sc_srv_res, "OK", 2)) {
		if ((ch = strstr(sc_srv_res, "INTERVAL"))) {
			sc_submit_interval = strtol(ch + 8, NULL, 10);
			pdebug(fmt_vastr("got new interval: %d",
						sc_submit_interval), DEBUG);
		}

		pdebug(fmt_vastr("submission ok: %s", sc_srv_res), DEBUG);

		return 0;
	}

	if (!strncmp(sc_srv_res, "BADAUTH", 7)) {
		if ((ch = strstr(sc_srv_res, "INTERVAL"))) {
			sc_submit_interval = strtol(ch + 8, NULL, 10);
			pdebug(fmt_vastr("got new interval: %d",
						sc_submit_interval), DEBUG);
		}

		pdebug("incorrect username/password", DEBUG);

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
			pdebug("3 BADAUTH returns on submission. Halting "
				"submissions until login fixed.", DEBUG)
			sc_throw_error("Incorrect username/password.\n"
				"Please fix in configuration.");
		}

		return -1;
	}

	if (!strncmp(sc_srv_res, "FAILED", 6))  {
		if ((ch = strstr(sc_srv_res, "INTERVAL"))) {
			sc_submit_interval = strtol(ch + 8, NULL, 10);
			pdebug(fmt_vastr("got new interval: %d",
						sc_submit_interval), DEBUG);
		}

		/* This could be important. (Such as FAILED - Get new plugin) */
		/*sc_throw_error(fmt_vastr("%s", sc_srv_res));*/

		pdebug(sc_srv_res, DEBUG);

		return -1;
	}

	if (!strncmp(sc_srv_res, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">", 50)) {
		ch = strstr(sc_srv_res, "<TITLE>");
		ch2 = strstr(sc_srv_res, "</TITLE>");
		if (ch != NULL && ch2 != NULL) {
			ch += strlen("<TITLE>");
			*ch2 = '\0';

			pdebug(fmt_vastr("HTTP Error (%d): '%s'",
					 atoi(ch), ch + 4), DEBUG);
//			*ch2 = '<'; // needed? --yaz
		}

		return -1;
	}

	pdebug(fmt_vastr("unknown server-reply %s", sc_srv_res), DEBUG);

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
		 * don't submit queued tracks which don't yet meet audioscrobbler
		 * requirements...
		 */
		if ((item->timeplayed < (item->len / 2)) &&
		    (item->timeplayed < 240))
			continue;

		if (!item)
			return i;

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

		pdebug(fmt_vastr("a[%d]=%s t[%d]=%s l[%d]=%d i[%d]=%d b[%d]=%s",
				i, I_ARTIST(item),
				i, I_TITLE(item),
				i, I_LEN(item),
				i, I_TIME(item),
				i, I_ALBUM(item)), DEBUG);
#ifdef ALLOW_MULTIPLE
		i++;
	}
#endif

	return i;
}

static int sc_submit_np(Tuple *tuple)
{
	CURL *curl;
	/* struct HttpPost *post = NULL , *last = NULL; */
	int status;
	gchar *entry;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, sc_np_url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			sc_store_res);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	/*cfa(&post, &last, "debug", "failed");*/

        char *field_artist = fmt_escape(aud_tuple_get_string(tuple, FIELD_ARTIST, NULL));
        char *field_title = fmt_escape(aud_tuple_get_string(tuple, FIELD_TITLE, NULL));
        char *field_album = aud_tuple_get_string(tuple, FIELD_ALBUM, NULL) ? fmt_escape(aud_tuple_get_string(tuple, FIELD_ALBUM, NULL)) : fmt_escape("");
	entry = g_strdup_printf("s=%s&a=%s&t=%s&b=%s&l=%d&n=%d&m=", sc_session_id,
		field_artist,
		field_title,
		field_album,
		aud_tuple_get_int(tuple, FIELD_LENGTH, NULL) / 1000,
		aud_tuple_get_int(tuple, FIELD_TRACK_NUMBER, NULL));
        curl_free(field_artist);
        curl_free(field_title);
        curl_free(field_album);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *) entry);
	memset(sc_curl_errbuf, 0, sizeof(sc_curl_errbuf));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, sc_curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, SCROBBLER_SB_WAIT);

	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	g_free(entry);

	if (status) {
		pdebug(sc_curl_errbuf, DEBUG);
		sc_sb_errors++;
		sc_free_res();
		return -1;
	}

	if (sc_parse_sb_res()) {
		sc_sb_errors++;
		sc_free_res();
		pdebug(fmt_vastr("Retrying in %d secs, %d elements in queue",
					sc_submit_interval, q_len()), DEBUG);
		return -1;
	}
	sc_free_res();
	return 0;
}

static int sc_submitentry(gchar *entry)
{
	CURL *curl;
	/* struct HttpPost *post = NULL , *last = NULL; */
	int status;
        GString *submission;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, sc_submit_url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			sc_store_res);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	/*cfa(&post, &last, "debug", "failed");*/

	/*pdebug(fmt_vastr("Response Hash: %s", sc_response_hash), DEBUG);*/
        submission = g_string_new("s=");
        g_string_append(submission, (gchar *)sc_session_id);
	g_string_append(submission, entry);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)submission->str);
	memset(sc_curl_errbuf, 0, sizeof(sc_curl_errbuf));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, sc_curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, SCROBBLER_SB_WAIT);

	status = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
        g_string_free(submission, TRUE);

	if (status) {
		pdebug(sc_curl_errbuf, DEBUG);
		sc_sb_errors++;
		sc_free_res();
		return -1;
	}

	if (sc_parse_sb_res()) {
		sc_sb_errors++;
		sc_free_res();
		pdebug(fmt_vastr("Retrying in %d secs, %d elements in queue",
					sc_submit_interval, q_len()), DEBUG);
		return -1;
	}
	sc_free_res();
	return 0;
}

static void sc_handlequeue(GMutex *mutex)
{
	GString *submitentry;
	int nsubmit;
	int wait;

	if(sc_submit_timeout < time(NULL) && sc_bad_users < 3)
	{
		submitentry = g_string_new("");

		g_mutex_lock(mutex);

		nsubmit = sc_generateentry(submitentry);

		g_mutex_unlock(mutex);

		if (nsubmit > 0)
		{
			pdebug(fmt_vastr("Number submitting: %d", nsubmit), DEBUG);
			pdebug(fmt_vastr("Submission: %s", submitentry->str), DEBUG);

			if(!sc_submitentry(submitentry->str))
			{
				g_mutex_lock(mutex);

#ifdef ALLOW_MULTIPLE
				q_free();
#else
				q_get();
#endif
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
				if(sc_sb_errors < 5)
					/* Retry after 1 min */
					wait = 60;
				else
					wait = /* sc_submit_interval + */
						( ((sc_sb_errors - 5) < 7) ?
						(60 << (sc_sb_errors-5)) :
						7200 );
				
				sc_submit_timeout = time(NULL) + wait;

				pdebug(fmt_vastr("Error while submitting. "
					"Retrying after %d seconds.", wait),
					DEBUG);
			}
		}

		g_string_free(submitentry, TRUE);
	}
}

static void read_cache(void)
{
    FILE *fd;
    char buf[PATH_MAX];
    int i=0;
    item_t *item;
    gchar* config_datadir;

    config_datadir = audacious_get_localdir();
    g_snprintf(buf, sizeof(buf), "%s/scrobblerqueue.txt", config_datadir);
    g_free(config_datadir);

    if (!(fd = fopen(buf, "r")))
        return;
    pdebug(fmt_vastr("Opening %s", buf), DEBUG);
    fclose(fd);

    gchar* cache;
    gchar** values;
    gchar** entry;
    g_file_get_contents(buf, &cache, NULL, NULL);
    values = g_strsplit(cache, "\n", 0);

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

            if (!strncmp(entry[5], "L", 1)) {
                Tuple *tuple = aud_tuple_new();
                gchar* string_value;
                string_value = xmms_urldecode_plain(artist);
                aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, string_value);
                g_free(string_value);
                string_value = xmms_urldecode_plain(title);
                aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, string_value);
                g_free(string_value);
                string_value = xmms_urldecode_plain(album);
                aud_tuple_associate_string(tuple, FIELD_ALBUM, NULL, string_value);
                g_free(string_value);
                aud_tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, track);
                item = q_put(tuple, t, len);

                aud_tuple_free(tuple);

                pdebug(fmt_vastr("a[%d]=%s t[%d]=%s l[%d]=%d i[%d]=%d b[%d]=%s",
                                 i, I_ARTIST(item),
                                 i, I_TITLE(item),
                                 i, I_LEN(item),
                                 i, I_TIME(item),
                                 i, I_ALBUM(item)), DEBUG);
            }

            free(artist);
            free(title);
            free(album);
            g_free(entry);
            i++;
        }
    }
    g_free(values);
    g_free(cache);
    pdebug("Done loading cache.", DEBUG);
}

static void dump_queue(void)
{
    FILE *fd;
    item_t *item;
    char *home, buf[PATH_MAX];
    gchar* config_datadir;

    /*pdebug("Entering dump_queue();", DEBUG);*/

    if (!(home = getenv("HOME")))
    {
        pdebug("No HOME directory found. Cannot dump queue.", DEBUG);
        return;
    }

    config_datadir = audacious_get_localdir();
    g_snprintf(buf, sizeof(buf), "%s/scrobblerqueue.txt", config_datadir);
    g_free(config_datadir);

    if (!(fd = fopen(buf, "w")))
    {
        pdebug(fmt_vastr("Failure opening %s", buf), DEBUG);
        return;
    }

    pdebug(fmt_vastr("Opening %s", buf), DEBUG);

    q_peekall(1);

    while ((item = q_peekall(0))) {
        fprintf(fd, "%s\t%s\t%s\t%d\t%d\t%s\t%d\n",
                    I_ARTIST(item),
                    I_ALBUM(item),
                    I_TITLE(item),
                    item->track,
                    I_LEN(item),
                    ((item->timeplayed > item->len/2) || (item->timeplayed > 240)) ? "L" : "S",
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
	pdebug("scrobbler shutting down", DEBUG);
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
			pdebug(fmt_vastr("Error while handshaking. Retrying "
				"after %d seconds.", wait), DEBUG);
		}
	}
}

/**** Public *****/

/* Called at session startup*/

void sc_init(char *uname, char *pwd)
{
	sc_hs_status = sc_hs_timeout = sc_hs_errors = sc_submit_timeout =
		sc_srv_res_size = sc_giveup = sc_major_error_present =
		sc_bad_users = sc_sb_errors = 0;
	sc_submit_interval = 1;

	sc_submit_url = sc_username = sc_password = sc_srv_res =
		sc_challenge_hash = sc_major_error = NULL;
	sc_username = strdup(uname);
	sc_password = strdup(pwd);
	read_cache();
	pdebug("scrobbler starting up", DEBUG);
}

void sc_addentry(GMutex *mutex, Tuple *tuple, int len)
{
	g_mutex_lock(mutex);

	sc_submit_np(tuple);
	q_put(tuple, -1, len);

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
