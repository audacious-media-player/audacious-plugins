/** @file daap_xform.c
 *  XMMS2 transform for accessing DAAP music shares.
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

/* XXX as of the current implementation, there is no method of logging out
 * of the servers.
 */

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include "daap_cmd.h"
#include "daap_util.h"
#include "daap_mdns_browse.h"

#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>

#define DEFAULT_DAAP_PORT 3689

/*
 * Type definitions
 */

typedef struct {
	gchar *host;
	guint port;

	GIOChannel *channel;

	xmms_error_t status;
} xmms_daap_data_t;

typedef struct {
	gboolean logged_in;

	guint session_id;
	guint revision_id;
	guint request_id;
} xmms_daap_login_data_t;

static GHashTable *login_sessions = NULL;

/*
 * Function prototypes
 */

static gboolean
xmms_daap_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean
xmms_daap_init (xmms_xform_t *xform);
static void
xmms_daap_destroy (xmms_xform_t *xform);
static gint
xmms_daap_read (xmms_xform_t *xform, void *buffer,
                gint len, xmms_error_t *error);
static gboolean
xmms_daap_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("daap",
                   "DAAP access plugin",
                   "SoC",
                   "Accesses iTunes (DAAP) music shares",
                   xmms_daap_plugin_setup);


/**
 * Extract hostname, port and command from an url.
 * daap://hostname:port/command
 */
static gboolean
get_data_from_url (const gchar *url, gchar **host, guint *port, gchar **cmd, xmms_error_t *err)
{
	const gchar *port_ptr, *cmd_ptr, *end_ptr, *stripped;

	stripped = url + sizeof (gchar) * strlen ("daap://");

	end_ptr = stripped + sizeof (gchar) * strlen (stripped);

	if (stripped == end_ptr) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Empty URL");
		return FALSE;
	}

	port_ptr = strstr (stripped, ":");
	if (port && port_ptr && (port_ptr + 1) != end_ptr) {
		*port = strtol (port_ptr + 1, (gchar **) NULL, 10);
		if (*port == 0) {
			*port = DEFAULT_DAAP_PORT;
		}
	} else if (port) {
		*port = DEFAULT_DAAP_PORT;
	}

	cmd_ptr = strstr (stripped, "/");
	if (cmd && cmd_ptr && (cmd_ptr + 1) != end_ptr) {
		*cmd = g_strdup (cmd_ptr);
	} else if (cmd) {
		/* cmd wanted but not found */
		xmms_error_set (err, XMMS_ERROR_INVAL, "No file requested");
	} else if (!cmd && cmd_ptr && (cmd_ptr + 1) != end_ptr) {
		/* cmd not wanted but found */
		xmms_error_set (err, XMMS_ERROR_NOENT, "No such directory");
		return FALSE;
	}

	if (port_ptr) {
		*host = g_strndup (stripped, port_ptr - stripped);
	} else if (cmd_ptr) {
		*host = g_strndup (stripped, cmd_ptr - stripped);
	} else {
		*host = g_strdup (stripped);
	}

	return TRUE;
}


static gboolean
xmms_daap_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_daap_init;
	methods.destroy = xmms_daap_destroy;
	methods.read = xmms_daap_read;
	methods.browse = xmms_daap_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "daap://*",
	                              XMMS_STREAM_TYPE_END);

	daap_mdns_initialize ();

	if (!login_sessions) {
		login_sessions = g_hash_table_new (g_str_hash, g_str_equal);
	}

	return TRUE;
}


/**
 * Add a song to the browsing list.
 */
static void
daap_add_song_to_list (xmms_xform_t *xform, cc_item_record_t *song)
{
	gchar *songurl;

	songurl = g_strdup_printf ("%u.%s", song->dbid, song->song_format);
	xmms_xform_browse_add_entry (xform, songurl, 0);
	g_free (songurl);

	if (song->iname) {
		xmms_xform_browse_add_entry_property_str (xform, "title",
		                                          song->iname);
	}

	if (song->song_data_artist) {
		xmms_xform_browse_add_entry_property_str (xform, "artist",
		                                          song->song_data_artist);
	}

	if (song->song_data_album) {
		xmms_xform_browse_add_entry_property_str (xform, "album",
		                                          song->song_data_album);
	}

	xmms_xform_browse_add_entry_property_int (xform, "tracknr",
	                                          song->song_track_no);
}


/**
 * Scan a daap server for songs.
 */
static gboolean
daap_get_urls_from_server (xmms_xform_t *xform, gchar *host, guint port,
                           xmms_error_t *err)
{
	GSList *dbid_list = NULL;
	GSList *song_list = NULL, *song_el;
	cc_item_record_t *db_data;
	xmms_daap_login_data_t *login_data;
	gchar *hash;

	hash = g_strdup_printf ("%s:%u", host, port);

	login_data = g_hash_table_lookup (login_sessions, hash);

	if (!login_data) {
		login_data = g_new0 (xmms_daap_login_data_t, 1);

		login_data->session_id = daap_command_login (host, port, 0, err);
		if (xmms_error_iserror (err)) {
			return FALSE;
		}

		login_data->revision_id = daap_command_update (host, port,
		                                               login_data->session_id,
		                                               0);

		login_data->request_id = 1;
		login_data->logged_in = TRUE;

		g_hash_table_insert (login_sessions, hash, login_data);
	} else {
		login_data->revision_id = daap_command_update (host, port,
		                                               login_data->session_id,
		                                               0);
	}

	dbid_list = daap_command_db_list (host, port, login_data->session_id,
	                                  login_data->revision_id, 0);
	if (!dbid_list) {
		return FALSE;
	}

	/* XXX i've never seen more than one db per server out in the wild,
	 *     let's hope that never changes *wink*
	 *     just use the first db in the list */
	db_data = (cc_item_record_t *) dbid_list->data;
	song_list = daap_command_song_list (host, port, login_data->session_id,
	                                    login_data->revision_id,
	                                    0, db_data->dbid);

	g_slist_foreach (dbid_list, (GFunc) cc_item_record_free, NULL);
	g_slist_free (dbid_list);

	if (!song_list) {
		return FALSE;
	}

	for (song_el = song_list; song_el; song_el = g_slist_next (song_el)) {
		daap_add_song_to_list (xform, song_el->data);
	}

	g_slist_foreach (song_list, (GFunc) cc_item_record_free, NULL);
	g_slist_free (song_list);

	return TRUE;
}


