#include "settings.h"

#include <pthread.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "fmt.h"
#include "plugin.h"
#include "scrobbler.h"
#include "config.h"
#include <glib.h>
#include <audacious/plugin.h>
#include <libaudcore/md5.h>

#define SCROBBLER_HS_URL "http://post.gerpok.com"
#define SCROBBLER_CLI_ID "aud"
#define SCROBBLER_HS_WAIT 1800
#define SCROBBLER_SB_WAIT 10
#define SCROBBLER_VERSION "1.1"
#define SCROBBLER_IMPLEMENTATION "0.1"		/* This is the implementation, not the player version. */
#define SCROBBLER_SB_MAXLEN 1024
#define CACHE_SIZE 1024
#define ALLOW_MULTIPLE

/* Scrobblerbackend for xmms plugin, first draft */

static int	gerpok_sc_hs_status,
		gerpok_sc_hs_timeout,
		gerpok_sc_hs_errors,
		gerpok_sc_sb_errors,
		gerpok_sc_bad_users,
		gerpok_sc_submit_interval,
		gerpok_sc_submit_timeout,
		gerpok_sc_srv_res_size,
		gerpok_sc_giveup,
		gerpok_sc_major_error_present;

static char 	*gerpok_sc_submit_url,
		*gerpok_sc_username = NULL,
		*gerpok_sc_password = NULL,
		*gerpok_sc_challenge_hash,
		gerpok_sc_response_hash[33],
		*gerpok_sc_srv_res,
		gerpok_sc_curl_errbuf[CURL_ERROR_SIZE],
		*gerpok_sc_major_error;

static void dump_queue();

/**** Queue stuff ****/

#define I_ARTIST(i) i->artist
#define I_TITLE(i) i->title
#define I_TIME(i) i->utctime
#define I_LEN(i) i->len
#define I_MB(i) i->mb
#define I_ALBUM(i) i->album

typedef struct {
	char *artist,
		*title,
		*mb,
		*album,
		*utctime,
		len[16];
	int numtries;
	void *next;
} item_t;

static item_t *q_queue = NULL;
static item_t *q_queue_last = NULL;
static int q_nitems;

static void q_item_free(item_t *item)
{
	if (item == NULL)
		return;
	curl_free(item->artist);
	curl_free(item->title);
	curl_free(item->utctime);
	curl_free(item->mb);
	curl_free(item->album);
	free(item);
}

static void q_put(Tuple *tuple, int len)
{
	item_t *item;
	const gchar *album;

	item = malloc(sizeof(item_t));

	item->artist = fmt_escape(aud_tuple_get_string(tuple, FIELD_ARTIST, NULL));
	item->title = fmt_escape(aud_tuple_get_string(tuple, FIELD_TITLE, NULL));
	item->utctime = fmt_escape(fmt_timestr(time(NULL), 1));
	g_snprintf(item->len, sizeof(item->len), "%d", len);

#ifdef NOTYET
	if(tuple->mb == NULL)
#endif
		item->mb = fmt_escape("");
#ifdef NOTYET
	else
		item->mb = fmt_escape((char*)tuple->mb);
#endif

	if((album = aud_tuple_get_string(tuple, FIELD_ALBUM, NULL)))
		item->album = fmt_escape("");
	else
		item->album = fmt_escape((char*) album);

	q_nitems++;

	item->next = NULL;

	if(q_queue_last == NULL)
		q_queue = q_queue_last = item;
	else
	{
        	q_queue_last->next = item;
		q_queue_last = item;
	}
}

