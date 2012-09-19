/*
 * TODO: headers
 * scrobbler_communication.c
 *
 *  Created on: 10 Jun 2012
 *      Author: luis
 */

//plugin includes
#include "scrobbler.h"

//audacious includes


//external includes
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>


#include <gtk/gtk.h>


typedef struct {
    gchar *paramName;
    gchar *argument;
} API_Parameter;

static CURL *curlHandle = NULL;     //global handle holding cURL options

#define SCROBBLER_API_KEY "4b4f73bda181868353f9b438604adf52"
#define SCROBBLER_SHARED_SECRET "716cc0a784bb62835de5bd674e65eb57"
#define SCROBBLER_URL "http://ws.audioscrobbler.com/2.0/"
gchar *session_key = NULL;
gchar *request_token = NULL;

//shared variables
gchar *received_data = NULL;   //Holds the result of the last request made to last.fm
size_t received_data_size = 0; //Holds the size of the received_data buffer

bool_t scrobbling_enabled = FALSE;


/*
 * The cURL callback function to store the received data from the last.fm servers.
 */
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
 *   send_message_to_lastfm("track.scrobble", 3
 *        "artist", "Artist Name", "track", "Track Name", "timestamp", time(NULL));
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
    for (int i = 0 ; i < n_args+1; i++) {
        AUDDBG("signable_param[%i].name: %s. arg: %s.\n", i, signable_params[i].paramName, signable_params[i].argument);
    }

    result = malloc(msg_size);
    if (result == NULL) {
        perror("malloc") ;
        return NULL;
    }

    int curr_index = g_snprintf(result, msg_size, "%s=%s", "method", method_name);
    if (curr_index < 0) {
        perror("g_snprintf");
        return NULL;
    }
    AUDDBG("Current message: %s. Curr index: %i. Size: %zu\n", result, curr_index, msg_size);

    int last_index = curr_index;
    int msg_size_available = msg_size - (curr_index*sizeof(gchar));

    for (int i = 0 ; i < n_args; i++) {
        AUDDBG("Writing this (%i): %s, %s\n", i+1, signable_params[i+1].paramName, signable_params[i+1].argument);
        curr_index = g_snprintf(result+last_index, msg_size_available,
                          "&%s=%s", signable_params[i+1].paramName, signable_params[i+1].argument);
        if (curr_index < 0) {
            perror("g_snprintf");
            return NULL;
        }
        AUDDBG("Current message: %s. Curr index: %i.\n", result, curr_index);

        last_index = last_index + curr_index;
        msg_size_available = msg_size - (last_index*sizeof(gchar));
    }

    gchar *api_sig = scrobbler_get_signature(n_args+1, signable_params);
    msg_size = (msg_size + strlen("&api_sig=")+ strlen(api_sig) + 1) * sizeof(gchar) ;

    result = realloc(result, msg_size);
    AUDDBG("Current message: %s. Curr index: %i.\n", result, curr_index);

    strcat(result, "&api_sig=");
    strcat(result, api_sig);
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
    /*fflush(stdin);
    getchar();*/
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, data);
    CURLcode curl_requests_result = curl_easy_perform(curlHandle);

    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not communicate with last.fm: %s.\n", curl_easy_strerror(curl_requests_result));
        return FALSE;
    }

    return TRUE;
}


static void scrobbler_set_ok_to_scrobble (bool_t is_ok) {

    gchar *artist = calloc(500, sizeof(char));
    gchar *track = calloc(500, sizeof(char));

    AUDDBG("\nScrobble something!\nType the artist: ");

    fflush(stdin);
    gets(artist);

    AUDDBG("\nType the track name: ");
    fflush(stdin);
    gets(track);

    gchar *scrobblemsg = create_message_to_lastfm("track.updateNowPlaying",
                                                  4,
                                                  "artist", artist,
                                                  "track", track,
                                                  "api_key", SCROBBLER_API_KEY,
                                                  "sk", session_key);
    if (send_message_to_lastfm(scrobblemsg) == FALSE) {
        g_free(scrobblemsg);
        AUDDBG("Now Playing not sent to last.fm!\n");
        return;
    }
    g_free(scrobblemsg);

    AUDDBG("Now playing. Check last.fm.\nEnter?\n");
    getchar();


    gchar *timestamp = g_strdup_printf("%ld",time(NULL));
    scrobblemsg = create_message_to_lastfm("track.scrobble",
                                           5,
                                           "artist", artist,
                                           "track", track,
                                           "timestamp", timestamp,
                                           "api_key", SCROBBLER_API_KEY,
                                           "sk", session_key);
    if (send_message_to_lastfm(scrobblemsg) == FALSE) {
        g_free(scrobblemsg);
        AUDDBG("Could not scrobble!\n");
        return;
    }
    g_free(scrobblemsg);
    AUDDBG("SCROBBLED! Check last.fm.\nEnter?\n");
    getchar();
    return;
}

