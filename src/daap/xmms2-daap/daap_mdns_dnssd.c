/** XMMS2 transform for accessing DAAP music shares.
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
#ifdef DAAP_MDNS_DNSSD

#include <glib.h>
#include <dns_sd.h>
#include <string.h>

#include "daap_mdns_browse.h"

typedef struct GMDNS_t GMDNS;
typedef struct GMDNSServer_t GMDNSServer;
typedef struct GMDNSUserData_t GMDNSUserData;

typedef void (*GMDNSFunc)(GMDNS *, gint, GMDNSServer *, gpointer);

struct GMDNS_t {
	GMutex *mutex;
	GSList *service_list;

	GMDNSFunc callback;
	gpointer user_data;
	GMDNSUserData *browse_ud;
};

struct GMDNSUserData_t {
	GMDNS *mdns;
	GMDNSServer *server;
	GPollFD *fd;
	GSource *source;

	DNSServiceRef client;
};

struct GMDNSServer_t {
	gchar *mdnsname;
	gchar *hostname;
	gchar *address;
	GHashTable *txtvalues;
	guint16 port;
};

enum {
	G_MDNS_SERVER_ADD,
	G_MDNS_SERVER_REMOVE
};

static void g_mdns_user_data_destroy (GMDNSUserData *ud);
static void g_mdns_server_destroy (GMDNSServer *server);
static gboolean g_mdns_poll_add (GMDNS *mdns, GMDNSUserData *ud, DNSServiceRef client);

static GMDNS *g_mdns;

static void
qr_reply (DNSServiceRef sdRef,
          DNSServiceFlags flags,
          uint32_t ifIndex,
          DNSServiceErrorType errorCode,
          const char *fullname,
          uint16_t rrtype,
          uint16_t rrclass,
          uint16_t rdlen,
          const void *rdata,
          uint32_t ttl,
          void *context)
{
	GMDNSUserData *ud = context;
	gchar addr[1000];
	const guchar *rd = (guchar *)rdata;

	g_return_if_fail (ud);
	g_return_if_fail (rrtype == kDNSServiceType_A);

	g_snprintf (addr, 1000, "%d.%d.%d.%d", rd[0], rd[1], rd[2], rd[3]);

	ud->server->address = g_strdup (addr);

	g_print ("adding server %s %s", ud->server->mdnsname, ud->server->address);
	g_mutex_lock (ud->mdns->mutex);
	ud->mdns->service_list = g_slist_prepend (ud->mdns->service_list, ud->server);
	g_mutex_unlock (ud->mdns->mutex);

	if (ud->mdns->callback) {
		ud->mdns->callback (ud->mdns, G_MDNS_SERVER_ADD, ud->server, ud->mdns->user_data);
	}
	g_mdns_user_data_destroy (ud);
}


static void
resolve_reply (DNSServiceRef client,
               DNSServiceFlags flags,
               uint32_t ifIndex,
               DNSServiceErrorType errorCode,
               const char *fullname,
               const char *hosttarget,
               uint16_t opaqueport,
               uint16_t txtLen,
               const char *txtRecord,
               void *context)
{
	GMDNSUserData *ud = context;
	GMDNSUserData *ud2;
	DNSServiceErrorType err;
	gint i;
	union { guint16 s; guchar b[2]; } portu = { opaqueport };

	g_return_if_fail (ud);

	ud->server->port = ((guint16) portu.b[0]) << 8 | portu.b[1];
	ud->server->hostname = g_strdup (hosttarget);
	ud->server->txtvalues = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                               g_free, g_free);

	for (i = 0; i < TXTRecordGetCount (txtLen, txtRecord); i++) {
		gchar key[256];
		const void *txt_value;
		gchar *value;
		guint8 vallen;

		err = TXTRecordGetItemAtIndex (txtLen, txtRecord, i, 256, key, &vallen, &txt_value);
		if (err != kDNSServiceErr_NoError) {
			g_warning ("error parsing TXT records!");
		}

		value = g_malloc (vallen + 1);
		g_strlcpy (value, txt_value, vallen + 1);
		g_hash_table_insert (ud->server->txtvalues, g_strdup (key), value);
	}

	ud2 = g_new0 (GMDNSUserData, 1);

	err = DNSServiceQueryRecord (&ud2->client, 0,
	                             kDNSServiceInterfaceIndexAny,
	                             ud->server->hostname,
	                             kDNSServiceType_A,
	                             kDNSServiceClass_IN,
	                             qr_reply, ud2);

	if (err != kDNSServiceErr_NoError) {
		g_warning ("Error from QueryRecord!");
	}

	g_mdns_poll_add (ud->mdns, ud2, ud2->client);
	ud2->server = ud->server;

	g_mdns_user_data_destroy (ud);
}


static void
browse_reply (DNSServiceRef client,
              DNSServiceFlags flags,
              uint32_t ifIndex,
              DNSServiceErrorType errorCode,
              const char *replyName,
              const char *replyType,
              const char *replyDomain,
              void *context)
{
	DNSServiceErrorType err;
	GMDNSServer *server;
	GMDNSUserData *ud = context;
	GMDNSUserData *ud2;
	gboolean remove = (flags & kDNSServiceFlagsAdd) ? FALSE : TRUE;

	if (!remove) {
		server = g_new0 (GMDNSServer, 1);
		server->mdnsname = g_strdup (replyName);
		ud2 = g_new0 (GMDNSUserData, 1);
		err = DNSServiceResolve (&ud2->client, 0, kDNSServiceInterfaceIndexAny,
		                         server->mdnsname,
		                         "_daap._tcp", "local",
		                         (DNSServiceResolveReply)resolve_reply, ud2);

		if (err != kDNSServiceErr_NoError) {
			g_warning ("Couldn't do ServiceResolv");
			g_free (server->mdnsname);
			g_free (server);
			return;
		}

		ud2->server = server;

		g_mdns_poll_add (ud->mdns, ud2, ud2->client);
	} else {
		GSList *n, *nxt;
		g_mutex_lock (ud->mdns->mutex);
		for (n = ud->mdns->service_list; n; n = nxt) {
			nxt = g_slist_next (n);
			GMDNSServer *server = n->data;
			if (strcmp (server->mdnsname, replyName) == 0) {
				n = ud->mdns->service_list = g_slist_remove (ud->mdns->service_list, server);
				g_mutex_unlock (ud->mdns->mutex);
				if (ud->mdns->callback)
					ud->mdns->callback (ud->mdns, G_MDNS_SERVER_REMOVE, server, ud->mdns->user_data);
				g_mdns_server_destroy (server);
				g_mutex_lock (ud->mdns->mutex);
			}
		}
		g_mutex_unlock (ud->mdns->mutex);
	}

}

static void
g_mdns_server_destroy (GMDNSServer *server)
{
	g_return_if_fail (server);
	if (server->hostname)
		g_free (server->hostname);
	if (server->mdnsname)
		g_free (server->mdnsname);
	if (server->address)
		g_free (server->address);
	if (server->txtvalues)
		g_hash_table_destroy (server->txtvalues);

	g_free (server);
}

static gboolean
g_mdns_source_prepare (GSource *source, gint *timeout_)
{
	/* No timeout here */
	return FALSE;
}

