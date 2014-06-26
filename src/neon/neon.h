/*
 *  A neon HTTP input plugin for Audacious
 *  Copyright (C) 2007 Ralf Ertzinger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _NEON_PLUGIN_H
#define _NEON_PLUGIN_H

#include <glib.h>
#include <pthread.h>

#include <libaudcore/vfs.h>
#include <ne_session.h>
#include <ne_request.h>
#include <ne_uri.h>

#include "rb.h"

typedef enum
{
    NEON_READER_INIT=0,
    NEON_READER_RUN=1,
    NEON_READER_ERROR,
    NEON_READER_EOF,
    NEON_READER_TERM
} neon_reader_t;

struct reader_status
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool reading;
    neon_reader_t status;
};

struct icy_metadata
{
    char * stream_name;
    char * stream_title;
    char * stream_url;
    char * stream_contenttype;
    int   stream_bitrate;
};

struct neon_handle
{
    char * url;                        /* The URL, as passed to us */
    ne_uri * purl;                      /* The URL, parsed into a structure */
    struct ringbuf rb;                  /* Ringbuffer for our data */
    unsigned char redircount;                  /* Redirect count for the opened URL */
    long pos;                           /* Current position in the stream (number of last byte delivered to the player) */
    unsigned long content_start;               /* Start position in the stream */
    long content_length;                /* Total content length, counting from content_start, if known. -1 if unknown */
    bool can_ranges;                /* TRUE if the webserver advertised accept-range: bytes */
    unsigned long icy_metaint;                 /* Interval in which the server will send metadata announcements. 0 if no announcments */
    unsigned long icy_metaleft;                /* Bytes left until the next metadata block */
    struct icy_metadata icy_metadata;   /* Current ICY metadata */
    ne_session * session;
    ne_request * request;
    pthread_t reader;
    struct reader_status reader_status;
    bool eof;
};

#endif
