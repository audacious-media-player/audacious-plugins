
//external includes
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <curl/curl.h>

#include <libaudcore/audstrings.h>

//plugin includes
#include "scrobbler.h"

typedef struct {
    gchar *paramName;
    gchar *argument;
} API_Parameter;

static CURL *curlHandle = NULL;     //global handle holding cURL options

bool_t scrobbling_enabled = TRUE;

//shared variables
gchar *received_data = NULL;   //Holds the result of the last request made to last.fm
size_t received_data_size = 0; //Holds the size of the received_data buffer



// The cURL callback function to store the received data from the last.fm servers.
static size_t result_callback (void *buffer, size_t size, size_t nmemb, void *userp) {

    const size_t len = size*nmemb;

    gchar *temp_data = realloc(received_data, received_data_size + len + 1);

    if (temp_data == NULL) {
      return 0;
    } else {
      received_data = temp_data;
    }

    memcpy(received_data + received_data_size, buffer, len);

    received_data_size += len;

    return len;
}

static int scrobbler_compare_API_Parameters(const void *a, const void *b) {
    return g_strcmp0(((const API_Parameter *) a)->paramName, ((const API_Parameter *) b)->paramName);
}

static char *scrobbler_get_signature(int nparams, API_Parameter *parameters) {
    qsort(parameters, nparams, sizeof(API_Parameter), scrobbler_compare_API_Parameters);

    char *all_params = NULL;
    gchar *result = NULL;
    gchar *api_sig = NULL;
    size_t api_sig_length = strlen(SCROBBLER_SHARED_SECRET);

    for (gint i = 0; i < nparams; i++) {
        api_sig_length += strlen(parameters[i].paramName) + strlen(parameters[i].argument);
    }
    all_params = g_new0(gchar, api_sig_length);

    for (int i = 0; i < nparams; i++) {
        strcat(all_params, parameters[i].paramName);
        strcat(all_params, parameters[i].argument);
    }

    api_sig = g_strconcat(all_params, SCROBBLER_SHARED_SECRET, NULL);
    g_free(all_params);


    result = g_compute_checksum_for_string(G_CHECKSUM_MD5, api_sig, -1);
    g_free(api_sig);
    return result;
}




/*
 * n_args should count with the given authentication parameters
 * At most 2: api_key, session_key.
 * api_sig (checksum) is always included, be it necessary or not
 * Example usage:
 *   create_message_to_lastfm("track.scrobble", 5
 *        "artist", "Artist Name", "track", "Track Name", "timestamp", time(NULL),
 *        "api_key", SCROBBLER_API_KEY, "sk", session_key);
 *
 * Returns NULL if an error occurrs
 */
static gchar *create_message_to_lastfm (char *method_name, int n_args, ...) {
    //TODO: Improve this f-ugly mess.
    gchar *result = NULL;

    //parameters to be sent to the get_signature() function
    API_Parameter signable_params[n_args+1];
    signable_params[0].paramName = g_strdup("method");
    signable_params[0].argument  = g_strdup(method_name);

    size_t msg_size = 2; // First '=' and final '\0'
    msg_size = msg_size + strlen("method") + strlen(method_name);

    va_list vl;
    va_start(vl, n_args);
    for (int i = 0; i < n_args; i++) {

        signable_params[i+1].paramName = g_strdup(va_arg(vl, gchar *));
        signable_params[i+1].argument  = g_strdup(va_arg(vl, gchar *));

        msg_size += strlen( signable_params[i+1].paramName );
        msg_size += strlen( signable_params[i+1].argument );
        msg_size += 2; //Counting '&' and '='
    }
    va_end(vl);

    gchar *aux;
    result = g_strconcat("method=", method_name, NULL);
    char *escaped_argument;

    for (int i = 0; i < n_args; i++) {
        escaped_argument = curl_easy_escape(curlHandle, signable_params[i+1].argument, 0);

        aux = g_strdup_printf("%s&%s=%s", result, signable_params[i+1].paramName, escaped_argument);
        g_free(result);
        curl_free(escaped_argument);
        result = aux;
    }

    gchar *api_sig = scrobbler_get_signature(n_args+1, signable_params);

    aux = g_strdup_printf("%s&api_sig=%s", result, api_sig);
    result = aux;

    AUDDBG("FINAL message: %s.\n", result);
    g_free(api_sig);
    for (int i = 0; i < n_args+1; i++) {
        g_free(signable_params[i].paramName);
        g_free(signable_params[i].argument);
    }

    return result;
}