static gboolean
g_mdns_source_check (GSource *source)
{
	/* Maybe check for errors here? */
	return TRUE;
}

static gboolean
g_mdns_source_dispatch (GSource *source,
                        GSourceFunc callback,
                        gpointer user_data)
{
	GMDNSUserData *ud = user_data;
	DNSServiceErrorType err;

	if ((ud->fd->revents & G_IO_ERR) || (ud->fd->revents & G_IO_HUP)) {
		return FALSE;
	} else if (ud->fd->revents & G_IO_IN) {
		err = DNSServiceProcessResult (ud->client);
		if (err != kDNSServiceErr_NoError) {
			g_warning ("DNSServiceProcessResult returned error");
			return FALSE;
		}
	}

	return TRUE;
}

static GSourceFuncs g_mdns_poll_funcs = {
	g_mdns_source_prepare,
	g_mdns_source_check,
	g_mdns_source_dispatch,
	NULL
};

static void
g_mdns_user_data_destroy (GMDNSUserData *ud)
{
	g_return_if_fail (ud);

	g_source_remove_poll (ud->source, ud->fd);
	g_free (ud->fd);
	g_source_destroy (ud->source);
	DNSServiceRefDeallocate (ud->client);
	g_free (ud);
}

static gboolean
g_mdns_poll_add (GMDNS *mdns, GMDNSUserData *ud, DNSServiceRef client)
{
	ud->fd = g_new0 (GPollFD, 1);
	ud->fd->fd = DNSServiceRefSockFD (client);
	ud->client = client;
	ud->mdns = mdns;

	if (ud->fd->fd == -1) {
		g_free (ud->fd);
		g_free (ud);
		return FALSE;
	}

	ud->fd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;

	ud->source = g_source_new (&g_mdns_poll_funcs, sizeof (GSource));
	g_source_set_callback (ud->source,
	                       (GSourceFunc) g_mdns_source_dispatch,
	                       ud, NULL);
	g_source_add_poll (ud->source, ud->fd);
	g_source_attach (ud->source, NULL);

	return TRUE;
}

