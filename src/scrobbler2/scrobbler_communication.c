/*
 * TODO: headers
 * scrobbler_communication.c
 *
 *  Created on: 10 Jun 2012
 *      Author: luis
 */


//external includes
#include <stdarg.h>
#include <stdlib.h>
#include <curl/curl.h>

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

    received_data = realloc(received_data, received_data_size + len + 1);

    memcpy(received_data + received_data_size, buffer, len);

    received_data_size += len;

    return len;
}

static int scrobbler_compare_API_Parameters(const void *a, const void *b) {
    return g_strcmp0(((const API_Parameter *) a)->paramName, ((const API_Parameter *) b)->paramName);
}

static char *scrobbler_get_signature(int nparams, API_Parameter *parameters) {
    qsort(parameters, nparams, sizeof(API_Parameter), scrobbler_compare_API_Parameters);

    gchar *result = NULL;
    gchar *api_sig = NULL;
    size_t api_sig_length = strlen(SCROBBLER_SHARED_SECRET);

    for (gint i = 0; i < nparams; i++) {
        api_sig_length += strlen(parameters[i].paramName) + strlen(parameters[i].argument);
    }
    result = calloc(api_sig_length, sizeof(gchar));

    for (int i = 0; i < nparams; i++) {
        strcat(result, parameters[i].paramName);
        strcat(result, parameters[i].argument);
    }

    api_sig = g_strconcat(result, SCROBBLER_SHARED_SECRET, NULL);
    g_free(result);


    result = g_compute_checksum_for_string(G_CHECKSUM_MD5, api_sig, -1);
    g_free(api_sig);
    return result;
}