static bool_t send_message_to_lastfm(gchar *data) {
    AUDDBG("This message will be sent to last.fm:\n%s\n%%%%End of message%%%%\n", data);//Enter?\n", data);
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, data);
    CURLcode curl_requests_result = curl_easy_perform(curlHandle);

    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not communicate with last.fm: %s.\n", curl_easy_strerror(curl_requests_result));
        return FALSE;
    }

    return TRUE;
}


//returns:
// FALSE if there was a network problem
// TRUE otherwise (request_token must be checked)
static bool_t scrobbler_request_token() {
    gchar *tokenmsg = create_message_to_lastfm("auth.getToken",
                                              1,
                                              "api_key", SCROBBLER_API_KEY
                                             );

    if (send_message_to_lastfm(tokenmsg) == FALSE) {
        AUDDBG("Could not send token request to last.fm.\n");
        g_free(tokenmsg);
        return FALSE;
    }

    bool_t success = TRUE;
    gchar *error_code = NULL;
    gchar *error_detail = NULL;

    if (read_token(&error_code, &error_detail) == FALSE) {
        success = FALSE;
        if (error_code != NULL && g_strcmp0(error_code, "8")) {
            //error code 8: There was an error granting the request token. Please try again later
            str_unref(request_token);
            request_token = NULL;
        }
    }

    str_unref(error_code);
    str_unref(error_detail);
    return success;
}


static bool_t update_session_key() {
    bool_t result = TRUE;
    gchar *error_code = NULL;
    gchar *error_detail = NULL;

    if (read_session_key(&error_code, &error_detail) == FALSE) {
        if (error_code != NULL && (
                g_strcmp0(error_code,  "4") == 0 || //invalid token
                g_strcmp0(error_code, "14") == 0 || //token not authorized
                g_strcmp0(error_code, "15") == 0    //token expired
            )) {
            AUDDBG("error code CAUGHT: %s\n", error_code);
            str_unref(session_key);
            session_key = NULL;
            result = TRUE;
        } else {
            result= FALSE;
        }
    }

    aud_set_str("scrobbler", "session_key", session_key ? session_key : "");

    str_unref(error_code);
    str_unref(error_detail);
    return result;
}

//returns:
// FALSE if there was a network problem
// TRUE otherwise (session_key must be checked)
static bool_t scrobbler_request_session() {

    gchar *sessionmsg = create_message_to_lastfm("auth.getSession",
                                                 2,
                                                 "token", request_token,
                                                 "api_key", SCROBBLER_API_KEY);

    if (send_message_to_lastfm(sessionmsg) == FALSE) {
        g_free(sessionmsg);
        return FALSE;
    }

    g_free(sessionmsg);
    //the token can only be sent once
    str_unref(request_token);
    request_token = NULL;

    return update_session_key();
}



