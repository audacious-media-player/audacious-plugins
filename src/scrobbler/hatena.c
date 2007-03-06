#include "settings.h"

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
#include <glib.h>

#include <audacious/titlestring.h>
#include <audacious/util.h>

#define SCROBBLER_HS_URL "http://music.hatelabo.jp/trackauth"
#define SCROBBLER_CLI_ID "aud"
#define SCROBBLER_HS_WAIT 1800
#define SCROBBLER_SB_WAIT 10
#define SCROBBLER_VERSION "1.1"
#define SCROBBLER_IMPLEMENTATION "0.1"		/* This is the implementation, not the player version. */
#define SCROBBLER_SB_MAXLEN 1024
#define CACHE_SIZE 1024

#define HATENA_SUBMIT_INTERVAL 60

/* Scrobblerbackend for xmms plugin, first draft */

static int	hatena_sc_hs_status,
		hatena_sc_hs_timeout,
		hatena_sc_hs_errors,
		hatena_sc_sb_errors,
		hatena_sc_bad_users,
		hatena_sc_submit_interval = HATENA_SUBMIT_INTERVAL,
		hatena_sc_submit_timeout,
		hatena_sc_srv_res_size,
		hatena_sc_giveup,
		hatena_sc_major_error_present;

static char 	*hatena_sc_submit_url,
		*hatena_sc_username = NULL,
		*hatena_sc_password = NULL,
		*hatena_sc_challenge_hash,
		hatena_sc_response_hash[33],
		*hatena_sc_srv_res,
		hatena_sc_curl_errbuf[CURL_ERROR_SIZE],
		*hatena_sc_major_error;

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