/*
 * n_args should count with the given authentication parameters
 * At most 2: api_key, session_key.
 * api_sig (checksum) is always included, be it necessary or not
 * Example usage:
 *   send_message_to_lastfm("track.scrobble", 5
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
    msg_size += strlen("method") + strlen(method_name);

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

    result = malloc(msg_size);
    if (result == NULL) {
        perror("malloc") ;
        return NULL;
    }

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
    printf("This message will be sent to last.fm:\n%s\n%%%%End of message%%%%\n", data);//Enter?\n", data);
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
        printf("Could not send token request to last.fm.\n");
        return FALSE;
    }

    gchar *error_code = NULL;
    gchar *error_detail = NULL;
    if (read_token(&error_code, &error_detail) == FALSE) {
        if (error_code != NULL && g_strcmp0(error_code, "8")) {
            //error code 8: There was an error granting the request token. Please try again later
            request_token = NULL;
            return FALSE;
        }
        return FALSE;
    }

    return TRUE;
}

//returns:
// FALSE if there was a network problem
// TRUE otherwise (session_key must be checked)
static bool_t scrobbler_request_session() {
    bool_t result = FALSE;
    gchar *error_code = NULL;
    gchar *error_detail = NULL;

    gchar *sessionmsg = create_message_to_lastfm("auth.getSession",
                                                 2,
                                                 "token", request_token,
                                                 "api_key", SCROBBLER_API_KEY);

    if (send_message_to_lastfm(sessionmsg) == FALSE) {
        g_free(sessionmsg);
        return FALSE;
    }

    //the token can only be sent once
    if (request_token != NULL) {
        g_free(request_token);
    }
    request_token = NULL;

    if (read_session_key(&error_code, &error_detail) == FALSE) {
        if (error_code != NULL && (
                g_strcmp0(error_code,  "4") == 0 || //invalid token
                g_strcmp0(error_code, "14") == 0 || //token not authorized
                g_strcmp0(error_code, "15") == 0    //token expired
            )) {
            printf("error code CAUGHT: %s\n", error_code);
            g_free(session_key);
            session_key = NULL;
            result = TRUE;
        } else {
            result= FALSE;
        }
    }

    if (session_key == NULL) {
        aud_set_string("scrobbler", "session_key", "");
    } else {
        aud_set_string("scrobbler", "session_key", session_key);
    }
    return result;
}



//returns:
// FALSE if there was a network problem.
// TRUE otherwise (scrobbling_enabled must be checked)
//sets scrobbling_enabled to TRUE if the session is OK
//sets session_key to NULL if it is invalid
static bool_t scrobbler_test_connection() {

    if (session_key == NULL || strlen(session_key) == 0) {
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
        printf("Network problems. Will not scrobble any tracks.\n");
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
            g_free(error_code);
            g_free(error_detail);
            g_free(session_key);
            session_key = NULL;
            aud_set_string("scrobbler", "session_key", "");
            scrobbling_enabled = FALSE;
            return TRUE;
        } else {
            //network problem.
            scrobbling_enabled = FALSE;
            printf("Connection NOT OK. Scrobbling disabled\n");
            return FALSE;
        }
    } else {
        //THIS IS THE ONLY PLACE WHERE SCROBBLING IS SET TO ENABLED
        scrobbling_enabled = TRUE;
        printf("Connection OK. Scrobbling enabled.\n");
        return TRUE;
    }
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


static void delete_lines_from_scrobble_log (GSList **lines_to_remove_ptr, gchar *queuepath) {
    GSList *lines_to_remove = *lines_to_remove_ptr;
    gchar *contents = NULL;
    gchar **lines = NULL;
    gchar **finallines = g_malloc_n(1, sizeof(gchar *));
    int n_finallines;

    if (lines_to_remove == NULL) {
        return;
    }
    lines_to_remove = g_slist_reverse(lines_to_remove);


    g_mutex_lock(&log_access_mutex);

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

    g_mutex_unlock(&log_access_mutex);


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
    GSList *lines_to_remove = NULL;
    gchar *error_code = NULL;
    gchar *error_detail = NULL;
    g_mutex_lock(&log_access_mutex);
    success = g_file_get_contents(queuepath, &contents, NULL, NULL);
    g_mutex_unlock(&log_access_mutex);
    if (!success) {
        AUDDBG("Couldn't access the queue file.\n");
    } else {

        lines = g_strsplit(contents, "\n", 0);

        for (int i = 0; lines[i] != NULL && strlen(lines[i]) > 0 && scrobbling_enabled; i++) {
            line = g_strsplit(lines[i], "\t", 0);

            //line[0] line[1] line[2] line[3] line[4] line[5] line[6]   line[7]
            //artist  album   title   number  length  "L"     timestamp NULL

            if (line[0] && line[2] && (strcmp(line[5], "L") == 0) && line[6] && (line[7] == NULL)) {
                scrobblemsg = create_message_to_lastfm("track.scrobble",
                                                       8,
                                                       "artist", line[0],
                                                       "album", line[1],
                                                       "track", line[2],
                                                       "trackNumber", line[3],
                                                       "duration", line[4],
                                                       "timestamp", line[6],
                                                       "api_key", SCROBBLER_API_KEY,
                                                       "sk", session_key);
                if (send_message_to_lastfm(scrobblemsg) == TRUE) {
                    error_code = NULL;
                    error_detail = NULL;
                    if (read_scrobble_result(&error_code, &error_detail) == TRUE) {
                        //TODO: a track might not be scrobbled due to "daily scrobble limit exeeded".
                        //This message comes on the ignoredMessage attribute, inside the XML of the response.
                        //We are not dealing with this case currently and are losing that scrobble.
                        AUDDBG("SCROBBLE OK.\n");
                        save_line_to_remove(&lines_to_remove, i);
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
                            g_free(session_key);
                            session_key = NULL;
                            aud_set_string("scrobbler", "session_key", "");
                        }
                        else {
                            save_line_to_remove(&lines_to_remove, i);
                        }
                    }

                    g_free(error_code);
                    g_free(error_detail);

                } else {
                    AUDDBG("Could not scrobble a track on the queue. Network problem?\n");
                    //scrobble to be retried
                    scrobbling_enabled = FALSE;
                }

                g_free(scrobblemsg);
            } else {
                AUDDBG("Unscrobbable line.\n");
                //leave entry on the cache file
            }
            g_strfreev(line);
        }//for

        if (lines_to_remove != NULL) {
            delete_lines_from_scrobble_log(&lines_to_remove, queuepath);
            g_slist_free_full(lines_to_remove, g_free);
        }
        g_strfreev(lines);
    }

    g_free(contents);
    g_free(queuepath);
}


//Scrobbling will only be enabled after the first connection test passed
gpointer scrobbling_thread (gpointer input_data) {

    while (scrobbler_running) {

        if (permission_check_requested) {
            if (session_key == NULL || strlen(session_key) == 0) {
                perm_result = PERMISSION_DENIED;

                if (request_token == NULL || strlen(request_token) == 0) {
                    if (scrobbler_request_token() == FALSE || request_token == NULL || strlen(request_token) == 0) {
                        perm_result = PERMISSION_NONET;
                    } //else PERMISSION_DENIED

                } else if (scrobbler_request_session() == FALSE) {
                    perm_result = PERMISSION_NONET;

                } else if (session_key == NULL || strlen(session_key) == 0) {
                    //This means we had a token, a session was requested now,
                    //but the token was not accepted or expired.
                    //Ask for a new token now.
                    if (scrobbler_request_token() == FALSE || request_token == NULL || strlen(request_token) == 0) {
                        perm_result = PERMISSION_NONET;
                    } //else PERMISSION_DENIED
                }
            }
            if (session_key != NULL && strlen(session_key) != 0 ){
                if (scrobbler_test_connection() == FALSE) {
                    perm_result = PERMISSION_NONET;

                    if (session_key == NULL || strlen(session_key) == 0) {
                        if (scrobbler_request_token() != FALSE && request_token != NULL && strlen(request_token) != 0) {
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
                        if (scrobbler_request_token() != FALSE && request_token != NULL && strlen(request_token) != 0) {
                            perm_result = PERMISSION_DENIED;
                        } else {
                            perm_result = PERMISSION_NONET;
                        }
                    }
                }
            } //session_key == NULL || strlen(session_key) == 0

            permission_check_requested = FALSE;
        } else if (FALSE) { //now_playing_requested

        } else {

            scrobble_cached_queue();
            //scrobbling may be disabled if communication errors occur

            g_mutex_lock(&communication_mutex);
            if (scrobbling_enabled) {
                g_cond_wait(&communication_signal, &communication_mutex);
                g_mutex_unlock(&communication_mutex);
            }
            else {
                //We don't want to wait until receiving a signal to retry
                //if submitting the cache failed due to network problems
                g_mutex_unlock(&communication_mutex);

                if (scrobbler_test_connection() == FALSE || !scrobbling_enabled) {
                    g_usleep(7*G_USEC_PER_SEC);
                }
            }
        }
    }//while(scrobbler_running)

    //reset all vars to their initial values
    if (received_data != NULL) {
        free(received_data);
        received_data = NULL;
    }
    received_data_size = 0;

    curl_easy_cleanup(curlHandle);
    curlHandle = NULL;

    scrobbling_enabled = TRUE;
    return NULL;
}

