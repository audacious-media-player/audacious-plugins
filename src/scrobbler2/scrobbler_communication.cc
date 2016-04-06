
//external includes
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <curl/curl.h>

#include <glib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>

//plugin includes
#include "scrobbler.h"

typedef struct {
    String paramName;
    String argument;
} API_Parameter;

static CURL *curlHandle = nullptr;     //global handle holding cURL options

gboolean scrobbling_enabled = true;

//shared variables
char *received_data = nullptr;   //Holds the result of the last request made to last.fm
size_t received_data_size = 0; //Holds the size of the received_data buffer



// The cURL callback function to store the received data from the last.fm servers.
static size_t result_callback (void *buffer, size_t size, size_t nmemb, void *userp) {

    const size_t len = size*nmemb;

    char *temp_data = g_renew(char, received_data, received_data_size + len + 1);

    if (temp_data == nullptr) {
      return 0;
    } else {
      received_data = temp_data;
    }

    memcpy(received_data + received_data_size, buffer, len);

    received_data_size += len;

    return len;
}

static char * scrobbler_get_signature (Index<API_Parameter> & params)
{
    params.sort ([] (const API_Parameter & a, const API_Parameter & b)
        { return g_strcmp0 (a.paramName, b.paramName); });

    StringBuf buf (0);

    for (const API_Parameter & param : params)
    {
        buf.insert (-1, param.paramName);
        buf.insert (-1, param.argument);
    }

    buf.insert (-1, SCROBBLER_SHARED_SECRET);

    return g_compute_checksum_for_string (G_CHECKSUM_MD5, buf, -1);
}

/*
 * n_args should count with the given authentication parameters
 * At most 2: api_key, session_key.
 * api_sig (checksum) is always included, be it necessary or not
 * Example usage:
 *   create_message_to_lastfm("track.scrobble", 5
 *        "artist", "Artist Name", "track", "Track Name", "timestamp", time(nullptr),
 *        "api_key", SCROBBLER_API_KEY, "sk", session_key);
 *
 * Returns nullptr if an error occurrs
 */
static String create_message_to_lastfm (const char * method_name, int n_args, ...)
{
    Index<API_Parameter> params;
    params.append (String ("method"), String (method_name));

    StringBuf buf = str_concat ({"method=", method_name});

    va_list vl;
    va_start (vl, n_args);

    for (int i = 0; i < n_args; i ++)
    {
        const char * name = va_arg (vl, const char *);
        const char * arg = va_arg (vl, const char *);

        params.append (String (name), String (arg));

        char * esc = curl_easy_escape (curlHandle, arg, 0);
        buf.insert (-1, "&");
        buf.insert (-1, name);
        buf.insert (-1, "=");
        buf.insert (-1, esc);
        curl_free (esc);
    }

    va_end (vl);

    char * api_sig = scrobbler_get_signature (params);
    buf.insert (-1, "&api_sig=");
    buf.insert (-1, api_sig);
    g_free (api_sig);

    AUDDBG ("FINAL message: %s.\n", (const char *) buf);

    return String (buf);
}

static gboolean send_message_to_lastfm (const char * data)
{
    AUDDBG("This message will be sent to last.fm:\n%s\n%%%%End of message%%%%\n", data);//Enter?\n", data);
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, data);
    CURLcode curl_requests_result = curl_easy_perform(curlHandle);

    if (curl_requests_result != CURLE_OK) {
        AUDERR("Could not communicate with last.fm: %s.\n", curl_easy_strerror(curl_requests_result));
        return false;
    }

    return true;
}

//returns:
// FALSE if there was a network problem
// TRUE otherwise (request_token must be checked)
static gboolean scrobbler_request_token ()
{
    String tokenmsg = create_message_to_lastfm ("auth.getToken", 1, "api_key", SCROBBLER_API_KEY);

    if (send_message_to_lastfm(tokenmsg) == false) {
        AUDDBG("Could not send token request to last.fm.\n");
        return false;
    }

    gboolean success = true;
    String error_code;
    String error_detail;

    if (read_token(error_code, error_detail) == false) {
        success = false;
        if (error_code != nullptr && g_strcmp0(error_code, "8")) {
            //error code 8: There was an error granting the request token. Please try again later
            request_token = String();
        }
    }

    return success;
}


