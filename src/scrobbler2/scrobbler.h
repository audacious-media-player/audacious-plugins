/*
 * scrobbler2.h
 *
 *  Created on: 10 Jun 2012
 *      Author: luis
 */

#ifndef SCROBBLER_H_
#define SCROBBLER_H_


//audacious includes
#include <audacious/misc.h>
#include <audacious/debug.h>

//external includes
#include <glib.h>
#include <libxml/xpath.h>



extern GCond  *queue_signaler;
extern GMutex *queue_mutex;

//scrobbler_communication.c
extern bool_t   scrobbler_communication_init();
extern gpointer scrobbling_thread(gpointer data);



/* Internal stuff */
//Data sent to the XML parser
extern gchar *received_data;
extern size_t received_data_size;

//Data filled by the XML parser
extern gchar *request_token;
extern gchar *session_key;

//scrobbler_xml_parsing.c
extern bool_t read_connection_test_result();
extern bool_t read_token();
extern bool_t read_session_key();
extern bool_t read_scrobble_result(char **error_code, char **error_detail);

#endif /* SCROBBLER_H_ */