static void q_put(TitleInput *tuple, int len)
{
	item_t *item;

	item = malloc(sizeof(item_t));

	item->artist = fmt_escape(tuple->performer);
	item->title = fmt_escape(tuple->track_name);
	item->utctime = fmt_escape(fmt_timestr(time(NULL), 1));
	snprintf(item->len, sizeof(item->len), "%d", len);

#ifdef NOTYET
	if(tuple->mb == NULL)
#endif
		item->mb = fmt_escape("");
#ifdef NOTYET
	else
		item->mb = fmt_escape((char*)tuple->mb);
#endif

	if(tuple->album_name == NULL)
		item->album = fmt_escape("");
	else
		item->album = fmt_escape((char*)tuple->album_name);

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

static item_t *q_peek(void)
{
	if (q_nitems == 0)
		return NULL;
	return q_queue;
}

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

static void hatena_sc_throw_error(char *errortxt)
{
	hatena_sc_major_error_present = 1;
	if(hatena_sc_major_error == NULL)
		hatena_sc_major_error = strdup(errortxt);

	return;
}

int hatena_sc_catch_error(void)
{
	return hatena_sc_major_error_present;
}

char *hatena_sc_fetch_error(void)
{
	return hatena_sc_major_error;
}

void hatena_sc_clear_error(void)
{
	hatena_sc_major_error_present = 0;
	if(hatena_sc_major_error != NULL)
		free(hatena_sc_major_error);
	hatena_sc_major_error = NULL;

	return;
}

static size_t hatena_sc_store_res(void *ptr, size_t size,
		size_t nmemb,
		void *stream __attribute__((unused)))
{
	int len = size * nmemb;

	hatena_sc_srv_res = realloc(hatena_sc_srv_res, hatena_sc_srv_res_size + len + 1);
	memcpy(hatena_sc_srv_res + hatena_sc_srv_res_size,
			ptr, len);
	hatena_sc_srv_res_size += len;
	return len;
}

static void hatena_sc_free_res(void)
{
	if(hatena_sc_srv_res != NULL)
		free(hatena_sc_srv_res);
	hatena_sc_srv_res = NULL;
	hatena_sc_srv_res_size = 0;
}

static int hatena_sc_parse_hs_res(void)
{
	char *interval;

	if (!hatena_sc_srv_res_size) {
		pdebug("No reply from server", DEBUG);
		return -1;
	}
	*(hatena_sc_srv_res + hatena_sc_srv_res_size) = 0;

	if (!strncmp(hatena_sc_srv_res, "FAILED ", 7)) {

		/* Throwing a major error, just in case */
		/* hatena_sc_throw_error(fmt_vastr("%s", hatena_sc_srv_res));
		   hatena_sc_hs_errors++; */
		pdebug(fmt_vastr("error: %s", hatena_sc_srv_res), DEBUG);

		return -1;
	}

	if (!strncmp(hatena_sc_srv_res, "UPDATE ", 7)) {

		hatena_sc_submit_url = strchr(strchr(hatena_sc_srv_res, '\n') + 1, '\n') + 1;
		*(hatena_sc_submit_url - 1) = 0;
		hatena_sc_submit_url = strdup(hatena_sc_submit_url);
		hatena_sc_challenge_hash = strchr(hatena_sc_srv_res, '\n') + 1;
		*(hatena_sc_challenge_hash - 1) = 0;
		hatena_sc_challenge_hash = strdup(hatena_sc_challenge_hash);

		/* Throwing major error. Need to alert client to update. */
		hatena_sc_throw_error(fmt_vastr("Please update Audacious.\n"
			"Update available at: http://audacious-media-player.org"));
		pdebug(fmt_vastr("update client: %s", hatena_sc_srv_res + 7), DEBUG);

		/*
		 * Russ isn't clear on whether we can submit with a not-updated
		 * client.  Neither is RJ.  I use what we did before.
		 */
		hatena_sc_giveup = -1;
		return -1;
	}
	if (!strncmp(hatena_sc_srv_res, "UPTODATE\n", 9)) {
		hatena_sc_bad_users = 0;

		hatena_sc_submit_url = strchr(strchr(hatena_sc_srv_res, '\n') + 1, '\n') + 1;
		*(hatena_sc_submit_url - 1) = 0;
		hatena_sc_submit_url = strdup(hatena_sc_submit_url);
		hatena_sc_challenge_hash = strchr(hatena_sc_srv_res, '\n') + 1;
		*(hatena_sc_challenge_hash - 1) = 0;
		hatena_sc_challenge_hash = strdup(hatena_sc_challenge_hash);

		return 0;
	}
	if(!strncmp(hatena_sc_srv_res, "BADUSER", 7)) {
		/* Throwing major error. */
		hatena_sc_throw_error("Incorrect username/password.\n"
				"Please fix in configuration.");
		pdebug("incorrect username/password", DEBUG);

		interval = strstr(hatena_sc_srv_res, "INTERVAL");
		if(!interval)
		{
			pdebug("missing INTERVAL", DEBUG);
		}
		else
		{
			*(interval - 1) = 0;
			hatena_sc_submit_interval = strtol(interval + 8, NULL, 10);
		}

		return -1;
	}

	pdebug(fmt_vastr("unknown server-reply '%s'", hatena_sc_srv_res), DEBUG);
	return -1;
}

static void hexify(char *pass, int len)
{
	char *bp = hatena_sc_response_hash;
	char hexchars[] = "0123456789abcdef";
	int i;

	memset(hatena_sc_response_hash, 0, sizeof(hatena_sc_response_hash));
	
	for(i = 0; i < len; i++) {
		*(bp++) = hexchars[(pass[i] >> 4) & 0x0f];
		*(bp++) = hexchars[pass[i] & 0x0f];
	}
	*bp = 0;

	return;
}

static int hatena_sc_handshake(void)
{
	int status;
	char buf[4096];
	CURL *curl;

	snprintf(buf, sizeof(buf), "%s/?hs=true&p=%s&c=%s&v=%s&u=%s",
			SCROBBLER_HS_URL, SCROBBLER_VERSION,
			SCROBBLER_CLI_ID, SCROBBLER_IMPLEMENTATION, hatena_sc_username);

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, buf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
			hatena_sc_store_res);
	memset(hatena_sc_curl_errbuf, 0, sizeof(hatena_sc_curl_errbuf));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, hatena_sc_curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, SC_CURL_TIMEOUT);
	status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	hatena_sc_hs_timeout = time(NULL) + SCROBBLER_HS_WAIT;

	if (status) {
		pdebug(hatena_sc_curl_errbuf, DEBUG);
		hatena_sc_hs_errors++;
		hatena_sc_free_res();
		return -1;
	}

	if (hatena_sc_parse_hs_res()) {
		hatena_sc_hs_errors++;
		hatena_sc_free_res();
		return -1;
	}

	if (hatena_sc_challenge_hash != NULL) {
		md5_state_t md5state;
		unsigned char md5pword[16];
		
		md5_init(&md5state);
		/*pdebug(fmt_vastr("Pass Hash: %s", hatena_sc_password), DEBUG);*/
		md5_append(&md5state, (unsigned const char *)hatena_sc_password,
				strlen(hatena_sc_password));
		/*pdebug(fmt_vastr("Challenge Hash: %s", hatena_sc_challenge_hash), DEBUG);*/
		md5_append(&md5state, (unsigned const char *)hatena_sc_challenge_hash,
				strlen(hatena_sc_challenge_hash));
		md5_finish(&md5state, md5pword);
		hexify((char*)md5pword, sizeof(md5pword));
		/*pdebug(fmt_vastr("Response Hash: %s", hatena_sc_response_hash), DEBUG);*/
	}

	hatena_sc_hs_errors = 0;
	hatena_sc_hs_status = 1;

	hatena_sc_free_res();

	pdebug(fmt_vastr("submiturl: %s - interval: %d", 
				hatena_sc_submit_url, hatena_sc_submit_interval), DEBUG);

	return 0;
}