//returns:
// FALSE if there was a network problem.
// TRUE otherwise (scrobbling_enabled must be checked)
//sets scrobbling_enabled to TRUE if the session is OK
//sets session_key to NULL if it is invalid
static bool_t scrobbler_test_connection() {

    if (!session_key || !session_key[0]) {
        scrobbling_enabled = FALSE;
        return TRUE;
    }


    gchar *testmsg = create_message_to_lastfm("user.getRecommendedArtists",
                                              3,
                                              "limit", "1",
                                              "api_key", SCROBBLER_API_KEY,
                                              "sk", session_key
                                             );
    bool_t success = send_message_to_lastfm(testmsg);
    g_free(testmsg);
    if (success == FALSE) {
        AUDDBG("Network problems. Will not scrobble any tracks.\n");
        scrobbling_enabled = FALSE;
        if (permission_check_requested) {
            perm_result = PERMISSION_NONET;
        }
        return FALSE;
    }

    gchar *error_code = NULL;
    gchar *error_detail = NULL;

    if (read_authentication_test_result(&error_code, &error_detail) == FALSE) {
        AUDDBG("Error code: %s. Detail: %s.\n", error_code, error_detail);
        if (error_code != NULL && (
                g_strcmp0(error_code, "4") == 0 || //error code 4: Authentication Failed - You do not have permissions to access the service
                g_strcmp0(error_code, "9") == 0    //error code 9: Invalid session key - Please re-authenticate
            )) {
            str_unref(session_key);
            session_key = NULL;
            aud_set_str("scrobbler", "session_key", "");
            scrobbling_enabled = FALSE;
        } else {
            //network problem.
            scrobbling_enabled = FALSE;
            AUDDBG("Connection NOT OK. Scrobbling disabled\n");
            success = FALSE;
        }
    } else {
        //THIS IS THE ONLY PLACE WHERE SCROBBLING IS SET TO ENABLED IN RUN-TIME
        scrobbling_enabled = TRUE;
        AUDDBG("Connection OK. Scrobbling enabled.\n");
    }

    str_unref(error_code);
    str_unref(error_detail);
    return success;
}

//called from scrobbler_init() @ scrobbler.c
bool_t scrobbler_communication_init() {
    CURLcode curl_requests_result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not initialize libCURL: %s.\n", curl_easy_strerror(curl_requests_result));
        return FALSE;
    }

    curlHandle = curl_easy_init();
    if (curlHandle == NULL) {
        AUDDBG("Could not initialize libCURL.\n");
        return FALSE;
    }

    curl_requests_result = curl_easy_setopt(curlHandle, CURLOPT_URL, SCROBBLER_URL);
    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not define scrobbler destination URL: %s.\n", curl_easy_strerror(curl_requests_result));
        return FALSE;
    }

    curl_requests_result = curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, result_callback);
    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not register scrobbler callback function: %s.\n", curl_easy_strerror(curl_requests_result));
        return FALSE;
    }

    return TRUE;
}

static void set_timestamp_to_current(gchar **line) {
    //line[0] line[1] line[2] line[3] line[4] line[5] line[6] line[7]   line[8]
    //artist  album   title   number  length  "L"     timestamp mbid    NULL

    gchar **splitted_line = g_strsplit(*line, "\t", 0);
    g_free(splitted_line[6]);
    splitted_line[6] = g_strdup_printf("%"G_GINT64_FORMAT"", g_get_real_time() / G_USEC_PER_SEC);
    AUDDBG("splitted line's timestamp is now: %s.\n", splitted_line[6]);
    g_free(*line);
    (*line) = g_strjoinv("\t", splitted_line);
}

