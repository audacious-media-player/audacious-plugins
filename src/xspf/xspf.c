/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <glib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include "audacious/main.h"
#include "audacious/util.h"
#include "audacious/playlist.h"
#include "audacious/playlist_container.h"
#include "audacious/plugin.h"

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "base64.h"

#define TMP_BUF_LEN 128

static void
add_file(xmlNode *track, const gchar *filename, gint pos)
{
	xmlNode *nptr;
	TitleInput *tuple;
	gchar *location = NULL, *b64filename = NULL, *locale_uri = NULL;

	tuple = bmp_title_input_new();

	// creator, album, title, duration, trackNum, annotation, image, 
	for(nptr = track->children; nptr != NULL; nptr = nptr->next){
		if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "location")){
			xmlChar *str = xmlNodeGetContent(nptr);
			location = g_locale_from_utf8(str,-1,NULL,NULL,NULL);
			if(!location)
				location = g_strdup(str);
			xmlFree(str);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "creator")){
			tuple->performer = (gchar *)xmlNodeGetContent(nptr);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "album")){
			tuple->album_name = (gchar *)xmlNodeGetContent(nptr);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "title")){
			tuple->track_name = (gchar *)xmlNodeGetContent(nptr);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "duration")){
			xmlChar *str = xmlNodeGetContent(nptr);
			tuple->length = atol(str);
			xmlFree(str);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "trackNum")){
			xmlChar *str = xmlNodeGetContent(nptr);
			tuple->track_number = atol(str);
			xmlFree(str);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "annotation")){
			tuple->comment = (gchar *)xmlNodeGetContent(nptr);
			continue;
		}

		//
		// additional metadata
		//
		// year, date, genre, formatter, mtime, b64filename
		//
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "meta")){
			xmlChar *rel = NULL;
			
			rel = xmlGetProp(nptr, "rel");

			if(!xmlStrcmp(rel, "year")){
				xmlChar *cont = xmlNodeGetContent(nptr);
				tuple->year = atol(cont);
				xmlFree(cont);
				continue;
			}
			else if(!xmlStrcmp(rel, "date")){
				tuple->date = (gchar *)xmlNodeGetContent(nptr);
				continue;
			}
			else if(!xmlStrcmp(rel, "genre")){
				tuple->genre = (gchar *)xmlNodeGetContent(nptr);
				continue;
			}
			else if(!xmlStrcmp(rel, "formatter")){
				tuple->formatter = (gchar *)xmlNodeGetContent(nptr);
				continue;
			}
			else if(!xmlStrcmp(rel, "mtime")){
				xmlChar *str = NULL;
				str = xmlNodeGetContent(nptr);
				tuple->mtime = (time_t)atoll(str);
				xmlFree(str);
				continue;
			}
			else if(!xmlStrcmp(rel, "b64filename")){
				gchar *b64str = NULL;
				b64str = (gchar *)xmlNodeGetContent(nptr);
				b64filename = g_malloc0(strlen(b64str)*3/4+1);
				from64tobits(b64filename, b64str);
				g_free(b64str);
				continue;
			}

			xmlFree(rel);
			rel = NULL;
		}

	}

	if (tuple->length == 0) {
		tuple->length = -1;
	}

	if (tuple->mtime == 0) {
		tuple->mtime = -1;
	}

	locale_uri = b64filename ? b64filename : location;

	if(locale_uri){
		tuple->file_name = g_path_get_basename(locale_uri);
		tuple->file_path = g_path_get_dirname(locale_uri);
		tuple->file_ext = g_strdup(strrchr(locale_uri, '.'));
		// add file to playlist
		playlist_load_ins_file_tuple(locale_uri, filename, pos, tuple);
		pos++;
	}

	g_free(location); g_free(b64filename);
	locale_uri = NULL; location = NULL; b64filename = NULL;
}

static void
find_track(xmlNode *tracklist, const gchar *filename, gint pos)
{
	xmlNode *nptr;
	for(nptr = tracklist->children; nptr != NULL; nptr = nptr->next){
		if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "track")){
			add_file(nptr, filename, pos);
		}
	}
}

static void
playlist_load_xspf(const gchar * filename, gint pos)
{
	xmlDocPtr doc;
	xmlNode *nptr, *nptr2;

	g_return_if_fail(filename != NULL);

	doc = xmlParseFile(filename);
	if (doc == NULL)
		return;

	// find trackList
	for(nptr = doc->children; nptr != NULL; nptr = nptr->next) {
		if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, "playlist")){
			for(nptr2 = nptr->children; nptr2 != NULL; nptr2 = nptr2->next){
				if(nptr2->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr2->name, "trackList")){
					find_track(nptr2, filename, pos);
				}
			}

		}
	}

	xmlFreeDoc(doc);

}


#define XSPF_ROOT_NODE_NAME "playlist"
#define XSPF_XMLNS "http://xspf.org/ns/0/"

