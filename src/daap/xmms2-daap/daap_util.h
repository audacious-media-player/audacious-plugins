/** @file daap_util.h
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

#ifndef DAAP_UTIL_H
#define DAAP_UTIL_H

gint
read_buffer_from_channel (GIOChannel *chan, gchar *buf, gint bufsize);

void
write_buffer_to_channel (GIOChannel *chan, gchar *buf, gint bufsize);

#endif