static item_t *q_put2(char *artist, char *title, char *len, char *time,
		char *album, char *mb)
{
	char *temp = NULL;
	item_t *item;

	item = calloc(1, sizeof(item_t));
	temp = fmt_unescape(artist);
	item->artist = fmt_escape(temp);
	curl_free(temp);
	temp = fmt_unescape(title);
	item->title = fmt_escape(temp);
	curl_free(temp);
	memcpy(item->len, len, sizeof(len));
	temp = fmt_unescape(time);
	item->utctime = fmt_escape(temp);
	curl_free(temp);
	temp = fmt_unescape(album);
	item->album = fmt_escape(temp);
	curl_free(temp);
	temp = fmt_unescape(mb);
	item->mb = fmt_escape(temp);
	curl_free(temp);

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

static void gerpok_sc_throw_error(char *errortxt)
{
	gerpok_sc_major_error_present = 1;
	if(gerpok_sc_major_error == NULL)
		gerpok_sc_major_error = strdup(errortxt);

	return;
}

int gerpok_sc_catch_error(void)
{
	return gerpok_sc_major_error_present;
}

char *gerpok_sc_fetch_error(void)
{
	return gerpok_sc_major_error;
}

void gerpok_sc_clear_error(void)
{
	gerpok_sc_major_error_present = 0;
	if(gerpok_sc_major_error != NULL)
		free(gerpok_sc_major_error);
	gerpok_sc_major_error = NULL;

	return;
}

static size_t gerpok_sc_store_res(void *ptr, size_t size,
		size_t nmemb,
		void *stream __attribute__((unused)))
{
	int len = size * nmemb;

	gerpok_sc_srv_res = realloc(gerpok_sc_srv_res, gerpok_sc_srv_res_size + len + 1);
	memcpy(gerpok_sc_srv_res + gerpok_sc_srv_res_size,
			ptr, len);
	gerpok_sc_srv_res_size += len;
	return len;
}

static void gerpok_sc_free_res(void)
{
	if(gerpok_sc_srv_res != NULL)
		free(gerpok_sc_srv_res);
	gerpok_sc_srv_res = NULL;
	gerpok_sc_srv_res_size = 0;
}

static int gerpok_sc_parse_hs_res(void)
{
	char *interval;

	if (!gerpok_sc_srv_res_size) {
		AUDDBG("No reply from server");
		return -1;
	}
	*(gerpok_sc_srv_res + gerpok_sc_srv_res_size) = 0;

	if (!strncmp(gerpok_sc_srv_res, "FAILED ", 7)) {
		interval = strstr(gerpok_sc_srv_res, "INTERVAL");
		if(!interval) {
			AUDDBG("missing INTERVAL");
		}
		else
		{
			*(interval - 1) = 0;
			gerpok_sc_submit_interval = strtol(interval + 8, NULL, 10);
		}

		/* Throwing a major error, just in case */
		/* gerpok_sc_throw_error(fmt_vastr("%s", gerpok_sc_srv_res));
		   gerpok_sc_hs_errors++; */
		AUDDBG("error: %s", gerpok_sc_srv_res);

		return -1;
	}

	if (!strncmp(gerpok_sc_srv_res, "UPDATE ", 7)) {
		interval = strstr(gerpok_sc_srv_res, "INTERVAL");
		if(!interval)
		{
			AUDDBG("missing INTERVAL");
		}
		else
		{
			*(interval - 1) = 0;
			gerpok_sc_submit_interval = strtol(interval + 8, NULL, 10);
		}

		gerpok_sc_submit_url = strchr(strchr(gerpok_sc_srv_res, '\n') + 1, '\n') + 1;
		*(gerpok_sc_submit_url - 1) = 0;
		gerpok_sc_submit_url = strdup(gerpok_sc_submit_url);
		gerpok_sc_challenge_hash = strchr(gerpok_sc_srv_res, '\n') + 1;
		*(gerpok_sc_challenge_hash - 1) = 0;
		gerpok_sc_challenge_hash = strdup(gerpok_sc_challenge_hash);

		/* Throwing major error. Need to alert client to update. */
		gerpok_sc_throw_error(fmt_vastr("Please update Audacious.\n"
			"Update available at: http://audacious-media-player.org"));
		AUDDBG("update client: %s", gerpok_sc_srv_res + 7);

		/*
		 * Russ isn't clear on whether we can submit with a not-updated
		 * client.  Neither is RJ.  I use what we did before.
		 */
		gerpok_sc_giveup = -1;
		return -1;
	}
	if (!strncmp(gerpok_sc_srv_res, "UPTODATE\n", 9)) {
		gerpok_sc_bad_users = 0;

		interval = strstr(gerpok_sc_srv_res, "INTERVAL");
		if (!interval) {
			AUDDBG("missing INTERVAL");
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
			gerpok_sc_submit_interval = strtol(interval + 8, NULL, 10);
		}

		gerpok_sc_submit_url = strchr(strchr(gerpok_sc_srv_res, '\n') + 1, '\n') + 1;
		*(gerpok_sc_submit_url - 1) = 0;
		gerpok_sc_submit_url = strdup(gerpok_sc_submit_url);
		gerpok_sc_challenge_hash = strchr(gerpok_sc_srv_res, '\n') + 1;
		*(gerpok_sc_challenge_hash - 1) = 0;
		gerpok_sc_challenge_hash = strdup(gerpok_sc_challenge_hash);

		return 0;
	}
	if(!strncmp(gerpok_sc_srv_res, "BADUSER", 7)) {
		/* Throwing major error. */
		gerpok_sc_throw_error("Incorrect username/password.\n"
				"Please fix in configuration.");
		AUDDBG("incorrect username/password");

		interval = strstr(gerpok_sc_srv_res, "INTERVAL");
		if(!interval)
		{
			AUDDBG("missing INTERVAL");
		}
		else
		{
			*(interval - 1) = 0;
			gerpok_sc_submit_interval = strtol(interval + 8, NULL, 10);
		}

		return -1;
	}

	AUDDBG("unknown server-reply '%s'", gerpok_sc_srv_res);
	return -1;
}

static void hexify(char *pass, int len)
{
	char *bp = gerpok_sc_response_hash;
	char hexchars[] = "0123456789abcdef";
	int i;

	memset(gerpok_sc_response_hash, 0, sizeof(gerpok_sc_response_hash));
	
	for(i = 0; i < len; i++) {
		*(bp++) = hexchars[(pass[i] >> 4) & 0x0f];
		*(bp++) = hexchars[pass[i] & 0x0f];
	}
	*bp = 0;

	return;
}

static int gerpok_sc_handshake(void)
{
	int status;
	char buf[4096];
	CURL *curl;

	g_snprintf(buf, sizeof(buf), "%s/?hs=true&p=%s&c=%s&v=%s&u=%s",
			SCROBBLER_HS_URL, SCROBBLER_VERSION,
			SCROBBLER_CLI_ID, SCROBBLER_IMPLEMENTATION, gerpok_sc_username);

	curl = curl_easy_init();
        setup_proxy(curl);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
			gerpok_sc_store_res);
	memset(gerpok_sc_curl_errbuf, 0, sizeof(gerpok_sc_curl_errbuf));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, gerpok_sc_curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	gerpok_sc_hs_timeout = time(NULL) + SCROBBLER_HS_WAIT;

	if (status) {
		AUDDBG(gerpok_sc_curl_errbuf);
		gerpok_sc_hs_errors++;
		gerpok_sc_free_res();
		return -1;
	}

	if (gerpok_sc_parse_hs_res()) {
		gerpok_sc_hs_errors++;
		gerpok_sc_free_res();
		return -1;
	}

	if (gerpok_sc_challenge_hash != NULL) {
		aud_md5state_t md5state;
		unsigned char md5pword[16];
		
		aud_md5_init(&md5state);
		/*AUDDBG("Pass Hash: %s", gerpok_sc_password);*/
		aud_md5_append(&md5state, (unsigned const char *)gerpok_sc_password,
				strlen(gerpok_sc_password));
		/*AUDDBG("Challenge Hash: %s", gerpok_sc_challenge_hash);*/
		aud_md5_append(&md5state, (unsigned const char *)gerpok_sc_challenge_hash,
				strlen(gerpok_sc_challenge_hash));
		aud_md5_finish(&md5state, md5pword);
		hexify((char*)md5pword, sizeof(md5pword));
		/*AUDDBG("Response Hash: %s", gerpok_sc_response_hash));*/
	}

	gerpok_sc_hs_errors = 0;
	gerpok_sc_hs_status = 1;

	gerpok_sc_free_res();

	AUDDBG("submiturl: %s - interval: %d", 
				gerpok_sc_submit_url, gerpok_sc_submit_interval);

	return 0;
}