static void delete_lines_from_scrobble_log (GSList **lines_to_remove_ptr, GSList **lines_to_retry_ptr, gchar *queuepath) {
    GSList *lines_to_remove = *lines_to_remove_ptr;
    GSList *lines_to_retry = *lines_to_retry_ptr;
    gchar *contents = NULL;
    gchar **lines = NULL;
    gchar **finallines = g_malloc_n(1, sizeof(gchar *));
    int n_finallines;

    if (lines_to_remove != NULL) {
        lines_to_remove = g_slist_reverse(lines_to_remove);
    }
    if (lines_to_retry != NULL) {
        lines_to_retry = g_slist_reverse(lines_to_retry);
    }


    pthread_mutex_lock(&log_access_mutex);

    gboolean success = g_file_get_contents(queuepath, &contents, NULL, NULL);
    if (!success) {
        AUDDBG("Could not read scrobbler.log contents.\n");
    } else {
        lines = g_strsplit(contents, "\n", 0);

        n_finallines = 0;
        for (int i = 0 ; lines[i] != NULL && strlen(lines[i]) > 0; i++) {
            if (lines_to_remove != NULL && *((int *) (lines_to_remove->data)) == i) {
                //this line is to remove
                lines_to_remove = g_slist_next(lines_to_remove);
            } else {
                //keep this line
                AUDDBG("Going to keep line %i\n", i);
                if (lines_to_retry != NULL && *((int *) (lines_to_retry->data)) == i) {
                  lines_to_retry = g_slist_next(lines_to_retry);
                  //this line will be retried with a zero timestamp
                  AUDDBG("Going to zero this line.\n");
                  AUDDBG("Line before: %s.\n", lines[i]);
                  set_timestamp_to_current(&(lines[i]));
                  AUDDBG("Line after: %s.\n", lines[i]);

                } else {
                  AUDDBG("not zeroing this line\n");
                }
                n_finallines++;
                finallines = g_realloc_n(finallines, n_finallines, sizeof(gchar *));
                finallines[n_finallines-1] = g_strdup(lines[i]);
            }
        }

        finallines = g_realloc_n(finallines, n_finallines+2, sizeof(gchar *));
        finallines[n_finallines] = g_strdup("");
        finallines[n_finallines+1] = NULL;
        g_free(contents);
        contents = g_strjoinv("\n", finallines);
        success = g_file_set_contents(queuepath, contents, -1, NULL);
        if (!success) {
            AUDDBG("Could not write to scrobbler.log!\n");
        }

    }

    pthread_mutex_unlock(&log_access_mutex);


    g_strfreev(finallines);
    g_strfreev(lines);
    g_free(contents);
}

static void save_line_to_remove(GSList **lines_to_remove, int linenumber) {
    int *rem = g_malloc(sizeof(int));
    *rem = linenumber;
    (*lines_to_remove) = g_slist_prepend((*lines_to_remove), rem);
}

