/*
 * Scrobbler Plugin v2.0 for Audacious by Pitxyoki
 *
 * Copyright 2012-2013 Luís Picciochi Oliveira <Pitxyoki@Gmail.com>
 *
 * This plugin is part of the Audacious Media Player.
 * It is licensed under the GNU General Public License, version 3.
 */

#ifndef SCROBBLER_H_
#define SCROBBLER_H_

//external includes
#include <pthread.h>
#include <string.h>
#include <glib.h>
#include <libxml/xpath.h>

//audacious includes
#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>

#define SCROBBLER_API_KEY "4b4f73bda181868353f9b438604adf52"
#define SCROBBLER_SHARED_SECRET "716cc0a784bb62835de5bd674e65eb57"
#define SCROBBLER_URL "https://ws.audioscrobbler.com/2.0/"


extern const PluginPreferences configuration;

extern enum permission {
    PERMISSION_UNKNOWN,
    PERMISSION_DENIED,
    PERMISSION_ALLOWED,
    PERMISSION_NONET
} perm_result;

extern gboolean scrobbler_running;
extern gboolean scrobbling_enabled;


//used to tell the scrobbling thread that there's something to do:
//A new track is to be scrobbled or a permission check was requested
extern pthread_mutex_t communication_mutex;
extern pthread_cond_t communication_signal;

//to avoid reading/writing the log file while other thread is accessing it
extern pthread_mutex_t log_access_mutex;

/* All "something"_requested variables are set to TRUE by the requester.
 * The scrobbling thread shall set them to FALSE and invalidate any auxiliary
 *data (such as now_playing_track).
 */

//TRUE when a permission check is being requested
extern gboolean permission_check_requested;
extern gboolean invalidate_session_requested;

//Migrate the settings from the old scrobbler
extern gboolean migrate_config_requested;

//Send "now playing"
extern gboolean now_playing_requested;
extern Tuple now_playing_track;




//scrobbler_communication.c
extern gboolean   scrobbler_communication_init();
extern void * scrobbling_thread(void * data);



/* Internal stuff */
//Data sent to the XML parser
extern char *received_data;
extern size_t received_data_size;

//Data filled by the XML parser
extern String request_token;
extern String session_key;
extern String username;

//scrobbler_xml_parsing.c
extern gboolean read_authentication_test_result(String &error_code, String &error_detail);
extern gboolean read_token(String &error_code, String &error_detail);
extern gboolean read_session_key(String &error_code, String &error_detail);
extern gboolean read_scrobble_result(String &error_code, String &error_detail, gboolean *ignored, String &ignored_code);

//scrobbler.c
extern StringBuf clean_string(const char *string);
#endif /* SCROBBLER_H_ */