static gboolean update_session_key() {
    gboolean result = true;
    String error_code;
    String error_detail;

    if (read_session_key(error_code, error_detail) == false) {
        if (error_code != nullptr && (
                g_strcmp0(error_code,  "4") == 0 || //invalid token
                g_strcmp0(error_code, "14") == 0 || //token not authorized
                g_strcmp0(error_code, "15") == 0    //token expired
            )) {
            AUDINFO("error code CAUGHT: %s\n", (const char *)error_code);
            session_key = String();
            result = true;
        } else {
            result= false;
        }
    }

    aud_set_str("scrobbler", "session_key", session_key ? session_key : "");

    return result;
}

//returns:
// FALSE if there was a network problem
// TRUE otherwise (session_key must be checked)
static gboolean scrobbler_request_session ()
{
    String sessionmsg = create_message_to_lastfm ("auth.getSession", 2,
     "token", (const char *) request_token, "api_key", SCROBBLER_API_KEY);

    if (send_message_to_lastfm(sessionmsg) == false)
        return false;

    //the token can only be sent once
    request_token = String();

    return update_session_key();
}

//returns:
// FALSE if there was a network problem.
// TRUE otherwise (scrobbling_enabled must be checked)
//sets scrobbling_enabled to TRUE if the session is OK
//sets session_key to nullptr if it is invalid
static gboolean scrobbler_test_connection() {

    if (!session_key || !session_key[0]) {
        scrobbling_enabled = false;
        return true;
    }

    String testmsg = create_message_to_lastfm ("user.getInfo", 2,
     "api_key", SCROBBLER_API_KEY,
     "sk", (const char *) session_key);

    gboolean success = send_message_to_lastfm(testmsg);

    if (success == false) {
        AUDDBG("Network problems. Will not scrobble any tracks.\n");
        scrobbling_enabled = false;
        if (permission_check_requested) {
            perm_result = PERMISSION_NONET;
        }
        return false;
    }

    String error_code;
    String error_detail;

    if (read_authentication_test_result(error_code, error_detail) == false) {
        AUDINFO("Error code: %s. Detail: %s.\n", (const char *)error_code,
         (const char *)error_detail);
        if (error_code != nullptr && (
                g_strcmp0(error_code, "4") == 0 || //error code 4: Authentication Failed - You do not have permissions to access the service
                g_strcmp0(error_code, "9") == 0    //error code 9: Invalid session key - Please re-authenticate
            )) {
            session_key = String();
            aud_set_str("scrobbler", "session_key", "");
            scrobbling_enabled = false;
        } else {
            //network problem.
            scrobbling_enabled = false;
            AUDDBG("Connection NOT OK. Scrobbling disabled\n");
            success = false;
        }
    } else {
        //THIS IS THE ONLY PLACE WHERE SCROBBLING IS SET TO ENABLED IN RUN-TIME
        scrobbling_enabled = true;
        AUDDBG("Connection OK. Scrobbling enabled.\n");
    }

    return success;
}

//called from scrobbler_init() @ scrobbler.c
gboolean scrobbler_communication_init() {
    CURLcode curl_requests_result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not initialize libCURL: %s.\n", curl_easy_strerror(curl_requests_result));
        return false;
    }

    curlHandle = curl_easy_init();
    if (curlHandle == nullptr) {
        AUDDBG("Could not initialize libCURL.\n");
        return false;
    }

    curl_requests_result = curl_easy_setopt(curlHandle, CURLOPT_URL, SCROBBLER_URL);
    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not define scrobbler destination URL: %s.\n", curl_easy_strerror(curl_requests_result));
        return false;
    }

    curl_requests_result = curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, result_callback);
    if (curl_requests_result != CURLE_OK) {
        AUDDBG("Could not register scrobbler callback function: %s.\n", curl_easy_strerror(curl_requests_result));
        return false;
    }

    return true;
}