static int hatena_sc_parse_sb_res(void)
{
	char *ch, *ch2;

	if (!hatena_sc_srv_res_size) {
		pdebug("No response from server", DEBUG);
		return -1;
	}
	*(hatena_sc_srv_res + hatena_sc_srv_res_size) = 0;

	if (!strncmp(hatena_sc_srv_res, "OK", 2)) {
		if ((ch = strstr(hatena_sc_srv_res, "INTERVAL"))) {
			hatena_sc_submit_interval = strtol(ch + 8, NULL, 10);
			pdebug(fmt_vastr("got new interval: %d",
						hatena_sc_submit_interval), DEBUG);
		}

		pdebug(fmt_vastr("submission ok: %s", hatena_sc_srv_res), DEBUG);

		return 0;
	}

	if (!strncmp(hatena_sc_srv_res, "BADAUTH", 7)) {
		if ((ch = strstr(hatena_sc_srv_res, "INTERVAL"))) {
			hatena_sc_submit_interval = strtol(ch + 8, NULL, 10);
			pdebug(fmt_vastr("got new interval: %d",
						hatena_sc_submit_interval), DEBUG);
		}

		pdebug("incorrect username/password", DEBUG);

		hatena_sc_giveup = 0;

		/*
		 * We obviously aren't authenticated.  The server might have
		 * lost our handshake status though, so let's try
		 * re-handshaking...  This might not be proper.
		 * (we don't give up)
		 */
		hatena_sc_hs_status = 0;

		if(hatena_sc_challenge_hash != NULL)
			free(hatena_sc_challenge_hash);
		if(hatena_sc_submit_url != NULL)
			free(hatena_sc_submit_url);

		hatena_sc_challenge_hash = hatena_sc_submit_url = NULL;
		hatena_sc_bad_users++;

		if(hatena_sc_bad_users > 2)
		{
			pdebug("3 BADAUTH returns on submission. Halting "
				"submissions until login fixed.", DEBUG)
			hatena_sc_throw_error("Incorrect username/password.\n"
				"Please fix in configuration.");
		}

		return -1;
	}

	if (!strncmp(hatena_sc_srv_res, "FAILED", 6))  {

		/* This could be important. (Such as FAILED - Get new plugin) */
		/*hatena_sc_throw_error(fmt_vastr("%s", hatena_sc_srv_res));*/

		pdebug(hatena_sc_srv_res, DEBUG);

		return -1;
	}

	if (!strncmp(hatena_sc_srv_res, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">", 50)) {
		ch = strstr(hatena_sc_srv_res, "<TITLE>");
		ch2 = strstr(hatena_sc_srv_res, "</TITLE>");
		if (ch != NULL && ch2 != NULL) {
			ch += strlen("<TITLE>");
			*ch2 = '\0';

			pdebug(fmt_vastr("HTTP Error (%d): '%s'",
					 atoi(ch), ch + 4), DEBUG);
//			*ch2 = '<'; // needed? --yaz
		}

		return -1;
	}

	pdebug(fmt_vastr("unknown server-reply %s", hatena_sc_srv_res), DEBUG);

	return -1;
}