static int gerpok_sc_parse_sb_res(void)
{
	char *ch, *ch2;

	if (!gerpok_sc_srv_res_size) {
		AUDDBG("No response from server");
		return -1;
	}
	*(gerpok_sc_srv_res + gerpok_sc_srv_res_size) = 0;

	if (!strncmp(gerpok_sc_srv_res, "OK", 2)) {
		if ((ch = strstr(gerpok_sc_srv_res, "INTERVAL"))) {
			gerpok_sc_submit_interval = strtol(ch + 8, NULL, 10);
			AUDDBG("got new interval: %d",
						gerpok_sc_submit_interval);
		}

		AUDDBG("submission ok: %s", gerpok_sc_srv_res);

		return 0;
	}

	if (!strncmp(gerpok_sc_srv_res, "BADAUTH", 7)) {
		if ((ch = strstr(gerpok_sc_srv_res, "INTERVAL"))) {
			gerpok_sc_submit_interval = strtol(ch + 8, NULL, 10);
			AUDDBG("got new interval: %d",
						gerpok_sc_submit_interval);
		}

		AUDDBG("incorrect username/password");

		gerpok_sc_giveup = 0;

		/*
		 * We obviously aren't authenticated.  The server might have
		 * lost our handshake status though, so let's try
		 * re-handshaking...  This might not be proper.
		 * (we don't give up)
		 */
		gerpok_sc_hs_status = 0;

		if(gerpok_sc_challenge_hash != NULL)
			free(gerpok_sc_challenge_hash);
		if(gerpok_sc_submit_url != NULL)
			free(gerpok_sc_submit_url);

		gerpok_sc_challenge_hash = gerpok_sc_submit_url = NULL;
		gerpok_sc_bad_users++;

		if(gerpok_sc_bad_users > 2)
		{
			AUDDBG("3 BADAUTH returns on submission. Halting "
				"submissions until login fixed.");
			gerpok_sc_throw_error("Incorrect username/password.\n"
				"Please fix in configuration.");
		}

		return -1;
	}

	if (!strncmp(gerpok_sc_srv_res, "FAILED", 6))  {
		if ((ch = strstr(gerpok_sc_srv_res, "INTERVAL"))) {
			gerpok_sc_submit_interval = strtol(ch + 8, NULL, 10);
			AUDDBG("got new interval: %d",
						gerpok_sc_submit_interval);
		}

		/* This could be important. (Such as FAILED - Get new plugin) */
		/*gerpok_sc_throw_error(fmt_vastr("%s", gerpok_sc_srv_res));*/

		AUDDBG(gerpok_sc_srv_res);

		return -1;
	}

	if (!strncmp(gerpok_sc_srv_res, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">", 50)) {
		ch = strstr(gerpok_sc_srv_res, "<TITLE>");
		ch2 = strstr(gerpok_sc_srv_res, "</TITLE>");
		if (ch != NULL && ch2 != NULL) {
			ch += strlen("<TITLE>");
			*ch2 = '\0';

			AUDDBG("HTTP Error (%d): '%s'",
					 atoi(ch), ch + 4);
//			*ch2 = '<'; // needed? --yaz
		}

		return -1;
	}

	AUDDBG("unknown server-reply %s", gerpok_sc_srv_res);

	return -1;
}