/*
 * Member functions
 */

static gboolean
xmms_daap_init (xmms_xform_t *xform)
{
	gint dbid;
	GSList *dbid_list = NULL;
	xmms_daap_data_t *data;
	xmms_daap_login_data_t *login_data;
	xmms_error_t err;
	const gchar *url;
	const gchar *metakey;
	gchar *command, *hash;
	guint filesize;

	g_return_val_if_fail (xform, FALSE);

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);

	g_return_val_if_fail (url, FALSE);

	data = g_new0 (xmms_daap_data_t, 1);

	xmms_error_reset (&err);

	if (!get_data_from_url (url, &(data->host), &(data->port), &command, &err)) {
		return FALSE;
	}

	hash = g_strdup_printf ("%s:%u", data->host, data->port);

	login_data = g_hash_table_lookup (login_sessions, hash);
	if (!login_data) {
		XMMS_DBG ("creating login data for %s", hash);
		login_data = g_new0 (xmms_daap_login_data_t, 1);

		login_data->request_id = 1;
		login_data->logged_in = TRUE;

		login_data->session_id = daap_command_login (data->host, data->port,
		                                             login_data->request_id,
		                                             &err);
		if (xmms_error_iserror (&err)) {
			return FALSE;
		}

		g_hash_table_insert (login_sessions, hash, login_data);
	}

	login_data->revision_id = daap_command_update (data->host, data->port,
	                                               login_data->session_id,
	                                               login_data->request_id);
	dbid_list = daap_command_db_list (data->host, data->port,
	                                  login_data->session_id,
	                                  login_data->revision_id,
	                                  login_data->request_id);
	if (!dbid_list) {
		return FALSE;
	}

	/* XXX: see XXX in the browse function above */
	dbid = ((cc_item_record_t *) dbid_list->data)->dbid;
	/* want to request a stream, but don't read the data yet */
	data->channel = daap_command_init_stream (data->host, data->port,
	                                          login_data->session_id,
	                                          login_data->revision_id,
	                                          login_data->request_id, dbid,
	                                          command, &filesize);
	if (! data->channel) {
		return FALSE;
	}
	login_data->request_id++;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	xmms_xform_metadata_set_int (xform, metakey, filesize);

	xmms_xform_private_data_set (xform, data);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);

	g_slist_foreach (dbid_list, (GFunc) cc_item_record_free, NULL);
	g_slist_free (dbid_list);
	g_free (command);

	return TRUE;
}

static void
xmms_daap_destroy (xmms_xform_t *xform)
{
	xmms_daap_data_t *data;

	data = xmms_xform_private_data_get (xform);

	g_io_channel_shutdown (data->channel, TRUE, NULL);
	g_io_channel_unref (data->channel);

	g_free (data->host);
	g_free (data);
}

static gint
xmms_daap_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error)
{
	xmms_daap_data_t *data;
	gsize read_bytes = 0;
	GIOStatus status;

	data = xmms_xform_private_data_get (xform);

	/* request is performed, header is stripped. now read the data. */
	while (read_bytes == 0) {
		status = g_io_channel_read_chars (data->channel, buffer, len,
		                                  &read_bytes, NULL);
		if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR) {
			break;
		}
	}

	return read_bytes;
}


static gboolean
xmms_daap_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error)
{
	gboolean ret = FALSE;

	if (g_strcasecmp (url, "daap://") == 0) {

		GSList *sl = daap_mdns_get_server_list ();

		for (; sl; sl = g_slist_next (sl)) {
			daap_mdns_server_t *mdns_serv;
			gchar *str;

			mdns_serv = sl->data;

			str = g_strdup_printf ("%s:%d", mdns_serv->address,
			                       mdns_serv->port);
			xmms_xform_browse_add_entry (xform, str, XMMS_XFORM_BROWSE_FLAG_DIR);
			g_free (str);

			xmms_xform_browse_add_entry_property_str (xform, "servername",
			                                          mdns_serv->server_name);

			xmms_xform_browse_add_entry_property_str (xform, "ip",
			                                          mdns_serv->address);

			xmms_xform_browse_add_entry_property_str (xform, "name",
			                                          mdns_serv->mdns_hostname);

			xmms_xform_browse_add_entry_property_int (xform, "port",
			                                          mdns_serv->port);

			/* TODO implement the machinery to allow for this */
			// val = xmms_object_cmd_value_int_new (mdns_serv->need_auth);
			// xmms_xform_browse_add_entry_property (xform, "passworded", val);
			// val = xmms_object_cmd_value_int_new (mdns_serv->version);
			// xmms_xform_browse_add_entry_property (xform, "version", val);
		}

		ret = TRUE;

		g_slist_free (sl);
	} else {
		gchar *host;
		guint port;

		if (get_data_from_url (url, &host, &port, NULL, error)) {
			ret = daap_get_urls_from_server (xform, host, port, error);
			g_free (host);
		}
	}

	return ret;
}
