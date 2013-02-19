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
#include <gtk/gtk.h>

//audacious includes
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/debug.h>
#include <audacious/preferences.h>

#define SCROBBLER_API_KEY "4b4f73bda181868353f9b438604adf52"
#define SCROBBLER_SHARED_SECRET "716cc0a784bb62835de5bd674e65eb57"
#define SCROBBLER_URL "http://ws.audioscrobbler.com/2.0/"


extern const PluginPreferences configuration;

extern enum permission {
    PERMISSION_UNKNOWN,
    PERMISSION_DENIED,
    PERMISSION_ALLOWED,
    PERMISSION_NONET
} perm_result;

extern bool_t scrobbler_running;
extern bool_t scrobbling_enabled;


//used to tell the scrobbling thread that there's something to do:
//A new track is to be scrobbled or a permission check was requested
extern pthread_mutex_t communication_mutex;
extern pthread_cond_t communication_signal;

//to avoid reading/writing the log file while other thread is accessing it
extern pthread_mutex_t log_access_mutex;

//TRUE when a permission check is being requested
extern bool_t permission_check_requested;
extern bool_t invalidate_session_requested;




//scrobbler_communication.c
extern bool_t   scrobbler_communication_init();
extern gpointer scrobbling_thread(gpointer data);
extern gpointer checking_thread(gpointer connection_test_result);



/* Internal stuff */
//Data sent to the XML parser
extern gchar *received_data;
extern size_t received_data_size;

//Data filled by the XML parser
extern gchar *request_token;
extern gchar *session_key;
extern gchar *username;

//scrobbler_xml_parsing.c
extern bool_t read_authentication_test_result(char **error_code, char **error_detail);
extern bool_t read_token(char **error_code, char **error_detail);
extern bool_t read_session_key(char **error_code, char **error_detail);
extern bool_t read_scrobble_result(char **error_code, char **error_detail);

#endif /* SCROBBLER_H_ */
