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
#ifdef AVAHI_MDNS_DUMMY
#include <glib.h>

#include "daap_mdns_browse.h"

gboolean
daap_mdns_initialize ()
{
	return FALSE;
}

GSList *
daap_mdns_get_server_list ()
{
	return NULL;
}

void
daap_mdns_destroy ()
{
}
#endif