static gchar *gerpok_sc_itemtag(char c, int n, char *str)
{
	static char buf[SCROBBLER_SB_MAXLEN]; 
	g_snprintf(buf, SCROBBLER_SB_MAXLEN, "&%c[%d]=%s", c, n, str);
	return buf;
}

#define cfa(f, l, n, v) \
curl_formadd(f, l, CURLFORM_COPYNAME, n, \
		CURLFORM_PTRCONTENTS, v, CURLFORM_END)

static int gerpok_sc_generateentry(GString *submission)
{
	int i;
	item_t *item;

	i = 0;
#ifdef ALLOW_MULTIPLE
	q_peekall(1);
	while ((item = q_peekall(0)) && i < 10) {
#else
		item = q_peek();
#endif
		if (!item)
			return i;

                g_string_append(submission,gerpok_sc_itemtag('a',i,I_ARTIST(item)));
                g_string_append(submission,gerpok_sc_itemtag('t',i,I_TITLE(item)));
                g_string_append(submission,gerpok_sc_itemtag('l',i,I_LEN(item)));
                g_string_append(submission,gerpok_sc_itemtag('i',i,I_TIME(item)));
                g_string_append(submission,gerpok_sc_itemtag('m',i,I_MB(item)));
                g_string_append(submission,gerpok_sc_itemtag('b',i,I_ALBUM(item)));

		AUDDBG("a[%d]=%s t[%d]=%s l[%d]=%s i[%d]=%s m[%d]=%s b[%d]=%s",
				i, I_ARTIST(item),
				i, I_TITLE(item),
				i, I_LEN(item),
				i, I_TIME(item),
				i, I_MB(item),
				i, I_ALBUM(item));
#ifdef ALLOW_MULTIPLE
		i++;
	}
#endif

	return i;
}