static void set_timestamp_to_current(char **line) {
    //line[0] line[1] line[2] line[3] line[4] line[5] line[6]   line[7]
    //artist  album   title   number  length  "L"     timestamp nullptr

    char **splitted_line = g_strsplit(*line, "\t", 0);
    g_free(splitted_line[6]);
    splitted_line[6] = g_strdup_printf("%" G_GINT64_FORMAT, g_get_real_time() / G_USEC_PER_SEC);
    AUDDBG("splitted line's timestamp is now: %s.\n", splitted_line[6]);
    g_free(*line);
    (*line) = g_strjoinv("\t", splitted_line);
}

static void delete_lines_from_scrobble_log (GSList **lines_to_remove_ptr, GSList **lines_to_retry_ptr, char *queuepath) {
    GSList *lines_to_remove = *lines_to_remove_ptr;
    GSList *lines_to_retry = *lines_to_retry_ptr;
    char *contents = nullptr;
    char **lines = nullptr;
    char **finallines = g_new (char *, 1);
    int n_finallines;

    if (lines_to_remove != nullptr) {
        lines_to_remove = g_slist_reverse(lines_to_remove);
    }
    if (lines_to_retry != nullptr) {
        lines_to_retry = g_slist_reverse(lines_to_retry);
    }


    pthread_mutex_lock(&log_access_mutex);

    gboolean success = g_file_get_contents(queuepath, &contents, nullptr, nullptr);
    if (!success) {
        AUDDBG("Could not read scrobbler.log contents.\n");
    } else {
        lines = g_strsplit(contents, "\n", 0);

        n_finallines = 0;
        for (int i = 0 ; lines[i] != nullptr && strlen(lines[i]) > 0; i++) {
            if (lines_to_remove != nullptr && *((int *) (lines_to_remove->data)) == i) {
                //this line is to remove
                lines_to_remove = g_slist_next(lines_to_remove);
            } else {
                //keep this line
                AUDDBG("Going to keep line %i\n", i);
                if (lines_to_retry != nullptr && *((int *) (lines_to_retry->data)) == i) {
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
                finallines = g_renew (char *, finallines, n_finallines);
                finallines[n_finallines-1] = g_strdup(lines[i]);
            }
        }

        finallines = g_renew (char *, finallines, n_finallines + 2);
        finallines[n_finallines] = g_strdup("");
        finallines[n_finallines+1] = nullptr;
        g_free(contents);
        contents = g_strjoinv("\n", finallines);
        success = g_file_set_contents(queuepath, contents, -1, nullptr);
        if (!success) {
            AUDERR("Could not write to scrobbler.log!\n");
        }

    }

    pthread_mutex_unlock(&log_access_mutex);


    g_strfreev(finallines);
    g_strfreev(lines);
    g_free(contents);
}

static void save_line_to_remove(GSList **lines_to_remove, int linenumber) {
    int *rem = g_new (int, 1);
    *rem = linenumber;
    (*lines_to_remove) = g_slist_prepend((*lines_to_remove), rem);
}

static void scrobble_cached_queue() {

    char *queuepath = g_build_filename(aud_get_path(AudPath::UserDir),"scrobbler.log", nullptr);
    char *contents = nullptr;
    gboolean success;
    char **lines = nullptr;
    char **line;
    GSList *lines_to_remove = nullptr; //lines to remove because they were scrobbled (or ignored, and will not be retried)
    GSList *lines_to_retry = nullptr; //lines to retry later because they were too old TODO: or "daily scrobble limit reached"

    pthread_mutex_lock(&log_access_mutex);
    success = g_file_get_contents(queuepath, &contents, nullptr, nullptr);
    pthread_mutex_unlock(&log_access_mutex);
    if (!success) {
        AUDDBG("Couldn't access the queue file.\n");
    } else {

        lines = g_strsplit(contents, "\n", 0);

        for (int i = 0; lines[i] != nullptr && strlen(lines[i]) > 0 && scrobbling_enabled; i++) {
            line = g_strsplit(lines[i], "\t", 0);

            //line[0] line[1] line[2] line[3] line[4] line[5] line[6]   line[7]
            //artist  album   title   number  length  "L"     timestamp nullptr

            if (line[0] && line[2] && (strcmp(line[5], "L") == 0) && line[6] && (line[7] == nullptr))
            {
                String scrobblemsg = create_message_to_lastfm ("track.scrobble",
                 8, "artist", line[0], "album", line[1], "track", line[2],
                 "trackNumber", line[3], "duration", line[4],
                 "timestamp", line[6], "api_key", SCROBBLER_API_KEY,
                 "sk", (const char *) session_key);

                if (send_message_to_lastfm(scrobblemsg) == true) {
                    String error_code;
                    String error_detail;
                    gboolean ignored = false;
                    String ignored_code;

                    if (read_scrobble_result(error_code, error_detail, &ignored, ignored_code) == true) {
                        AUDDBG("SCROBBLE OK. Error code: %s. Error detail: %s\n",
                         (const char *)error_code, (const char *)error_detail);
                        AUDDBG("SCROBBLE OK. ignored: %i.\n", ignored);
                        AUDDBG("SCROBBLE OK. ignored code: %s.\n",
                         (const char *)ignored_code);
                        if (ignored == true && g_strcmp0(ignored_code, "3") == 0) { //3: Timestamp was too old
                            AUDDBG("SCROBBLE IGNORED!!! %i, detail: %s\n",
                             ignored, (const char *)ignored_code);
                            save_line_to_remove(&lines_to_retry, i);
                        } else if (ignored == true && g_strcmp0(ignored_code, "") == 0) { //5: Daily scrobble limit reached
                           //TODO: a track might not be scrobbled due to "daily scrobble limit exeeded".
                           //This message comes on the ignoredMessage attribute, inside the XML of the response.
                           //We are not dealing with this case currently and are losing that scrobble.
                           //TODO

                        } else {
                            AUDDBG("Not ignored. Carrying on...\n");
                            save_line_to_remove(&lines_to_remove, i);
                        }
                    } else {
                        AUDINFO("SCROBBLE NOT OK. Error code: %s. Error detail: %s.\n",
                         (const char *)error_code, (const char *)error_detail);

                        if (! error_code) { //net error(?) or the answer from last.fm was not well read
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
                            scrobbling_enabled = false;
                            session_key = String();
                            aud_set_str("scrobbler", "session_key", "");
                        }
                        else {
                            save_line_to_remove(&lines_to_remove, i);
                        }
                    }
                } else {
                    AUDDBG("Could not scrobble a track on the queue. Network problem?\n");
                    //scrobble to be retried
                    scrobbling_enabled = false;
                }

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

        if (lines_to_remove != nullptr) {
            g_slist_free_full(lines_to_remove, g_free);
        }
        if (lines_to_retry != nullptr) {
            g_slist_free_full(lines_to_retry, g_free);
        }

        g_strfreev(lines);
    }

    g_free(contents);
    g_free(queuepath);
}


static void send_now_playing() {

  String error_code;
  String error_detail;
  gboolean ignored = false;
  String ignored_code;
  /*
   * now_playing_track can be set to something else while we this method is
   * running. Creating a local variable avoids to get data for different tracks,
   * while now_playing_track was updated concurrently.
   *
   * FIXME: Make this actually thread-safe.
   */
  Tuple curr_track = now_playing_track.ref ();

  StringBuf artist = clean_string (curr_track.get_str (Tuple::Artist));
  StringBuf title = clean_string (curr_track.get_str (Tuple::Title));
  StringBuf album = clean_string (curr_track.get_str (Tuple::Album));

  int track  = curr_track.get_int (Tuple::Track);
  int length = curr_track.get_int (Tuple::Length);

  if (artist[0] && title[0] && length > 0) {
    StringBuf track_str = (track > 0) ? int_to_str (track) : StringBuf (0);
    StringBuf length_str = int_to_str (length / 1000);

    String playingmsg = create_message_to_lastfm ("track.updateNowPlaying", 7,
     "artist", (const char *) artist, "album", (const char *) album,
     "track", (const char *) title, "trackNumber", (const char *) track_str,
     "duration", (const char *) length_str, "api_key", SCROBBLER_API_KEY,
     "sk", (const char *) session_key);

    gboolean success = send_message_to_lastfm(playingmsg);

    if (success == false) {
      AUDDBG("Network problems. Could not send \"now playing\" to last.fm\n");
      scrobbling_enabled = false;
    } else if (read_scrobble_result(error_code, error_detail, &ignored, ignored_code) == true) {
      //see scrobble_cached_queue()
      AUDDBG("NOW PLAYING OK.\n");
    } else {
      AUDINFO("NOW PLAYING NOT OK. Error code: %s. Error detail: %s.\n",
       (const char *)error_code, (const char *)error_detail);
      //From the API: Now Playing requests that fail should not be retried.

      if (g_strcmp0(error_code, "9") == 0) {
        //Bad Session. Reauth.
        //We don't really care about any other errors.
        scrobbling_enabled = false;
        session_key = String();
        aud_set_str("scrobbler", "session_key", "");
      }

    }
    //We don't care if the now playing was not accepted, no need to read the result from the server.

  }
}

static void treat_permission_check_request() {
    if (!session_key || !session_key[0]) {
        perm_result = PERMISSION_DENIED;

        if (!request_token || !request_token[0]) {
            if (scrobbler_request_token() == false || !request_token || !request_token[0]) {
                perm_result = PERMISSION_NONET;
            } //else PERMISSION_DENIED

        } else if (scrobbler_request_session() == false) {
            perm_result = PERMISSION_NONET;

        } else if (!session_key || !session_key[0]) {
            //This means we had a token, a session was requested now,
            //but the token was not accepted or expired.
            //Ask for a new token now.
            if (scrobbler_request_token() == false || !request_token || !request_token[0]) {
                perm_result = PERMISSION_NONET;
            } //else PERMISSION_DENIED
        }
    }
    if (session_key && session_key[0]) {
        if (scrobbler_test_connection() == false) {
            perm_result = PERMISSION_NONET;

            if (!session_key || !session_key[0]) {
                if (scrobbler_request_token() != false && request_token && request_token[0]) {
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
                if (scrobbler_request_token() != false && request_token && request_token[0]) {
                    perm_result = PERMISSION_DENIED;
                } else {
                    perm_result = PERMISSION_NONET;
                }
            }
        }
    } //session_key == nullptr || strlen(session_key) == 0
}

// This is a sister function of scrobbler_request_session, using the getMobileSession
//API call, for migrating from the old config
//returns:
// FALSE if there was a network problem OR a session_key was not obtained
// TRUE if a new session_key was obtained
static gboolean treat_migrate_config() {

    String password = aud_get_str("audioscrobbler","password");
    String username = aud_get_str("audioscrobbler","username");
    if (!password[0] || !username[0])
        return false;

    char *checksumThis = g_strdup_printf("%s%s", (const char *)username, (const char *)password);
    char *authToken = g_compute_checksum_for_string(G_CHECKSUM_MD5, checksumThis, -1);

    String sessionmsg = create_message_to_lastfm ("auth.getMobileSession", 3,
     "authToken", authToken, "username", (const char *) username,
     "api_key", SCROBBLER_API_KEY);

    g_free(checksumThis);
    g_free(authToken);

    if (send_message_to_lastfm(sessionmsg) == false)
        return false;

    if (!update_session_key())
        return false;

    return (session_key && session_key[0]);
}


//Scrobbling will only be enabled after the first connection test passed
void * scrobbling_thread (void * input_data) {

    while (scrobbler_running) {

        if (migrate_config_requested) {
          if (treat_migrate_config() == false) {
            aud_ui_show_error(_("Audacious is now using an improved version of the Last.fm Scrobbler.\nPlease check the Preferences for the Scrobbler plugin."));
          }
          aud_set_str("scrobbler", "migrated", "true");
          migrate_config_requested = false;

        } else if (permission_check_requested) {
            treat_permission_check_request();
            permission_check_requested = false;

        } else if (invalidate_session_requested) {
            session_key = String();
            aud_set_str("scrobbler", "session_key", "");
            invalidate_session_requested = false;

        } else if (now_playing_requested) {
            if (scrobbling_enabled) {
              send_now_playing();
            }
            now_playing_requested = false;

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

                if (scrobbler_test_connection() == false || !scrobbling_enabled) {
                    struct timeval curtime;
                    struct timespec timeout;
                    pthread_mutex_lock(&communication_mutex);
                    gettimeofday(&curtime, nullptr);
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
    received_data = nullptr;
    received_data_size = 0;

    curl_easy_cleanup(curlHandle);
    curlHandle = nullptr;

    scrobbling_enabled = true;
    return nullptr;
}

