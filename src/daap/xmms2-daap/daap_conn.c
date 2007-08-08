/** @file daap_conn.c
 *  Manages the connection to a DAAP server.
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>






#include "cc_handlers.h"
#include "daap_md5.h"
#include "daap_conn.h"
#include "daap_util.h"


    GIOChannel *
daap_open_connection (gchar *host, gint port)
{
    gint ai_status;
    gint sockfd;
    struct sockaddr_in server;
    struct addrinfo *ai_hint, *ai_result;
    GIOChannel *sock_chan;
    GError *err = NULL;

    sockfd = socket (AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return NULL;
    }

    sock_chan = g_io_channel_unix_new (sockfd);
    if (!g_io_channel_get_close_on_unref (sock_chan)) {
        g_io_channel_set_close_on_unref (sock_chan, TRUE);
    }

    g_io_channel_set_flags (sock_chan, G_IO_FLAG_NONBLOCK, &err);
    if (NULL != err) {
        g_print ("Error setting nonblock flag: %s\n", err->message);
        g_io_channel_unref (sock_chan);
        return NULL;
    }

    /* call getaddrinfo() to convert a hostname to ip */

    ai_hint = g_new0 (struct addrinfo, 1);
    /* FIXME sometime in the future, we probably want to append
     *       " | {A,P}F_INET6" for IPv6 support */
    ai_hint->ai_family = AF_INET;
    ai_hint->ai_protocol = PF_INET;

    while ((ai_status = getaddrinfo (host, NULL, ai_hint, &ai_result))) {
        if (ai_status != EAI_AGAIN) {
            g_print ("Error with getaddrinfo(): %s", gai_strerror (ai_status));
            g_io_channel_unref (sock_chan);
            return NULL;
        }
    }

    memset (&server, 0, sizeof (struct sockaddr_in));

    server.sin_addr = ((struct sockaddr_in *) ai_result->ai_addr)->sin_addr;
    server.sin_family = AF_INET;
    server.sin_port = htons (port);

    g_free (ai_hint);
    freeaddrinfo (ai_result);

    while (42) {
        fd_set fds;
        struct timeval tmout;
        gint sret;
        gint err = 0;
        guint errsize = sizeof (err);

        tmout.tv_sec = 3;
        tmout.tv_usec = 0;

        sret = connect (sockfd,
                (struct sockaddr *) &server,
                sizeof (struct sockaddr_in));

        if (sret == 0) {
            break;
        } else if (sret == -1 && errno != EINPROGRESS) {
            g_print ("connect says: %s", strerror (errno));
            g_io_channel_unref (sock_chan);
            return NULL;
        }

        FD_ZERO (&fds);
        FD_SET (sockfd, &fds);

		sret = select (sockfd + 1, NULL, &fds, NULL, &tmout);
		if (sret <= 0 ) {
			g_io_channel_unref (sock_chan);
			return NULL;
		}

		/** Haha, lol lol ololo sockets in POSIX */
		if (getsockopt (sockfd, SOL_SOCKET, SO_ERROR, &err, &errsize) < 0) {
			g_io_channel_unref (sock_chan);
			return NULL;
		}

		if (err != 0) {
			g_print ("Connect call failed!");
			g_io_channel_unref (sock_chan);
			return NULL;
		}

		if (FD_ISSET (sockfd, &fds)) {
			break;
		}
	}

	g_io_channel_set_encoding (sock_chan, NULL, &err);
	if (NULL != err) {
		g_print ("Error setting encoding: %s\n", err->message);
		g_io_channel_unref (sock_chan);
		return NULL;
	}

	return sock_chan;
}

gchar *
daap_generate_request (const gchar *path, gchar *host, gint request_id)
{
	gchar *req;
	gint8 hash[33];

	memset (hash, 0, 33);

	daap_hash_generate (DAAP_VERSION, (guchar *) path, 2, (guchar *) hash,
	                    request_id);

	req = g_strdup_printf ("GET %s %s\r\n"
	                       "Host: %s\r\n"
	                       "Accept: */*\r\n"
	                       "User-Agent: %s\r\n"
	                       "Accept-Language: en-us, en;q=5.0\r\n"
	                       "Client-DAAP-Access-Index: 2\r\n"
	                       "Client-DAAP-Version: 3.0\r\n"
	                       "Client-DAAP-Validation: %s\r\n"
	                       "Client-DAAP-Request-ID: %d\r\n"
	                       "Connection: close\r\n"
	                       "\r\n",
	                       path, HTTP_VER_STRING, host,
	                       USER_AGENT, hash, request_id);
	return req;
}