static int gerpok_sc_submitentry(gchar *entry)
{
	CURL *curl;
	/* struct HttpPost *post = NULL , *last = NULL; */
	int status;
        GString *submission;

	curl = curl_easy_init();
        setup_proxy(curl);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, gerpok_sc_submit_url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			gerpok_sc_store_res);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	/*cfa(&post, &last, "debug", "failed");*/

	/*AUDDBG("Username: %s", gerpok_sc_username));*/
        submission = g_string_new("u=");
        g_string_append(submission,(gchar *)gerpok_sc_username);

	/*AUDDBG("Response Hash: %s", gerpok_sc_response_hash));*/
        g_string_append(submission,"&s=");
        g_string_append(submission,(gchar *)gerpok_sc_response_hash);

	g_string_append(submission, entry);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)submission->str);
	memset(gerpok_sc_curl_errbuf, 0, sizeof(gerpok_sc_curl_errbuf));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, gerpok_sc_curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);

	/*
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, SCROBBLER_SB_WAIT);
	*/

	status = curl_easy_perform(curl);

	curl_easy_cleanup(curl);

        g_string_free(submission,TRUE);

	if (status) {
		AUDDBG(gerpok_sc_curl_errbuf);
		gerpok_sc_sb_errors++;
		gerpok_sc_free_res();
		return -1;
	}

	if (gerpok_sc_parse_sb_res()) {
		gerpok_sc_sb_errors++;
		gerpok_sc_free_res();
		AUDDBG("Retrying in %d secs, %d elements in queue",
					gerpok_sc_submit_interval, q_len());
		return -1;
	}
	gerpok_sc_free_res();
	return 0;
}