/**
 * Browse for a service. The callback will be called
 * when it's fully resloved and queried
 */
static gboolean
g_mdns_browse (GMDNS *mdns,
               gchar *service,
               GMDNSFunc callback,
               gpointer user_data)
{
	DNSServiceErrorType err;
	DNSServiceRef client;
	GMDNSUserData *ud;

	g_return_val_if_fail (!mdns->browse_ud, FALSE);

	ud = g_new0 (GMDNSUserData, 1);

	err = DNSServiceBrowse (&client, 0, kDNSServiceInterfaceIndexAny,
	                        service, 0, browse_reply, ud);

	if (err != kDNSServiceErr_NoError) {
		g_warning ("Couldn't setup mDNS poller, error = %d",err);
		return FALSE;
	}

	g_mdns_poll_add (mdns, ud, client);

	mdns->callback = callback;
	mdns->user_data = user_data;
	mdns->browse_ud = ud;

	return TRUE;
}

/**
 * Remove updates for browsing. Make sure to
 * call this before you initialize a new browsing
 */
static gboolean
g_mdns_stop_browsing (GMDNS *mdns)
{
	g_return_val_if_fail (mdns, FALSE);

	g_mdns_user_data_destroy (mdns->browse_ud);
	mdns->callback = NULL;
	mdns->user_data = NULL;

	return TRUE;
}

/**
 * Return the full list of services
 * the list is threadsafe but not the entries
 * so it might be removed while you using it.
 */
GSList *
daap_mdns_get_server_list ()
{
	GSList *ret=NULL, *n;
	daap_mdns_server_t *server;

	g_mutex_lock (g_mdns->mutex);
	for (n = g_mdns->service_list; n; n = g_slist_next (n)) {
		GMDNSServer *s = n->data;
		server = g_new0 (daap_mdns_server_t, 1);
		server->mdns_hostname = s->mdnsname;
		server->server_name = s->hostname;
		server->port = s->port;
		server->address = s->address;
		ret = g_slist_append (ret, server);
	}
	g_mutex_unlock (g_mdns->mutex);

	return ret;
}

/**
 * Free resources held by GMDNS
 */
void
daap_mdns_destroy ()
{
	GSList *n;
	g_return_if_fail (g_mdns);

	g_mdns_stop_browsing (g_mdns);

	g_mutex_lock (g_mdns->mutex);
	for (n = g_mdns->service_list; n; n = g_slist_next (n)) {
		g_mdns_server_destroy (n->data);
	}
	g_mutex_unlock (g_mdns->mutex);
	g_mutex_free (g_mdns->mutex);

	g_free (g_mdns);
}

gboolean
daap_mdns_initialize ()
{
	g_mdns = g_new0 (GMDNS, 1);
	g_mdns->mutex = g_mutex_new ();
	return g_mdns_browse (g_mdns, "_daap._tcp", NULL, NULL);
}
#endif