// Returns TRUE if authorization was granted by the user
// Returns FALSE if the user canceled the authorization request
// May (request) termination of the plugin
//TODO
static bool_t scrobbler_request_authorization() {
    //1. request an authentication token from last.fm
    gchar *tokenmsg = create_message_to_lastfm("auth.getToken",
                                              1,
                                              "api_key", SCROBBLER_API_KEY
                                             );

//    AUDDBG("THIS IS THE TOKENMSG: %s.\n", tokenmsg);
    if (send_message_to_lastfm(tokenmsg) == FALSE) {
        printf("Could not send token request to last.fm.\n");
        return FALSE;
    }

    if (read_token() == FALSE) {
        printf("Could not read received token.\n");
        return FALSE;
    }

    //2. Request authorization from the user
    printf("Go to http://www.last.fm/api/auth/?api_key=%s&token=%s.\n", SCROBBLER_API_KEY, request_token);
    printf("And accept the application.\nPress Enter when you have done it.\n");
    getc(stdin);

    gchar *sessionmsg = create_message_to_lastfm("auth.getSession",
                                                 2,
                                                 "token", request_token,
                                                 "api_key", SCROBBLER_API_KEY);

    if (send_message_to_lastfm(sessionmsg) == FALSE) {
        printf("Message not sent to last.fm - TODO: treat this.\n");
        return FALSE;
    }

    if (read_session_key() == FALSE) {
        printf("could not read session key.\n");
        return FALSE;
    }

    printf("Got a new session key! Storing it...\n");
    aud_set_string("scrobbler", "session_key", session_key);
    printf("Success! Check out the config file. Can now scrobble.\n");
    return TRUE;
}

//Check if we have a valid last.fm session
static void scrobbler_test_connection() {

    session_key = aud_get_string("scrobbler", "session_key");
    if (session_key == NULL || (*session_key) == 0 || (strlen(session_key) == 0)) {

        AUDDBG("No valid last.fm session key found.\n");
        if (scrobbler_request_authorization() == FALSE) {
            //TODO: see what we want to do here. Probably nothing.
            return;
        }
    }

//    AUDDBG("Got this session key: %s.\n", session_key);
//    AUDDBG("Checking session validity on last.fm.\n");
    gchar *testmsg = create_message_to_lastfm("user.getInfo",
                                              1,
                                              "api_key", SCROBBLER_API_KEY
                                             );

    bool_t success = send_message_to_lastfm(testmsg);
    g_free(testmsg);
    if (success == FALSE) {
        //TODO: how to treat network problems?
        printf("Network problems. Will not scrobble any tracks.\n");
        scrobbling_enabled = FALSE;
        g_usleep(20*G_USEC_PER_SEC); //retry after 20 seconds?
        return;
    }


    // If the session key is valid, we can make authenticated calls
    // Else, we don't have a valid session. we need to ask the user to accept
    //audacious at the last.fm site
    if (read_connection_test_result() == TRUE) {
        printf("Connection OK. Scrobbling enabled.\n");
        scrobbling_enabled = TRUE;
    } else {
        printf("Connection NOT OK. Scrobbling disabled\n");
        aud_interface_show_error("Last.fm didn't accept the application? Will not scrobble any tracks.");
        scrobbling_enabled = FALSE;
        g_usleep(20*G_USEC_PER_SEC); //retry after 20 seconds? (This is probably to get out of here)
        return;
    }

    return;
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
    AUDDBG("Segfault?\n");

    if (lines_to_remove == NULL) {
        return;
    }
    AUDDBG("Segfault?\n");
    lines_to_remove = g_slist_reverse(lines_to_remove);


    AUDDBG("Segfault?\n");
    g_mutex_lock(queue_mutex);

    gboolean success = g_file_get_contents(queuepath, &contents, NULL, NULL);
    if (!success) {
        AUDDBG("Could not read scrobbler.log contents.\n");
    } else {
        lines = g_strsplit(contents, "\n", 0);

        n_finallines = 0;
        for (int i = 0 ; lines[i] != NULL && strlen(lines[i]) > 0; i++) {
            AUDDBG("Line %i: %s. What is to del: %i\n.",i, lines[i], lines_to_remove == NULL? -1 : *((int *) (lines_to_remove->data)));
            if (lines_to_remove != NULL && *((int *) (lines_to_remove->data)) == i) {
                //this line is to remove
                lines_to_remove = g_slist_next(lines_to_remove);
            } else {
                //keep this line
                AUDDBG("Nope..\n");
                n_finallines++;
                finallines = g_realloc_n(finallines, n_finallines, sizeof(gchar *));
                finallines[n_finallines-1] = g_strdup(lines[i]);
            }
        }

        finallines = g_realloc_n(finallines, n_finallines+2, sizeof(gchar *));
        finallines[n_finallines] = g_strdup("");
        finallines[n_finallines+1] = NULL;
        g_free(contents);
//        if (finallines != NULL) {
            AUDDBG("Trying to strjoinv...\n");
            contents = g_strjoinv("\n", finallines);
            AUDDBG("strjoinv called successfully.\n");
            AUDDBG("Here are the contents: %s.\n", contents);
//        } else {
//            contents = g_strdup("");
//        }
        success = g_file_set_contents(queuepath, contents, -1, NULL);
        if (!success) {
            AUDDBG("Could not write to scrobbler.log!\n");
        }

    }

    g_mutex_unlock(queue_mutex);


    g_strfreev(finallines);
    g_strfreev(lines);
    g_free(contents);
}