static void gerpok_sc_handlequeue(GMutex *mutex)
{
	GString *submitentry;
	int nsubmit;
	int wait;

	if(gerpok_sc_submit_timeout < time(NULL) && gerpok_sc_bad_users < 3)
	{
		submitentry = g_string_new("");

		g_mutex_lock(mutex);

		nsubmit = gerpok_sc_generateentry(submitentry);

		g_mutex_unlock(mutex);

		if (nsubmit > 0)
		{
			AUDDBG("Number submitting: %d", nsubmit);
			AUDDBG("Submission: %s", submitentry->str);

			if(!gerpok_sc_submitentry(submitentry->str))
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

				gerpok_sc_sb_errors = 0;
			}
			if(gerpok_sc_sb_errors)
			{
				if(gerpok_sc_sb_errors < 5)
					/* Retry after 1 min */
					wait = 60;
				else
					wait = /* gerpok_sc_submit_interval + */
						( ((gerpok_sc_sb_errors - 5) < 7) ?
						(60 << (gerpok_sc_sb_errors-5)) :
						7200 );
				
				gerpok_sc_submit_timeout = time(NULL) + wait;

				AUDDBG("Error while submitting. "
					"Retrying after %d seconds.", wait);
			}
		}

		g_string_free(submitentry, TRUE);
	}
}

static void read_cache(void)
{
	FILE *fd;
	char buf[PATH_MAX], *cache = NULL, *ptr1, *ptr2;
	int cachesize, written, i = 0;
	item_t *item;
	gchar* config_datadir;

	cachesize = written = 0;

	config_datadir = audacious_get_localdir();
	g_snprintf(buf, sizeof(buf), "%s/gerpokqueue.txt", config_datadir);
	g_free(config_datadir);

	if (!(fd = fopen(buf, "r")))
		return;
	AUDDBG("Opening %s", buf);
	while(!feof(fd))
	{
		cachesize += CACHE_SIZE;
		cache = realloc(cache, cachesize + 1);
		written += fread(cache + written, 1, CACHE_SIZE, fd);
		cache[written] = '\0';
	}
	fclose(fd);
	ptr1 = cache;
	while(ptr1 < cache + written - 1)
	{
		char *artist, *title, *len, *time, *album, *mb;

		AUDDBG("Pushed:");
		ptr2 = strchr(ptr1, ' ');
		artist = calloc(1, ptr2 - ptr1 + 1);
		strncpy(artist, ptr1, ptr2 - ptr1);
		ptr1 = ptr2 + 1;
		ptr2 = strchr(ptr1, ' ');
		title = calloc(1, ptr2 - ptr1 + 1);
		strncpy(title, ptr1, ptr2 - ptr1);
		ptr1 = ptr2 + 1;
		ptr2 = strchr(ptr1, ' ');
		len = calloc(1, ptr2 - ptr1 + 1);
		strncpy(len, ptr1, ptr2 - ptr1);
		ptr1 = ptr2 + 1;
		ptr2 = strchr(ptr1, ' ');
		time = calloc(1, ptr2 - ptr1 + 1);
		strncpy(time, ptr1, ptr2 - ptr1);
		ptr1 = ptr2 + 1;
		ptr2 = strchr(ptr1, ' ');
		album = calloc(1, ptr2 - ptr1 + 1);
		strncpy(album, ptr1, ptr2 - ptr1);
		ptr1 = ptr2 + 1;
		ptr2 = strchr(ptr1, '\n');
		if(ptr2 != NULL)
			*ptr2 = '\0';
		mb = calloc(1, strlen(ptr1) + 1);
		strncpy(mb, ptr1, strlen(ptr1));
		if(ptr2 != NULL)
			*ptr2 = '\n';
		/* Why is our save printing out CR/LF? */
		ptr1 = ptr2 + 1;

		item = q_put2(artist, title, len, time, album, mb);
		AUDDBG("a[%d]=%s t[%d]=%s l[%d]=%s i[%d]=%s m[%d]=%s b[%d]=%s",
				i, I_ARTIST(item),
				i, I_TITLE(item),
				i, I_LEN(item),
				i, I_TIME(item),
				i, I_MB(item),
				i, I_ALBUM(item));
		free(artist);
		free(title);
		free(len);
		free(time);
		free(album);
		free(mb);

		i++;
	}
	AUDDBG("Done loading cache.");
	free(cache);
}