static void
playlist_save_xspf(const gchar *filename, gint pos)
{
	xmlDocPtr doc;
	xmlNodePtr rootnode, tmp, tracklist;
	GList *node;

	doc = xmlNewDoc("1.0");

	rootnode = xmlNewNode(NULL, XSPF_ROOT_NODE_NAME);
	xmlSetProp(rootnode, "xmlns", XSPF_XMLNS);
	xmlSetProp(rootnode, "version", "1");
	xmlDocSetRootElement(doc, rootnode);

	tmp = xmlNewNode(NULL, "creator");
	xmlAddChild(tmp, xmlNewText(PACKAGE "-" VERSION));
	xmlAddChild(rootnode, tmp);

	tracklist = xmlNewNode(NULL, "trackList");
	xmlAddChild(rootnode, tracklist);

	PLAYLIST_LOCK();

	for (node = playlist_get(); node != NULL; node = g_list_next(node))
	{
		PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
		xmlNodePtr track, location;
		gchar *utf_filename = NULL;
		gboolean use_base64 = FALSE;

		track = xmlNewNode(NULL, "track");
		location = xmlNewNode(NULL, "location");

		/* try locale encoding first */
		utf_filename = g_locale_to_utf8(entry->filename, -1, NULL, NULL, NULL);

		if (!utf_filename) {
			use_base64 = TRUE; /* filename isn't straightforward. */
			/* if above fails, try to guess */
			utf_filename = str_to_utf8(entry->filename);
		}
		if(!g_utf8_validate(utf_filename, -1, NULL))
			continue;
		xmlAddChild(location, xmlNewText(utf_filename));
		xmlAddChild(track, location);
		xmlAddChild(tracklist, track);

		/* do we have a tuple? */
		if (entry->tuple != NULL)
		{
			if (entry->tuple->performer != NULL &&
			    g_utf8_validate(entry->tuple->performer, -1, NULL))
			{
				tmp = xmlNewNode(NULL, "creator");
				xmlAddChild(tmp, xmlNewText(entry->tuple->performer));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->album_name != NULL &&
			    g_utf8_validate(entry->tuple->album_name, -1, NULL))
			{
				tmp = xmlNewNode(NULL, "album");
				xmlAddChild(tmp, xmlNewText(entry->tuple->album_name));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->track_name != NULL &&
			    g_utf8_validate(entry->tuple->track_name, -1, NULL))
			{
				tmp = xmlNewNode(NULL, "title");
				xmlAddChild(tmp, xmlNewText(entry->tuple->track_name));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->length > 0)
			{
				gchar *str;
				str = g_malloc(TMP_BUF_LEN);
				tmp = xmlNewNode(NULL, "duration");
				sprintf(str, "%d", entry->tuple->length);
				xmlAddChild(tmp, xmlNewText(str));
				g_free(str);
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->track_number != 0)
			{
				gchar *str;
				str = g_malloc(TMP_BUF_LEN);
				tmp = xmlNewNode(NULL, "trackNum");
				sprintf(str, "%d", entry->tuple->track_number);
				xmlAddChild(tmp, xmlNewText(str));
				g_free(str);
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->comment != NULL &&
			    g_utf8_validate(entry->tuple->comment, -1, NULL))
			{
				tmp = xmlNewNode(NULL, "annotation");
				xmlAddChild(tmp, xmlNewText(entry->tuple->comment));
				xmlAddChild(track, tmp);
			}

			//
			// additional metadata
			//
			// year, date, genre, formatter, mtime
			//
			if (entry->tuple->year != 0)
			{
				gchar *str;
				str = g_malloc(TMP_BUF_LEN);
				tmp = xmlNewNode(NULL, "meta");
				xmlSetProp(tmp, "rel", "year");
				sprintf(str, "%d", entry->tuple->year);
				xmlAddChild(tmp, xmlNewText(str));
				xmlAddChild(track, tmp);
				g_free(str);
			}

			if (entry->tuple->date != NULL &&
			    g_utf8_validate(entry->tuple->date, -1, NULL))
			{
				tmp = xmlNewNode(NULL, "meta");
				xmlSetProp(tmp, "rel", "date");
				xmlAddChild(tmp, xmlNewText(entry->tuple->date));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->genre != NULL &&
			    g_utf8_validate(entry->tuple->genre, -1, NULL))
			{
				tmp = xmlNewNode(NULL, "meta");
				xmlSetProp(tmp, "rel", "genre");
				xmlAddChild(tmp, xmlNewText(entry->tuple->genre));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->formatter != NULL &&
			    g_utf8_validate(entry->tuple->formatter, -1, NULL))
			{
				tmp = xmlNewNode(NULL, "meta");
				xmlSetProp(tmp, "rel", "formatter");
				xmlAddChild(tmp, xmlNewText(entry->tuple->formatter));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->mtime) {
				gchar *str;
				str = g_malloc(TMP_BUF_LEN);
				tmp = xmlNewNode(NULL, "meta");
				xmlSetProp(tmp, "rel", "mtime");
				sprintf(str, "%ld", (long) entry->tuple->mtime);
				xmlAddChild(tmp, xmlNewText(str));
				xmlAddChild(track, tmp);
				g_free(str);
			}

		}

		if (use_base64 && entry->filename) {
			gchar *b64str = NULL;
			b64str = g_malloc0(strlen(entry->filename)*2);
			to64frombits(b64str, entry->filename, strlen(entry->filename));
			tmp = xmlNewNode(NULL, "meta");
			xmlSetProp(tmp, "rel", "b64filename");
			xmlAddChild(tmp, xmlNewText(b64str));
			xmlAddChild(track, tmp);
			g_free(b64str);
			use_base64 = FALSE;
		}

		g_free(utf_filename);
		utf_filename = NULL;

	}

	PLAYLIST_UNLOCK();

	xmlSaveFormatFile(filename, doc, 1);
	xmlFreeDoc(doc);
}

PlaylistContainer plc_xspf = {
	.name = "XSPF Playlist Format",
	.ext = "xspf",
	.plc_read = playlist_load_xspf,
	.plc_write = playlist_save_xspf,
};

static void init(void)
{
	playlist_container_register(&plc_xspf);
}

static void cleanup(void)
{
	playlist_container_unregister(&plc_xspf);
}

LowlevelPlugin llp_xspf = {
	NULL,
	NULL,
	"XSPF Playlist Format",
	init,
	cleanup,
};

LowlevelPlugin *get_lplugin_info(void)
{
	return &llp_xspf;
}