static void save_line_to_remove(GSList **lines_to_remove, int linenumber) {
    int *rem = g_malloc(sizeof(int));
    *rem = linenumber;
    AUDDBG("DELTHIS: %i. Segfault?\n", linenumber);
    (*lines_to_remove) = g_slist_prepend((*lines_to_remove), rem);
    AUDDBG("No segfault?\n");
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

    g_mutex_lock(queue_mutex);
    success = g_file_get_contents(queuepath, &contents, NULL, NULL);
    g_mutex_unlock(queue_mutex);
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
                        AUDDBG("SCROBBLE OK.\n");
                        save_line_to_remove(&lines_to_remove, i);
                    } else {
                        AUDDBG("SCROBBLE NOT OK. Error code: %s. Error detail: %s.\n", error_code, error_detail);

                        if (error_code == NULL) { //net error(?) or the answer from last.fm was not well read
                            //scrobble to be retried
                        }
                        else if (g_strcmp0(error_code, "11") == 0 ||
                                 g_strcmp0(error_code, "16") == 0){
                            //scrobble to be retried
                        }
                        else if (g_strcmp0(error_code,  "9") == 0) {
                            //Bad Session. Reauth.
                            //TODO: show a popup telling the user he must re-configure the scrobbler?
                            scrobbling_enabled = FALSE;
                        }
                        else {
                            save_line_to_remove(&lines_to_remove, i);
                        }
                    }

                    AUDDBG("Segfault?\n");
                    g_free(error_code);

                    AUDDBG("No Segfault?\n");
                    g_free(error_detail);

                    AUDDBG("No No Segfault?\n");
                } else {
                    AUDDBG("Could not scrobble a track on the queue. Network problem?\n");
                    //scrobble to be retried
                    scrobbling_enabled = FALSE;
                }

                g_free(scrobblemsg);
            } else {
                AUDDBG("Unscrobbable line.\n");
                //leave entry to the cache file
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
gpointer scrobbling_thread (gpointer inputData) {
    //TODO: this flow is to be altered when we have a configuration GUI
    while (TRUE) {
        scrobbler_test_connection();

        while(scrobbling_enabled) {
            scrobble_cached_queue();

            if (scrobbling_enabled) {
                g_mutex_lock(queue_mutex);
                g_cond_wait(queue_signaler, queue_mutex);
                g_mutex_unlock(queue_mutex);
            }
        }
    }
    return NULL;
}