static gchar *hatena_sc_itemtag(char c, int n, char *str)
{
    static char buf[SCROBBLER_SB_MAXLEN]; 
    snprintf(buf, SCROBBLER_SB_MAXLEN, "&%c[%d]=%s", c, n, str);
    return buf;
}

#define cfa(f, l, n, v) \
curl_formadd(f, l, CURLFORM_COPYNAME, n, \
		CURLFORM_PTRCONTENTS, v, CURLFORM_END)

static int hatena_sc_generateentry(GString *submission)
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

                g_string_append(submission,hatena_sc_itemtag('a',i,I_ARTIST(item)));
                g_string_append(submission,hatena_sc_itemtag('t',i,I_TITLE(item)));
                g_string_append(submission,hatena_sc_itemtag('l',i,I_LEN(item)));
                g_string_append(submission,hatena_sc_itemtag('i',i,I_TIME(item)));
                g_string_append(submission,hatena_sc_itemtag('m',i,I_MB(item)));
                g_string_append(submission,hatena_sc_itemtag('b',i,I_ALBUM(item)));

		pdebug(fmt_vastr("a[%d]=%s t[%d]=%s l[%d]=%s i[%d]=%s m[%d]=%s b[%d]=%s",
				i, I_ARTIST(item),
				i, I_TITLE(item),
				i, I_LEN(item),
				i, I_TIME(item),
				i, I_MB(item),
				i, I_ALBUM(item)), DEBUG);
#ifdef ALLOW_MULTIPLE
		i++;
	}
#endif

	return i;
}

static int hatena_sc_submitentry(gchar *entry)
{
	CURL *curl;
	/* struct HttpPost *post = NULL , *last = NULL; */
	int status;
        GString *submission;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, hatena_sc_submit_url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			hatena_sc_store_res);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	/*cfa(&post, &last, "debug", "failed");*/

	/*pdebug(fmt_vastr("Username: %s", hatena_sc_username), DEBUG);*/
        submission = g_string_new("u=");
        g_string_append(submission,(gchar *)hatena_sc_username);

	/*pdebug(fmt_vastr("Response Hash: %s", hatena_sc_response_hash), DEBUG);*/
        g_string_append(submission,"&s=");
        g_string_append(submission,(gchar *)hatena_sc_response_hash);

	g_string_append(submission, entry);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)submission->str);
	memset(hatena_sc_curl_errbuf, 0, sizeof(hatena_sc_curl_errbuf));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, hatena_sc_curl_errbuf);
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
		pdebug(hatena_sc_curl_errbuf, DEBUG);
		hatena_sc_sb_errors++;
		hatena_sc_free_res();
		return -1;
	}

	if (hatena_sc_parse_sb_res()) {
		hatena_sc_sb_errors++;
		hatena_sc_free_res();
		pdebug(fmt_vastr("Retrying in %d secs, %d elements in queue",
					hatena_sc_submit_interval, q_len()), DEBUG);
		return -1;
	}
	hatena_sc_free_res();
	return 0;
}