static void scrobble_cached_queue() {

    gchar *queuepath = g_build_filename(aud_get_path(AUD_PATH_USER_DIR),"scrobbler.log", NULL);
    gchar *contents = NULL;
    gboolean success;
    gchar **lines = NULL;
    gchar **line;
    gchar *scrobblemsg;
    GSList *lines_to_remove = NULL; //lines to remove because they were scrobbled (or ignored, and will not be retried)
    GSList *lines_to_retry = NULL; //lines to retry later because they were too old TODO: or "daily scrobble limit reached"

    pthread_mutex_lock(&log_access_mutex);
    success = g_file_get_contents(queuepath, &contents, NULL, NULL);
    pthread_mutex_unlock(&log_access_mutex);
    if (!success) {
        AUDDBG("Couldn't access the queue file.\n");
    } else {

        lines = g_strsplit(contents, "\n", 0);

        for (int i = 0; lines[i] != NULL && strlen(lines[i]) > 0 && scrobbling_enabled; i++) {
            line = g_strsplit(lines[i], "\t", 0);

	    //line[0] line[1] line[2] line[3] line[4] line[5] line[6] line[7]   line[8]
	    //artist  album   title   number  length  "L"     timestamp mbid    NULL

            if (line[0] && line[2] && (strcmp(line[5], "L") == 0) && line[6] && (line[8] == NULL)) {
                scrobblemsg = create_message_to_lastfm("track.scrobble",
                                                       9,
                                                       "artist", line[0],
                                                       "album", line[1],
                                                       "track", line[2],
                                                       "trackNumber", line[3],
                                                       "duration", line[4],
                                                       "timestamp", line[6],
						       "mbid", line[7],
                                                       "api_key", SCROBBLER_API_KEY,
                                                       "sk", session_key);
                if (send_message_to_lastfm(scrobblemsg) == TRUE) {
                    char *error_code = NULL;
                    char *error_detail = NULL;
                    bool_t ignored = FALSE;
                    char *ignored_code = NULL;

                    if (read_scrobble_result(&error_code, &error_detail, &ignored, &ignored_code) == TRUE) {
                        AUDDBG("SCROBBLE OK. Error code: %s. Error detail: %s\n", error_code, error_detail);
                        AUDDBG("SCROBBLE OK. ignored: %i.\n", ignored);
                        AUDDBG("SCROBBLE OK. ignored code: %s.\n", ignored_code);
                        if (ignored == TRUE && g_strcmp0(ignored_code, "3") == 0) { //3: Timestamp was too old
                            AUDDBG("SCROBBLE IGNORED!!! %i, detail: %s\n", ignored, ignored_code);
                            save_line_to_remove(&lines_to_retry, i);
                        } else if (ignored == TRUE && g_strcmp0(ignored_code, "") == 0) { //5: Daily scrobble limit reached
                           //TODO: a track might not be scrobbled due to "daily scrobble limit exeeded".
                           //This message comes on the ignoredMessage attribute, inside the XML of the response.
                           //We are not dealing with this case currently and are losing that scrobble.
                           //TODO

                        } else {
                            AUDDBG("Not ignored. Carrying on...\n");
                            save_line_to_remove(&lines_to_remove, i);
                        }
                    } else {
                        AUDDBG("SCROBBLE NOT OK. Error code: %s. Error detail: %s.\n", error_code, error_detail);

                        if (error_code == NULL) { //net error(?) or the answer from last.fm was not well read
                            //scrobble to be retried
                        }
                        else if (g_strcmp0(error_code, "11") == 0 ||
                                 g_strcmp0(error_code, "16") == 0){
                            //error code 11: Service Offline - This service is temporarily offline. Try again later.
                            //error code 16: The service is temporarily unavailable, please try again.
                            //scrobble to be retried
                        }
                        else if (g_strcmp0(error_code,  "9") == 0) {
                            //Bad Session. Reauth.
                            scrobbling_enabled = FALSE;
                            str_unref(session_key);
                            session_key = NULL;
                            aud_set_str("scrobbler", "session_key", "");
                        }
                        else {
                            save_line_to_remove(&lines_to_remove, i);
                        }
                    }

                    str_unref(error_code);
                    str_unref(error_detail);
                    str_unref(ignored_code);
                } else {
                    AUDDBG("Could not scrobble a track on the queue. Network problem?\n");
                    //scrobble to be retried
                    scrobbling_enabled = FALSE;
                }

                g_free(scrobblemsg);
                //TODO: Avoid spamming last.fm --- It's not as simple as just adding a wait here:
                //If audacious is closed while we're on this loop, tracks might be re-scrobbled.
                //This shall be doable if we only pick one track at a time from the scrobbler.log
            } else {
                AUDDBG("Unscrobbable line.\n");
                //leave entry on the cache file
            }
            g_strfreev(line);
        }//for


        delete_lines_from_scrobble_log(&lines_to_remove, &lines_to_retry, queuepath);

        if (lines_to_remove != NULL) {
            g_slist_free_full(lines_to_remove, g_free);
        }
        if (lines_to_retry != NULL) {
            g_slist_free_full(lines_to_retry, g_free);
        }

        g_strfreev(lines);
    }

    g_free(contents);
    g_free(queuepath);
}


