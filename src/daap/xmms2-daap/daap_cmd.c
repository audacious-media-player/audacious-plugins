/** @file daap_cmd.c
 *  Wrapper functions for issuing DAAP commands.
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

#include "daap_cmd.h"
#include "daap_conn.h"

static cc_data_t *
daap_request_data (GIOChannel *chan, const gchar *path, gchar *host, guint request_id);
static gboolean
daap_request_stream (GIOChannel *chan, gchar *path, gchar *host,
                     guint request_id, guint *size);
static gchar *
daap_url_append_meta (gchar *url, GSList *meta_list);

guint
daap_command_login (gchar *host, gint port, guint request_id ) {
	GIOChannel *chan;
	cc_data_t *cc_data;

	guint session_id = 0;

	chan = daap_open_connection (host, port);
	if (!chan) {
        g_print("Connection to server failed! "
		        "Please make sure the url is of the form:\n"
		        "daap://ip[:port]/[song]");
		return 0;
	}

	cc_data = daap_request_data (chan, "/login", host, request_id);
	if (cc_data) {
		session_id = cc_data->session_id;
		cc_data_free (cc_data);
	}

	g_io_channel_shutdown (chan, TRUE, NULL);
	g_io_channel_unref (chan);

	return session_id;
}

guint
daap_command_update (gchar *host, gint port, guint session_id, guint request_id)
{
	GIOChannel *chan;
	gchar *request;
	cc_data_t *cc_data;
	guint revision_id = 0;

	chan = daap_open_connection (host, port);
	if (!chan) {
		return 0;
	}

	request = g_strdup_printf ("/update?session-id=%d", session_id);

	cc_data = daap_request_data (chan, request, host, request_id);
	if (cc_data) {
		revision_id = cc_data->revision_id;
		cc_data_free (cc_data);
	}

	g_free (request);
	g_io_channel_shutdown (chan, TRUE, NULL);
	g_io_channel_unref (chan);

	return revision_id;
}

gboolean
daap_command_logout (gchar *host, gint port, guint session_id, guint request_id)
{
	GIOChannel *chan;
	gchar *request;

	chan = daap_open_connection (host, port);
	if (!chan) {
		return FALSE;
	}

	request = g_strdup_printf ("/logout?session-id=%d", session_id);

	/* there is no cc_data generated, so we don't need to store it anywhere */
	daap_request_data (chan, request, host, request_id);

	g_free (request);
	g_io_channel_shutdown (chan, TRUE, NULL);
	g_io_channel_unref (chan);

	return TRUE;
}

GSList *
daap_command_db_list (gchar *host, gint port, guint session_id,
                      guint revision_id, guint request_id)
{
	GIOChannel *chan;
	gchar *request;
	cc_data_t *cc_data;
	GSList *db_id_list = NULL;

	chan = daap_open_connection (host, port);
	if (!chan) {
		return NULL;
	}

	request = g_strdup_printf ("/databases?session-id=%d&revision-id=%d",
	                           session_id, revision_id);

	cc_data = daap_request_data (chan, request, host, request_id);
	g_free (request);
	if (cc_data) {
		db_id_list = cc_record_list_deep_copy (cc_data->record_list);
		cc_data_free (cc_data);
	}

	g_io_channel_shutdown (chan, TRUE, NULL);
	g_io_channel_unref (chan);

	return db_id_list;
}

GSList *
daap_command_song_list (gchar *host, gint port, guint session_id,
                        guint revision_id, guint request_id, gint db_id)
{
	GIOChannel *chan;
	gchar *request;
	cc_data_t *cc_data;

	GSList * song_list;
	GSList * meta_items = NULL;

	chan = daap_open_connection (host, port);
	if (!chan) {
		return NULL;
	}

	meta_items = g_slist_prepend (meta_items, g_strdup ("dmap.itemid"));
	meta_items = g_slist_prepend (meta_items, g_strdup ("dmap.itemname"));
	meta_items = g_slist_prepend (meta_items, g_strdup ("daap.songartist"));
	meta_items = g_slist_prepend (meta_items, g_strdup ("daap.songformat"));
	meta_items = g_slist_prepend (meta_items, g_strdup ("daap.songtracknumber"));
	meta_items = g_slist_prepend (meta_items, g_strdup ("daap.songalbum"));

	request = g_strdup_printf ("/databases/%d/items?"
	                           "session-id=%d&revision-id=%d",
	                           db_id, session_id, revision_id);

	if (meta_items) {
		request = daap_url_append_meta (request, meta_items);
	}

	cc_data = daap_request_data (chan, request, host, request_id);
	song_list = cc_record_list_deep_copy (cc_data->record_list);

	g_free (request);
	cc_data_free (cc_data);
	g_io_channel_shutdown (chan, TRUE, NULL);
	g_io_channel_unref (chan);
	g_slist_foreach (meta_items, (GFunc) g_free, NULL);
	g_slist_free (meta_items);

	return song_list;
}

GIOChannel *
daap_command_init_stream (gchar *host, gint port, guint session_id,
                          guint revision_id, guint request_id,
                          gint dbid, gchar *song, guint *filesize)
{
	GIOChannel *chan;
	gchar *request;
	gboolean ok;

	chan = daap_open_connection (host, port);
	if (!chan) {
		return NULL;
	}

	request = g_strdup_printf ("/databases/%d/items%s"
	                           "?session-id=%d",
	                           dbid, song, session_id);

	ok = daap_request_stream (chan, request, host, request_id, filesize);
	g_free (request);

	if (!ok) {
		return NULL;
	}

	return chan;
}

static cc_data_t *
daap_request_data (GIOChannel *chan, const gchar *path, gchar *host, guint request_id)
{
	guint status;
	gchar *request, *header = NULL;
	cc_data_t *retval;

	request = daap_generate_request (path, host, request_id);
	daap_send_request (chan, request);
	g_free (request);

	daap_receive_header (chan, &header);
	if (!header) {
		return NULL;
	}

	status = get_server_status (header);

	switch (status) {
		case UNKNOWN_SERVER_STATUS:
		case HTTP_BAD_REQUEST:
		case HTTP_FORBIDDEN:
		case HTTP_NO_CONTENT:
		case HTTP_NOT_FOUND:
			retval = NULL;
			break;
		case HTTP_OK:
		default:
			retval = daap_handle_data (chan, header);
			break;
	}
	g_free (header);

	return retval;
}

static gboolean
daap_request_stream (GIOChannel *chan, gchar *path, gchar *host,
                     guint request_id, guint *size)
{
	guint status;
	gchar *request, *header = NULL;

	request = daap_generate_request (path, host, request_id);
	daap_send_request (chan, request);
	g_free (request);

	daap_receive_header (chan, &header);
	if (!header) {
		return FALSE;
	}

	status = get_server_status (header);
	if (HTTP_OK != status) {
		g_free (header);
		return FALSE;
	}

	*size = get_data_length (header);

	g_free (header);

	return TRUE;
}

static gchar *
daap_url_append_meta (gchar *url, GSList *meta_list)
{
	gchar * tmpurl;

	tmpurl = url;
	url = g_strdup_printf ("%s&meta=%s", url, (gchar *) meta_list->data);
	g_free (tmpurl);
	meta_list = g_slist_next (meta_list);

	for ( ; meta_list != NULL; meta_list = g_slist_next (meta_list)) {
		tmpurl = url;
		url = g_strdup_printf ("%s,%s", url, (gchar *) meta_list->data);
		g_free (tmpurl);
	}

	return url;
}