static void hatena_sc_handlequeue(GMutex *mutex)
{
	GString *submitentry;
	int nsubmit;
	int wait;

	if(hatena_sc_submit_timeout < time(NULL) && hatena_sc_bad_users < 3)
	{
		submitentry = g_string_new("");

		g_mutex_lock(mutex);

		nsubmit = hatena_sc_generateentry(submitentry);

		g_mutex_unlock(mutex);

		if (nsubmit > 0)
		{
			pdebug(fmt_vastr("Number submitting: %d", nsubmit), DEBUG);
			pdebug(fmt_vastr("Submission: %s", submitentry->str), DEBUG);

			if(!hatena_sc_submitentry(submitentry->str))
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

				hatena_sc_sb_errors = 0;
			}
			if(hatena_sc_sb_errors)
			{
				if(hatena_sc_sb_errors < 5)
					/* Retry after 1 min */
					wait = 60;
				else
					wait = /* hatena_sc_submit_interval + */
						( ((hatena_sc_sb_errors - 5) < 7) ?
						(60 << (hatena_sc_sb_errors-5)) :
						7200 );
				
				hatena_sc_submit_timeout = time(NULL) + wait;

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
	char buf[PATH_MAX], *cache = NULL, *ptr1, *ptr2;
	int cachesize, written, i = 0;
	item_t *item;

	cachesize = written = 0;

	snprintf(buf, sizeof(buf), "%s/hatenaqueue.txt", audacious_get_localdir());

	if (!(fd = fopen(buf, "r")))
		return;
	pdebug(fmt_vastr("Opening %s", buf), DEBUG);
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

		pdebug("Pushed:", DEBUG);
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
		pdebug(fmt_vastr("a[%d]=%s t[%d]=%s l[%d]=%s i[%d]=%s m[%d]=%s b[%d]=%s",
				i, I_ARTIST(item),
				i, I_TITLE(item),
				i, I_LEN(item),
				i, I_TIME(item),
				i, I_MB(item),
				i, I_ALBUM(item)), DEBUG);
		free(artist);
		free(title);
		free(len);
		free(time);
		free(album);
		free(mb);

		i++;
	}
	pdebug("Done loading cache.", DEBUG);
	free(cache);
}

static void dump_queue(void)
{
	FILE *fd;
	item_t *item;
	char *home, buf[PATH_MAX];

	/*pdebug("Entering dump_queue();", DEBUG);*/

	if (!(home = getenv("HOME")))
	{
		pdebug("No HOME directory found. Cannot dump queue.", DEBUG);
		return;
	}

	snprintf(buf, sizeof(buf), "%s/hatenaqueue.txt", audacious_get_localdir());

	if (!(fd = fopen(buf, "w")))
	{
		pdebug(fmt_vastr("Failure opening %s", buf), DEBUG);
		return;
	}

	pdebug(fmt_vastr("Opening %s", buf), DEBUG);

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

void hatena_sc_cleaner(void)
{
	if(hatena_sc_submit_url != NULL)
		free(hatena_sc_submit_url);
	if(hatena_sc_username != NULL)
		free(hatena_sc_username);
	if(hatena_sc_password != NULL)
		free(hatena_sc_password);
	if(hatena_sc_challenge_hash != NULL)
		free(hatena_sc_challenge_hash);
	if(hatena_sc_srv_res != NULL)
		free(hatena_sc_srv_res);
	if(hatena_sc_major_error != NULL)
		free(hatena_sc_major_error);
	dump_queue();
	q_free();
	pdebug("scrobbler shutting down", DEBUG);
}

static void hatena_sc_checkhandshake(void)
{
	int wait;

	if (!hatena_sc_username || !hatena_sc_password)
		return;

	if (hatena_sc_hs_status)
		return;
	if (hatena_sc_hs_timeout < time(NULL))
	{
		hatena_sc_handshake();

		if(hatena_sc_hs_errors)
		{
			if(hatena_sc_hs_errors < 5)
				/* Retry after 60 seconds */
				wait = 60;
			else
				wait = /* hatena_sc_submit_interval + */
					( ((hatena_sc_hs_errors - 5) < 7) ?
					(60 << (hatena_sc_hs_errors-5)) :
					7200 );
			hatena_sc_hs_timeout = time(NULL) + wait;
			pdebug(fmt_vastr("Error while handshaking. Retrying "
				"after %d seconds.", wait), DEBUG);
		}
	}
}

/**** Public *****/

/* Called at session startup*/

void hatena_sc_init(char *uname, char *pwd)
{
	hatena_sc_hs_status = hatena_sc_hs_timeout = hatena_sc_hs_errors = hatena_sc_submit_timeout =
		hatena_sc_srv_res_size = hatena_sc_giveup = hatena_sc_major_error_present =
		hatena_sc_bad_users = hatena_sc_sb_errors = 0;
	hatena_sc_submit_interval = 100;

	hatena_sc_submit_url = hatena_sc_username = hatena_sc_password = hatena_sc_srv_res =
		hatena_sc_challenge_hash = hatena_sc_major_error = NULL;
	hatena_sc_username = strdup(uname);
	hatena_sc_password = strdup(pwd);
	read_cache();
	pdebug("scrobbler starting up", DEBUG);
}

void hatena_sc_addentry(GMutex *mutex, TitleInput *tuple, int len)
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
int hatena_sc_idle(GMutex *mutex)
{
	hatena_sc_checkhandshake();
	if (hatena_sc_hs_status)
		hatena_sc_handlequeue(mutex);

	return hatena_sc_giveup;
}