static void dump_queue(void)
{
	FILE *fd;
	item_t *item;
	char *home, buf[PATH_MAX];
	gchar* config_datadir;

	/*AUDDBG("Entering dump_queue();");*/

	if (!(home = getenv("HOME")))
	{
		AUDDBG("No HOME directory found. Cannot dump queue.");
		return;
	}

	config_datadir = audacious_get_localdir();
	g_snprintf(buf, sizeof(buf), "%s/gerpokqueue.txt", config_datadir);
	g_free(config_datadir);

	if (!(fd = fopen(buf, "w")))
	{
		AUDDBG("Failure opening %s", buf);
		return;
	}

	AUDDBG("Opening %s", buf);

	q_peekall(1);

	while ((item = q_peekall(0))) {
		fprintf(fd, "%s %s %s %s %s %s\n",
					I_ARTIST(item),
					I_TITLE(item),
					I_LEN(item),
					I_TIME(item),
					I_ALBUM(item),
					I_MB(item));
	}

	fclose(fd);
}

/* This was made public */

void gerpok_sc_cleaner(void)
{
	if(gerpok_sc_submit_url != NULL)
		free(gerpok_sc_submit_url);
	if(gerpok_sc_username != NULL)
		free(gerpok_sc_username);
	if(gerpok_sc_password != NULL)
		free(gerpok_sc_password);
	if(gerpok_sc_challenge_hash != NULL)
		free(gerpok_sc_challenge_hash);
	if(gerpok_sc_srv_res != NULL)
		free(gerpok_sc_srv_res);
	if(gerpok_sc_major_error != NULL)
		free(gerpok_sc_major_error);
	dump_queue();
	q_free();
	AUDDBG("scrobbler shutting down");
}

static void gerpok_sc_checkhandshake(void)
{
	int wait;

	if (!gerpok_sc_username || !gerpok_sc_password)
		return;

	if (gerpok_sc_hs_status)
		return;
	if (gerpok_sc_hs_timeout < time(NULL))
	{
		gerpok_sc_handshake();

		if(gerpok_sc_hs_errors)
		{
			if(gerpok_sc_hs_errors < 5)
				/* Retry after 60 seconds */
				wait = 60;
			else
				wait = /* gerpok_sc_submit_interval + */
					( ((gerpok_sc_hs_errors - 5) < 7) ?
					(60 << (gerpok_sc_hs_errors-5)) :
					7200 );
			gerpok_sc_hs_timeout = time(NULL) + wait;
			AUDDBG("Error while handshaking. Retrying "
				"after %d seconds.", wait);
		}
	}
}

/**** Public *****/

/* Called at session startup*/

void gerpok_sc_init(char *uname, char *pwd)
{
	gerpok_sc_hs_status = gerpok_sc_hs_timeout = gerpok_sc_hs_errors = gerpok_sc_submit_timeout =
		gerpok_sc_srv_res_size = gerpok_sc_giveup = gerpok_sc_major_error_present =
		gerpok_sc_bad_users = gerpok_sc_sb_errors = 0;
	gerpok_sc_submit_interval = 100;

	gerpok_sc_submit_url = gerpok_sc_username = gerpok_sc_password = gerpok_sc_srv_res =
		gerpok_sc_challenge_hash = gerpok_sc_major_error = NULL;
	gerpok_sc_username = strdup(uname);
	gerpok_sc_password = strdup(pwd);
	read_cache();
	AUDDBG("scrobbler starting up");
}

void gerpok_sc_addentry(GMutex *mutex, Tuple *tuple, int len)
{
	g_mutex_lock(mutex);
	q_put(tuple, len);
	/*
	 * This will help make sure the queue will be saved on a nasty
	 * segfault...
	 */
	dump_queue();
	g_mutex_unlock(mutex);
}

/* Call periodically from the plugin */
int gerpok_sc_idle(GMutex *mutex)
{
	gerpok_sc_checkhandshake();
	if (gerpok_sc_hs_status)
		gerpok_sc_handlequeue(mutex);

	return gerpok_sc_giveup;
}