static void send_now_playing() {

  gchar  *error_code = NULL;
  gchar  *error_detail = NULL;
  bool_t ignored = FALSE;
  gchar  *ignored_code = NULL;
  /*
   * now_playing_track can be set to something else while we this method is
   * running. Creating a local variable avoids to get data for different tracks,
   * while now_playing_track was updated concurrently.
   */
  Tuple *curr_track = now_playing_track;

  char *artist = clean_string(tuple_get_str(curr_track, FIELD_ARTIST));
  char *title  = clean_string(tuple_get_str(curr_track, FIELD_TITLE));
  char *album  = clean_string(tuple_get_str(curr_track, FIELD_ALBUM));
  char *mbid = clean_string(tuple_get_str(curr_track, FIELD_MBID));

  int track  = tuple_get_int(curr_track, FIELD_TRACK_NUMBER);
  int length = tuple_get_int(curr_track, FIELD_LENGTH);

  tuple_unref(curr_track);

  if (artist[0] && title[0] && length > 0) {
    char *track_str = (track > 0) ? int_to_str(track) : str_get("");
    char *length_str = int_to_str(length / 1000);

    gchar *playingmsg = create_message_to_lastfm("track.updateNowPlaying",
                                            8,
                                           "artist", artist,
                                           "album", album,
                                           "track", title,
					   "mbid", mbid,
                                           "trackNumber", track_str,
                                           "duration", length_str,
                                           "api_key", SCROBBLER_API_KEY,
                                           "sk", session_key);

    bool_t success = send_message_to_lastfm(playingmsg);
    g_free(playingmsg);

    str_unref(track_str);
    str_unref(length_str);

    if (success == FALSE) {
      AUDDBG("Network problems. Could not send \"now playing\" to last.fm\n");
      scrobbling_enabled = FALSE;
    } else if (read_scrobble_result(&error_code, &error_detail, &ignored, &ignored_code) == TRUE) {
      //see scrobble_cached_queue()
      AUDDBG("NOW PLAYING OK.\n");
    } else {
      AUDDBG("NOW PLAYING NOT OK. Error code: %s. Error detail: %s.\n", error_code, error_detail);
      //From the API: Now Playing requests that fail should not be retried.

      if (g_strcmp0(error_code, "9") == 0) {
        //Bad Session. Reauth.
        //We don't really care about any other errors.
        scrobbling_enabled = FALSE;
        str_unref(session_key);
        session_key = NULL;
        aud_set_str("scrobbler", "session_key", "");
      }

    }
    //We don't care if the now playing was not accepted, no need to read the result from the server.

  }

  str_unref(artist);
  str_unref(title);
  str_unref(album);
  str_unref(mbid);

  str_unref(error_code);
  str_unref(error_detail);
  str_unref(ignored_code);
}

static void treat_permission_check_request() {
    if (!session_key || !session_key[0]) {
        perm_result = PERMISSION_DENIED;

        if (!request_token || !request_token[0]) {
            if (scrobbler_request_token() == FALSE || !request_token || !request_token[0]) {
                perm_result = PERMISSION_NONET;
            } //else PERMISSION_DENIED

        } else if (scrobbler_request_session() == FALSE) {
            perm_result = PERMISSION_NONET;

        } else if (!session_key || !session_key[0]) {
            //This means we had a token, a session was requested now,
            //but the token was not accepted or expired.
            //Ask for a new token now.
            if (scrobbler_request_token() == FALSE || !request_token || !request_token[0]) {
                perm_result = PERMISSION_NONET;
            } //else PERMISSION_DENIED
        }
    }
    if (session_key && session_key[0]) {
        if (scrobbler_test_connection() == FALSE) {
            perm_result = PERMISSION_NONET;

            if (!session_key || !session_key[0]) {
                if (scrobbler_request_token() != FALSE && request_token && request_token[0]) {
                    perm_result = PERMISSION_DENIED;
                } //else PERMISSION_NONET
            }

        } else {
            if (scrobbling_enabled) {
                perm_result = PERMISSION_ALLOWED;
            } else {
             /* This means that we have a session key but couldn't make
              * an authenticated call with it. This happens when:
              * a) the user revoked the permission to Audacious on his
              * last.fm account OR
              * b) something might be wrong on last.fm's side (?) OR
              * c) the user fiddled with the audacious config file and
              * the key is now invalid
              */
                if (scrobbler_request_token() != FALSE && request_token && request_token[0]) {
                    perm_result = PERMISSION_DENIED;
                } else {
                    perm_result = PERMISSION_NONET;
                }
            }
        }
    } //session_key == NULL || strlen(session_key) == 0
}

