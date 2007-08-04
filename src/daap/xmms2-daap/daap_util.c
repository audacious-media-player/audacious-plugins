/** @file daap_util.c
 *  Miscellaneous utility functions.
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

#include <glib.h>
#include <glib/gprintf.h>

#include "daap_util.h"

void
write_buffer_to_channel (GIOChannel *chan, gchar *buf, gint bufsize)
{
	guint total_sent_bytes = 0;
	gsize sent_bytes;
	GIOStatus io_stat;
	GError *err = NULL;

	do {
		io_stat = g_io_channel_write_chars (chan,
		                                    buf + total_sent_bytes,
		                                    bufsize - total_sent_bytes,
		                                    &sent_bytes,
		                                    &err);
		if (io_stat == G_IO_STATUS_ERROR) {
			if (NULL != err) {
				g_print ("Error writing to channel: %s\n", err->message);
			}
			break;
		}

		bufsize -= sent_bytes;
		total_sent_bytes += sent_bytes;
	} while (bufsize > 0);

	g_io_channel_flush (chan, &err);
	if (NULL != err) {
		g_print ("warning: error flushing channel: %s\n", err->message);
	}
}

gint
read_buffer_from_channel (GIOChannel *chan, gchar *buf, gint bufsize)
{
	guint n_total_bytes_read = 0;
	gsize read_bytes;
	GIOStatus io_stat;
	GError *err = NULL;

	do {
		io_stat = g_io_channel_read_chars (chan,
		                                   buf + n_total_bytes_read,
		                                   bufsize - n_total_bytes_read,
		                                   &read_bytes,
		                                   &err);
		if (io_stat == G_IO_STATUS_ERROR) {
			g_print ("warning: error reading from channel: %s\n", err->message);
		}
		n_total_bytes_read += read_bytes;

		if (io_stat == G_IO_STATUS_EOF) {
			break;
		}
	} while (bufsize > n_total_bytes_read);

	return n_total_bytes_read;
}

