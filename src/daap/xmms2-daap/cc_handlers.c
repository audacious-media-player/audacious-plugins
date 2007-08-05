/** @file cc_handlers.c
 *  Functions for parsing DAAP content code data.
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

#define _GNU_SOURCE
#include <string.h>

#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "cc_handlers.h"
#include "daap_conn.h"

#define DMAP_BYTES_REMAINING ((gint) (data_end - current_data))

static void
endian_swap_int16 (gint16 *i)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	gint16 tmp;
	tmp = (gint16) (((*i >>  8) & 0x00FF) |
	                ((*i <<  8) & 0xFF00));
	*i = tmp;
#endif
}

static void
endian_swap_int32 (gint32 *i)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	gint32 tmp;
	tmp = (gint32) (((*i >> 24) & 0x000000FF) |
	                ((*i >>  8) & 0x0000FF00) |
	                ((*i <<  8) & 0x00FF0000) |
	                ((*i << 24) & 0xFF000000));
	*i = tmp;
#endif
}

static void
endian_swap_int64 (gint64 *i)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	gint64 tmp;
	tmp = (gint64) (                 ((*i >> 40) & 0x00000000000000FF) |
	                                 ((*i >> 32) & 0x000000000000FF00) |
	                                 ((*i >> 24) & 0x0000000000FF0000) |
	                                 ((*i >>  8) & 0x00000000FF000000) |
	               G_GINT64_CONSTANT ((*i <<  8) & 0x000000FF00000000) |
	               G_GINT64_CONSTANT ((*i << 24) & 0x0000FF0000000000) |
	               G_GINT64_CONSTANT ((*i << 32) & 0x00FF000000000000) |
	               G_GINT64_CONSTANT ((*i << 40) & 0xFF00000000000000));
	*i = tmp;
#endif
}

static gint
grab_data_string (gchar **container, gchar *data, gint str_len)
{
	gint offset = 0;

	if (0 != str_len) {
		*container = (gchar *) malloc (sizeof (gchar) * (str_len+1));

		memcpy (*container, data + DMAP_CC_SZ + DMAP_INT_SZ, str_len);
		(*container)[str_len] = '\0';

		offset += str_len;
	}

	return offset;
}

static gint
grab_data_version (gint16 *cont_upper, gint16 *cont_lower, gchar *data)
{
	gint offset = DMAP_CC_SZ;

	memcpy (cont_lower, data + offset, DMAP_INT_SZ);
	endian_swap_int16 (cont_lower);
	offset += DMAP_INT_SZ;

	memcpy (cont_upper, data + offset, DMAP_INT_SZ);
	endian_swap_int16 (cont_upper);
	offset += DMAP_INT_SZ;

	return offset;
}

static gint
grab_data (void *container, gchar *data, content_type ct)
{
	gint offset;
	gint data_size;

	offset = DMAP_CC_SZ;
	memcpy (&data_size, data + offset, DMAP_INT_SZ);
	endian_swap_int32 ((gint32 *)&data_size);
	offset += DMAP_INT_SZ;

	switch (ct) {
		case DMAP_CTYPE_BYTE:
		case DMAP_CTYPE_UNSIGNEDBYTE:
			memcpy (container, data + offset, DMAP_BYTE_SZ);
			offset += DMAP_BYTE_SZ;
			break;
		case DMAP_CTYPE_SHORT:
		case DMAP_CTYPE_UNSIGNEDSHORT:
			memcpy (container, data + offset, DMAP_SHORT_SZ);
			endian_swap_int16 (container);
			offset += DMAP_SHORT_SZ;
			break;
		case DMAP_CTYPE_INT:
		case DMAP_CTYPE_UNSIGNEDINT:
			memcpy (container, data + offset, DMAP_INT_SZ);
			endian_swap_int32 (container);
			offset += DMAP_INT_SZ;
			break;
		case DMAP_CTYPE_LONG:
		case DMAP_CTYPE_UNSIGNEDLONG:
			memcpy (container, data + offset, DMAP_LONG_SZ);
			endian_swap_int64 (container);
			offset += DMAP_LONG_SZ;
			break;
		case DMAP_CTYPE_STRING:
			offset += grab_data_string ((gchar **) container, data, data_size);
			break;
		case DMAP_CTYPE_DATE:
			memcpy (container, data + offset, DMAP_INT_SZ);
			endian_swap_int32 (container);
			offset += DMAP_INT_SZ;
			break;
		default:
			g_print ("Warning: Unrecognized content type (%d).\n", ct);
			break;
	}

	return offset;
}

static gint
cc_handler_mtco (cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data (&(fields->n_rec_matches), current_data, DMAP_CTYPE_INT);
	return offset;
}

static gint
cc_handler_mrco (cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data (&(fields->n_ret_items), current_data, DMAP_CTYPE_INT);
	return offset;
}

static gint
cc_handler_muty (cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data (&(fields->updt_type), current_data, DMAP_CTYPE_BYTE);
	return offset;
}

static gint
cc_handler_mstt (cc_data_t *fields, gchar *current_data)
{
	gint offset = grab_data (&(fields->status), current_data, DMAP_CTYPE_INT);
	return offset;
}

static gint
cc_handler_mlit (cc_data_t *fields, gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_item_record_t *item_fields;

	current_data = data + 8;
	data_end = data + data_len;

	item_fields = g_malloc0 (sizeof (cc_item_record_t));

	while (current_data < data_end && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','i','i','d'):
				offset += grab_data (&(item_fields->dbid), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('m','p','e','r'):
				offset += grab_data (&(item_fields->persistent_id), current_data,
				                    DMAP_CTYPE_LONG);
				break;
			case CC_TO_INT ('m','i','n','m'):
				offset += grab_data (&(item_fields->iname), current_data,
				                    DMAP_CTYPE_STRING);
				break;

			/* song list specific */
			case CC_TO_INT ('m','i','k','d'):
				offset += grab_data (&(item_fields->item_kind), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('a','s','d','k'):
				offset += grab_data (&(item_fields->song_data_kind), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('a','s','u','l'):
				offset += grab_data (&(item_fields->song_data_url), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','a','l'):
				offset += grab_data (&(item_fields->song_data_album),
				                    current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','a','r'):
				offset += grab_data (&(item_fields->song_data_artist),
				                    current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','b','r'):
				offset += grab_data (&(item_fields->song_bitrate), current_data,
				                    DMAP_CTYPE_SHORT);
				break;
			case CC_TO_INT ('a','s','c','m'):
				offset += grab_data (&(item_fields->song_comment), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','d','a'):
				offset += grab_data (&(item_fields->song_date), current_data,
				                    DMAP_CTYPE_DATE);
				break;
			case CC_TO_INT ('a','s','d','m'):
				offset += grab_data (&(item_fields->song_date_mod), current_data,
				                    DMAP_CTYPE_DATE);
				break;
			case CC_TO_INT ('a','s','g','n'):
				offset += grab_data (&(item_fields->song_genre), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','f','m'):
				offset += grab_data (&(item_fields->song_format), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','d','t'):
				offset += grab_data (&(item_fields->song_description), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','s','r'):
				offset += grab_data (&(item_fields->sample_rate), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('a','s','s','z'):
				offset += grab_data (&(item_fields->song_size), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('a','s','s','t'):
				offset += grab_data (&(item_fields->song_start_time), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('a','s','s','p'):
				offset += grab_data (&(item_fields->song_stop_time), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('a','s','t','m'):
				offset += grab_data (&(item_fields->song_total_time), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('a','s','y','r'):
				offset += grab_data (&(item_fields->song_year), current_data,
				                    DMAP_CTYPE_SHORT);
				break;
			case CC_TO_INT ('a','s','t','n'):
				offset += grab_data (&(item_fields->song_track_no), current_data,
				                    DMAP_CTYPE_SHORT);
				break;
			case CC_TO_INT ('a','s','c','p'):
				offset += grab_data (&(item_fields->song_composer), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('a','s','t','c'):
				offset += grab_data (&(item_fields->song_track_count), current_data,
				                    DMAP_CTYPE_SHORT);
				break;
			case CC_TO_INT ('a','s','d','c'):
				offset += grab_data (&(item_fields->song_disc_count), current_data,
				                    DMAP_CTYPE_SHORT);
				break;
			case CC_TO_INT ('a','s','d','n'):
				offset += grab_data (&(item_fields->song_disc_no), current_data,
				                    DMAP_CTYPE_SHORT);
				break;
			case CC_TO_INT ('a','s','c','o'):
				offset += grab_data (&(item_fields->song_compilation), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('a','s','b','t'):
				offset += grab_data (&(item_fields->song_bpm), current_data,
				                    DMAP_CTYPE_SHORT);
				break;
			case CC_TO_INT ('a','g','r','p'):
				offset += grab_data (&(item_fields->song_grouping), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			case CC_TO_INT ('m','c','t','i'):
				offset += grab_data (&(item_fields->container_id), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('m','u','d','l'):
				offset += grab_data (&(item_fields->deleted_id), current_data,
				                    DMAP_CTYPE_INT);
				break;

			/* db list specific */
			case CC_TO_INT ('m','i','m','c'):
				offset += grab_data (&(item_fields->db_n_items), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('m','c','t','c'):
				offset += grab_data (&(item_fields->db_n_playlist), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('a','e','S','P'):
				offset += grab_data (&(item_fields->is_smart_pl), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('a','b','p','l'):
				offset += grab_data (&(item_fields->is_base_pl), current_data,
				                    DMAP_CTYPE_BYTE);
				break;

			/* exit conditions */
			case CC_TO_INT ('m','l','i','t'):
				do_break = TRUE;
				break;
			default:
				g_print ("Warning: Unrecognized content code "
				          "or end of data: %s\n", current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	fields->record_list = g_slist_prepend (fields->record_list, item_fields);

	return (gint) (current_data - data);
}

static gint
cc_handler_mlcl (cc_data_t *fields, gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	current_data = data + 8;
	data_end = data + data_len;

	while (current_data < data_end && !do_break) {
		if (CC_TO_INT (current_data[0], current_data[1],
		               current_data[2], current_data[3]) ==
		    CC_TO_INT ('m','l','i','t')) {

			offset += cc_handler_mlit (fields, current_data,
			                           DMAP_BYTES_REMAINING);
		} else {
			break;
		}

		current_data += offset;
		offset = 0;
	}

	return (gint)(current_data - data);
}

static cc_data_t *
cc_handler_adbs (gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_data_t *fields;

	current_data = data + 8;
	data_end = data + data_len;

	fields = cc_data_new ();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','s','t','t'):
				offset += cc_handler_mstt (fields, current_data);
				break;
			case CC_TO_INT ('m','u','t','y'):
				offset += cc_handler_muty (fields, current_data);
				break;
			case CC_TO_INT ('m','t','c','o'):
				offset += cc_handler_mtco (fields, current_data);
				break;
			case CC_TO_INT ('m','r','c','o'):
				offset += cc_handler_mrco (fields, current_data);
				break;
			case CC_TO_INT ('m','l','c','l'):
				offset += cc_handler_mlcl (fields, current_data,
				                           DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return fields;
}

static cc_data_t *
cc_handler_msrv (gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_data_t *fields;
	current_data = data + 8;
	data_end = data + data_len;

	fields = cc_data_new ();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','s','t','t'):
				offset += cc_handler_mstt (fields, current_data);
				break;
			case CC_TO_INT ('a','p','r','o'):
				offset += grab_data_version (&(fields->daap_proto_major),
				                            &(fields->daap_proto_minor),
				                            current_data);
				break;
			case CC_TO_INT ('m','p','r','o'):
				offset += grab_data_version (&(fields->dmap_proto_major),
				                            &(fields->dmap_proto_minor),
				                            current_data);
				break;
			case CC_TO_INT ('m','s','t','m'):
				offset += grab_data (&(fields->timeout_interval), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('m','s','i','x'):
				offset += grab_data (&(fields->has_indexing), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','e','x'):
				offset += grab_data (&(fields->has_extensions), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','u','p'):
				offset += grab_data (&(fields->has_update), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','a','l'):
				offset += grab_data (&(fields->has_autologout), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','l','r'):
				offset += grab_data (&(fields->login_required), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','q','y'):
				offset += grab_data (&(fields->has_queries), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','r','s'):
				offset += grab_data (&(fields->has_resolve), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','b','r'):
				offset += grab_data (&(fields->has_browsing), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','p','i'):
				offset += grab_data (&(fields->has_persistent), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','a','s'):
				offset += grab_data (&(fields->auth_type), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('m','s','a','u'):
				offset += grab_data (&(fields->auth_method), current_data,
				                    DMAP_CTYPE_BYTE);
				break;
			case CC_TO_INT ('a','e','S','V'):
				offset += grab_data (&(fields->sharing_version), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('m','s','d','c'):
				offset += grab_data (&(fields->db_count), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('m','i','n','m'):
				offset += grab_data (&(fields->server_name), current_data,
				                    DMAP_CTYPE_STRING);
				break;
			default:
				g_print ("Warning: Unrecognized content code "
				          "or end of data: %s\n",
				          current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return fields;
}

static cc_data_t *
cc_handler_mccr (gchar *data, gint data_len)
{
	/* not implemented */
	return NULL;
}

static cc_data_t *
cc_handler_mlog (gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_data_t *fields;

	current_data = data + 8;
	data_end = data + data_len;

	fields = cc_data_new ();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','s','t','t'):
				offset += cc_handler_mstt (fields, current_data);
				break;
			case CC_TO_INT ('m','l','i','d'):
				offset += grab_data (&(fields->session_id), current_data, DMAP_CTYPE_INT);
				break;
			default:
				g_print ("Unrecognized content code or end of data: %s\n",
				          current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return fields;
}

static cc_data_t *
cc_handler_mupd (gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_data_t *fields;

	current_data = data + 8;
	data_end = data + data_len;

	fields = cc_data_new ();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','u','s','r'):
				offset += grab_data (&(fields->revision_id), current_data,
				                    DMAP_CTYPE_INT);
				break;
			case CC_TO_INT ('m','s','t','t'):
				offset += cc_handler_mstt (fields, current_data);
				break;
			default:
				g_print ("Unrecognized content code or end of data: %s\n",
				          current_data);
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return fields;
}

static cc_data_t *
cc_handler_avdb (gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_data_t *fields;

	current_data = data + 8;
	data_end = data + data_len;

	fields = cc_data_new ();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','s','t','t'):
				offset += cc_handler_mstt (fields, current_data);
				break;
			case CC_TO_INT ('m','u','t','y'):
				offset += cc_handler_muty (fields, current_data);
				break;
			case CC_TO_INT ('m','t','c','o'):
				offset += cc_handler_mtco (fields, current_data);
				break;
			case CC_TO_INT ('m','r','c','o'):
				offset += cc_handler_mrco (fields, current_data);
				break;
			case CC_TO_INT ('m','l','c','l'):
				offset += cc_handler_mlcl (fields, current_data,
				                           DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return fields;
}

static cc_data_t *
cc_handler_apso (gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_data_t *fields;

	current_data = data + 8;
	data_end = data + data_len;

	fields = cc_data_new ();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','s','t','t'):
				offset += cc_handler_mstt (fields, current_data);
				break;
			case CC_TO_INT ('m','u','t','y'):
				offset += cc_handler_muty (fields, current_data);
				break;
			case CC_TO_INT ('m','t','c','o'):
				offset += cc_handler_mtco (fields, current_data);
				break;
			case CC_TO_INT ('m','r','c','o'):
				offset += cc_handler_mrco (fields, current_data);
				break;
			case CC_TO_INT ('m','l','c','l'):
				offset += cc_handler_mlcl (fields, current_data,
				                           DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return fields;
}

static cc_data_t *
cc_handler_aply (gchar *data, gint data_len)
{
	gint offset = 0;
	gboolean do_break = FALSE;
	gchar *current_data, *data_end;
	cc_data_t *fields;

	current_data = data + 8;
	data_end = data + data_len;

	fields = cc_data_new ();

	while ((current_data < data_end) && !do_break) {
		switch (CC_TO_INT (current_data[0], current_data[1],
		                   current_data[2], current_data[3])) {
			case CC_TO_INT ('m','s','t','t'):
				offset += cc_handler_mstt (fields, current_data);
				break;
			case CC_TO_INT ('m','u','t','y'):
				offset += cc_handler_muty (fields, current_data);
				break;
			case CC_TO_INT ('m','t','c','o'):
				offset += cc_handler_mtco (fields, current_data);
				break;
			case CC_TO_INT ('m','r','c','o'):
				offset += cc_handler_mrco (fields, current_data);
				break;
			case CC_TO_INT ('m','l','c','l'):
				offset += cc_handler_mlcl (fields, current_data,
				                           DMAP_BYTES_REMAINING);
				break;
			default:
				do_break = TRUE;
				break;
		}

		current_data += offset;
		offset = 0;
	}

	return fields;
}

cc_data_t *
cc_data_new ()
{
	cc_data_t *retval;

	retval = g_malloc0 (sizeof (cc_data_t));
	retval->record_list = NULL;

	return retval;
}

void
cc_data_free (cc_data_t *fields)
{
	if (NULL != fields->server_name) g_free (fields->server_name);

	g_slist_foreach (fields->record_list,
	                 (GFunc) cc_item_record_free,
	                 NULL);
	g_slist_free (fields->record_list);

	g_free (fields);
}

void
cc_item_record_free (cc_item_record_t *item)
{
	if (NULL != item->iname)            g_free (item->iname);
	if (NULL != item->song_data_url)    g_free (item->song_data_url);
	if (NULL != item->song_data_album)  g_free (item->song_data_album);
	if (NULL != item->song_data_artist) g_free (item->song_data_artist);
	if (NULL != item->song_comment)     g_free (item->song_comment);
	if (NULL != item->song_description) g_free (item->song_description);
	if (NULL != item->song_genre)       g_free (item->song_genre);
	if (NULL != item->song_format)      g_free (item->song_format);
	if (NULL != item->song_composer)    g_free (item->song_composer);
	if (NULL != item->song_grouping)    g_free (item->song_grouping);

	g_free (item);
}

GSList *
cc_record_list_deep_copy (GSList *record_list) {
	GSList *retval = NULL;
	cc_item_record_t *record, *data;

	for (; record_list; record_list = g_slist_next (record_list)) {
		data = record_list->data;
		record = g_malloc0 (sizeof (cc_item_record_t));
		if (!record) {
			g_print ("memory allocation failed for cc_record_list_deep_copy\n");
			return NULL;
		}

		record->item_kind        = data->item_kind;
		record->song_data_kind   = data->song_data_kind;
		record->song_compilation = data->song_compilation;
		record->is_smart_pl      = data->is_smart_pl;
		record->is_base_pl       = data->is_base_pl;

		record->song_bitrate     = data->song_bitrate;
		record->song_year        = data->song_year;
		record->song_track_no    = data->song_track_no;
		record->song_track_count = data->song_track_count;
		record->song_disc_count  = data->song_disc_count;
		record->song_disc_no     = data->song_disc_no;
		record->song_bpm         = data->song_bpm;

		record->dbid             = data->dbid;
		record->sample_rate      = data->sample_rate;
		record->song_size        = data->song_size;
		record->song_start_time  = data->song_start_time;
		record->song_stop_time   = data->song_stop_time;
		record->song_total_time  = data->song_total_time;
		record->song_date        = data->song_date;
		record->song_date_mod    = data->song_date_mod;
		record->container_id     = data->container_id;

		record->deleted_id       = data->deleted_id;

		record->persistent_id    = data->persistent_id;

		record->iname            = g_strdup (data->iname);
		record->song_data_url    = g_strdup (data->song_data_url);
		record->song_data_album  = g_strdup (data->song_data_album);
		record->song_data_artist = g_strdup (data->song_data_artist);
		record->song_comment     = g_strdup (data->song_comment);
		record->song_description = g_strdup (data->song_description);
		record->song_genre       = g_strdup (data->song_genre);
		record->song_format      = g_strdup (data->song_format);
		record->song_composer    = g_strdup (data->song_composer);
		record->song_grouping    = g_strdup (data->song_grouping);

		/* db list specific */
		record->db_n_items       = data->db_n_items;
		record->db_n_playlist    = data->db_n_playlist;

		retval = g_slist_prepend (retval, record);
	}

	return retval;
}

cc_data_t *
cc_handler (gchar *data, gint data_len)
{
	cc_data_t *retval;

	switch (CC_TO_INT (data[0],data[1],data[2],data[3])) {
		case CC_TO_INT ('a','d','b','s'):
			retval = cc_handler_adbs (data, data_len);
			break;
		case CC_TO_INT ('m','s','r','v'):
			retval = cc_handler_msrv (data, data_len);
			break;
		case CC_TO_INT ('m','c','c','r'):
			retval = cc_handler_mccr (data, data_len);
			break;
		case CC_TO_INT ('m','l','o','g'):
			retval = cc_handler_mlog (data, data_len);
			break;
		case CC_TO_INT ('m','u','p','d'):
			retval = cc_handler_mupd (data, data_len);
			break;
		case CC_TO_INT ('a','v','d','b'):
			retval = cc_handler_avdb (data, data_len);
			break;
		case CC_TO_INT ('a','p','s','o'):
			retval = cc_handler_apso (data, data_len);
			break;
		case CC_TO_INT ('a','p','l','y'):
			retval = cc_handler_aply (data, data_len);
			break;
		default:
			retval = NULL;
			break;
	}

	return retval;
}