// This is a sister function of scrobbler_request_session, using the getMobileSession
//API call, for migrating from the old config
//returns:
// FALSE if there was a network problem OR a session_key was not obtained
// TRUE if a new session_key was obtained
static bool_t treat_migrate_config() {

    char *password = aud_get_str("audioscrobbler","password");
    if (!password[0]) {
        str_unref(password);
        return FALSE;
    }

    char *username = aud_get_str("audioscrobbler","username");
    if (!username[0]) {
        str_unref(password);
        str_unref(username);
        return FALSE;
    }

    gchar *checksumThis = g_strdup_printf("%s%s", username, password);
    gchar *authToken = g_compute_checksum_for_string(G_CHECKSUM_MD5, checksumThis, -1);

    gchar *sessionmsg = create_message_to_lastfm("auth.getMobileSession",
                                                 3,
                                                 "authToken", authToken,
                                                 "username", username,
                                                 "api_key", SCROBBLER_API_KEY);
    str_unref(username);
    str_unref(password);
    g_free(checksumThis);
    g_free(authToken);

    if (send_message_to_lastfm(sessionmsg) == FALSE) {
        g_free(sessionmsg);
        return FALSE;
    }

    g_free(sessionmsg);

    if (!update_session_key())
        return FALSE;

    return (session_key && session_key[0]);
}


//Scrobbling will only be enabled after the first connection test passed
gpointer scrobbling_thread (gpointer input_data) {

    while (scrobbler_running) {

        if (migrate_config_requested) {
          if (treat_migrate_config() == FALSE) {
            aud_interface_show_error(_("Audacious is now using an improved version of the Last.fm Scrobbler.\nPlease check the Preferences for the Scrobbler plugin."));
          }
          aud_set_str("scrobbler", "migrated", "true");
          migrate_config_requested = FALSE;

        } else if (permission_check_requested) {
            treat_permission_check_request();
            permission_check_requested = FALSE;

        } else if (invalidate_session_requested) {
            str_unref(session_key);
            session_key = NULL;
            aud_set_str("scrobbler", "session_key", "");
            invalidate_session_requested = FALSE;

        } else if (now_playing_requested) {
            if (scrobbling_enabled) {
              send_now_playing();
            }
            now_playing_requested = FALSE;

        } else {
            if (scrobbling_enabled) {
              scrobble_cached_queue();
            }
            //scrobbling may be disabled at this point if communication errors occur

            pthread_mutex_lock(&communication_mutex);
            if (scrobbling_enabled) {
                pthread_cond_wait(&communication_signal, &communication_mutex);
                pthread_mutex_unlock(&communication_mutex);
            }
            else {
                //We don't want to wait until receiving a signal to retry
                //if submitting the cache failed due to network problems
                pthread_mutex_unlock(&communication_mutex);

                if (scrobbler_test_connection() == FALSE || !scrobbling_enabled) {
                    struct timeval curtime;
                    struct timespec timeout;
                    pthread_mutex_lock(&communication_mutex);
                    gettimeofday(&curtime, NULL);
                    timeout.tv_sec = curtime.tv_sec + 7;
                    timeout.tv_nsec = curtime.tv_usec * 1000;
                    pthread_cond_timedwait(&communication_signal, &communication_mutex, &timeout);
                    pthread_mutex_unlock(&communication_mutex);
                }
            }
        }
    }//while(scrobbler_running)

    //reset all vars to their initial values
    g_free(received_data);
    received_data = NULL;
    received_data_size = 0;

    curl_easy_cleanup(curlHandle);
    curlHandle = NULL;

    scrobbling_enabled = TRUE;
    return NULL;
}

