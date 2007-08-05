/** @file daap_mdns_browse.h
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

#ifndef DAAP_MDNS_BROWSE_H
#define DAAP_MDNS_BROWSE_H

#include <glib.h>

typedef struct {
	gchar *server_name;
	gchar *address;
	gchar *mdns_hostname;
	guint16 port;
} daap_mdns_server_t;

gboolean
daap_mdns_initialize ();

GSList *
daap_mdns_get_server_list ();

void
daap_mdns_destroy ();

#endif
