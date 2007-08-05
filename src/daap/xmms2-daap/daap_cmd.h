/** @file daap_cmd.h
 *
 *  Copyright (C) 2006-2007 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef DAAP_CMD_H
#define DAAP_CMD_H

#include "cc_handlers.h"

/**
 * Log into a DAAP server.
 * Issue the command necessary for logging in.
 *
 * @param host host IP of server
 * @param port port that the server uses on host
 * @param request_id the request id
 * @return a session id for use in further commands
 */
guint
daap_command_login (gchar *host, gint port, guint request_id);

/**
 * Update the DAAP server status.
 * Issue the command necessary for updating the connection to a DAAP server.
 *
 * @param host host IP of server
 * @param port port that the server uses on host
 * @param session_id the id of the current session
 * @param request_id the request id
 * @return a revision id for use in further commands
 */
guint
daap_command_update (gchar *host, gint port, guint session_id, guint request_id);

/**
 * Log out of a DAAP server.
 * Issue the command necessary for logging out.
 *
 * @param host host IP of server
 * @param port port that the server uses on host
 * @param session_id the id of the current session
 * @param request_id the request id
 * @return TRUE on success, FALSE otherwise
 */
gboolean
daap_command_logout (gchar *host, gint port, guint session_id, guint request_id);

/**
 * Get a list of databases.
 * Issue the command for fetching a list of song databases on the server.
 *
 * @param host host IP of server
 * @param port port that the server uses on host
 * @param session_id the id of the current session
 * @param revision_id the id of the current revision
 * @param request_id the request id
 * @return a list of database ids
 */
GSList *
daap_command_db_list (gchar *host, gint port, guint session_id,
                      guint revision_id, guint request_id);

/**
 * Get a list of songs in a database.
 * Issue the command for fetching a list of songs in a database on the server.
 *
 * @param host host IP of server
 * @param port port that the server uses on host
 * @param session_id the id of the current session
 * @param revision_id the id of the current revision
 * @param request_id the request id
 * @param db_id the database id
 * @return a list of songs in the database
 */
GSList *
daap_command_song_list (gchar *host, gint port, guint session_id,
                        guint revision_id, guint request_id, gint db_id);

/**
 * Begin streaming a song.
 * Issue the command for streaming a song on the server.
 * NOTE: This command only _begins_ the stream; unlike the other command
 * functions, this one does not close the socket/channel, this must be done
 * manually after reading the data.
 *
 * @param host host IP of server
 * @param port port that the server uses on host
 * @param session_id the id of the current session
 * @param revision_id the id of the current revision
 * @param request_id the request id
 * @param dbid the database id
 * @param song a string containing the id and file type of the song to stream
 * @param filesize a pointer to an integer that stores the content length 
 * @return: a GIOChannel corresponding to streaming song data
 */
GIOChannel *
daap_command_init_stream (gchar *host, gint port, guint session_id,
                          guint revision_id, guint request_id,
                          gint dbid, gchar *song, guint *filesize);

#endif