void
daap_send_request (GIOChannel *sock_chan, gchar *request)
{
	gint n_bytes_to_send;

	n_bytes_to_send = strlen (request);

	write_buffer_to_channel (sock_chan, request, n_bytes_to_send);
}

void
daap_receive_header (GIOChannel *sock_chan, gchar **header)
{
	guint n_total_bytes_recvd = 0;
	gsize linelen;
	gchar *response, *recv_line;
	GIOStatus io_stat;
	GError *err = NULL;

	if (NULL != header) {
		*header = NULL;
	}

	response = (gchar *) g_malloc0 (sizeof (gchar) * MAX_HEADER_LENGTH);
	if (NULL == response) {
		g_print ("Error: couldn't allocate memory for response.\n");
		return;
	}

	/* read data from the io channel one line at a time, looking for
	 * the end of the header */
	do {
		io_stat = g_io_channel_read_line (sock_chan, &recv_line, &linelen,
		                                  NULL, &err);
		if (io_stat == G_IO_STATUS_ERROR) {
			g_print ("Error reading from channel: %s\n", err->message);
			break;
		}

		if (NULL != recv_line) {
			memcpy (response+n_total_bytes_recvd, recv_line, linelen);
			n_total_bytes_recvd += linelen;

			if (strcmp (recv_line, "\r\n") == 0) {
				g_free (recv_line);
				if (NULL != header) {
					*header = (gchar *) g_malloc0 (sizeof (gchar) *
					                               n_total_bytes_recvd);
					if (NULL == *header) {
						g_print ("error: couldn't allocate header\n");
						break;
					}
					memcpy (*header, response, n_total_bytes_recvd);
				}
				break;
			}

			g_free (recv_line);
		}

		if (io_stat == G_IO_STATUS_EOF) {
			break;
		}

		if (n_total_bytes_recvd >= MAX_HEADER_LENGTH) {
			g_print ("Warning: Maximum header size reached without finding "
			          "end of header; bailing.\n");
			break;
		}
	} while (TRUE);

	g_free (response);

	if (sock_chan) {
		g_io_channel_flush (sock_chan, &err);
		if (NULL != err) {
			g_print ("Error flushing buffer: %s\n", err->message);
			return;
		}
	}
}

cc_data_t *
daap_handle_data (GIOChannel *sock_chan, gchar *header)
{
	cc_data_t * retval;
	gint response_length;
	gchar *response_data;

	response_length = get_data_length (header);

	if (BAD_CONTENT_LENGTH == response_length) {
		g_print ("warning: Header does not contain a \""CONTENT_LENGTH
		          "\" parameter.\n");
		return NULL;
	} else if (0 == response_length) {
		g_print ("warning: "CONTENT_LENGTH" is zero, most likely the result of "
		          "a bad request.\n");
		return NULL;
	}

	response_data = (gchar *) g_malloc0 (sizeof (gchar) * response_length);
	if (NULL == response_data) {
		g_print ("error: could not allocate response memory\n");
		return NULL;
	}

	read_buffer_from_channel (sock_chan, response_data, response_length);

	retval = cc_handler (response_data, response_length);
	g_free (response_data);

	return retval;
}

gint
get_data_length (gchar *header)
{
	gint len;
	gchar *content_length;

	content_length = strstr (header, CONTENT_LENGTH);
	if (NULL == content_length) {
		len = BAD_CONTENT_LENGTH;
	} else {
		content_length += strlen (CONTENT_LENGTH);
		len = atoi (content_length);
	}

	return len;
}

gint
get_server_status (gchar *header)
{
	gint status;
	gchar *server_status;

	server_status = strstr (header, HTTP_VER_STRING);
	if (NULL == server_status) {
		status = UNKNOWN_SERVER_STATUS;
	} else {
		server_status += strlen (HTTP_VER_STRING" ");
		status = atoi (server_status);
	}

	return status;
}

