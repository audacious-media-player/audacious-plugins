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
#include <stdlib.h>
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
#include <libxml/uri.h>

#define XSPF_ROOT_NODE_NAME "playlist"
#define XSPF_XMLNS "http://xspf.org/ns/0/"

#define TMP_BUF_LEN 128

gchar *base = NULL;
gboolean override_mtime = FALSE;

// this function is taken from libxml2-2.6.27.
static xmlChar *
audPathToURI(const xmlChar *path)
{
    xmlURIPtr uri;
    xmlURI temp;
    xmlChar *ret, *cal;

    if (path == NULL)
        return(NULL);

    if ((uri = xmlParseURI((const char *) path)) != NULL) {
	xmlFreeURI(uri);
	return xmlStrdup(path);
    }
    cal = xmlCanonicPath(path);
    if (cal == NULL)
        return(NULL);
    memset(&temp, 0, sizeof(temp));
    temp.path = (char *) cal;
    ret = xmlSaveUri(&temp);
    xmlFree(cal);
    return(ret);
}

static void
add_file(xmlNode *track, const gchar *filename, gint pos)
{
	xmlNode *nptr;
	TitleInput *tuple;
	gchar *location = NULL;
	Playlist *playlist = playlist_get_active();

	tuple = bmp_title_input_new();

    // staticlist hack
    tuple->mtime = -1; // mark as uninitialized.

	// creator, album, title, duration, trackNum, annotation, image, 
	for(nptr = track->children; nptr != NULL; nptr = nptr->next){
		if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"location")){
			gchar *str = (gchar *)xmlNodeGetContent(nptr);
			gchar *tmp = (gchar *)xmlURIUnescapeString(str, -1, NULL);

			if(strstr(tmp, "file://")){
				location = g_strdup_printf("%s%s", base ? base : "", tmp+7);
			}
			else {
				location = g_strdup_printf("%s%s", base ? base : "", tmp);
			}
			xmlFree(str); g_free(tmp);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"title")){
			tuple->track_name = (gchar *)xmlNodeGetContent(nptr);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"creator")){
			tuple->performer = (gchar *)xmlNodeGetContent(nptr);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"annotation")){
			tuple->comment = (gchar *)xmlNodeGetContent(nptr);
			continue;
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"album")){
			tuple->album_name = (gchar *)xmlNodeGetContent(nptr);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"trackNum")){
			xmlChar *str = xmlNodeGetContent(nptr);
			tuple->track_number = atol((char *)str);
			xmlFree(str);
		}
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"duration")){
			xmlChar *str = xmlNodeGetContent(nptr);
			tuple->length = atol((char *)str);
			xmlFree(str);
		}

		//
		// additional metadata
		//
		// year, date, genre, formatter, mtime
		//
		else if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"meta")){
			xmlChar *rel = NULL;
			
			rel = xmlGetProp(nptr, (xmlChar *)"rel");

			if(!xmlStrcmp(rel, (xmlChar *)"year")){
				xmlChar *cont = xmlNodeGetContent(nptr);
				tuple->year = atol((char *)cont);
				xmlFree(cont);
				continue;
			}
			else if(!xmlStrcmp(rel, (xmlChar *)"date")){
				tuple->date = (gchar *)xmlNodeGetContent(nptr);
				continue;
			}
			else if(!xmlStrcmp(rel, (xmlChar *)"genre")){
				tuple->genre = (gchar *)xmlNodeGetContent(nptr);
				continue;
			}
			else if(!xmlStrcmp(rel, (xmlChar *)"formatter")){
				tuple->formatter = (gchar *)xmlNodeGetContent(nptr);
				continue;
			}
			else if(!xmlStrcmp(rel, (xmlChar *)"mtime")){
				xmlChar *str = NULL;
				str = xmlNodeGetContent(nptr);
				tuple->mtime = (time_t)atoll((char *)str);
				xmlFree(str);
				continue;
			}
			xmlFree(rel);
			rel = NULL;
		}

	}

	if (tuple->length == 0) {
		tuple->length = -1;
	}

    if(override_mtime) {
        tuple->mtime = 0; //when mtime=0, scanning will be skipped.  --yaz
    }

	if(location){
		tuple->file_name = g_path_get_basename(location);
		tuple->file_path = g_path_get_dirname(location);
		tuple->file_ext = g_strdup(strrchr(location, '.'));
		// add file to playlist
		playlist_load_ins_file_tuple(playlist, location, filename, pos, tuple);
		pos++;
	}

	g_free(location);
	location = NULL;
}

static void
find_track(xmlNode *tracklist, const gchar *filename, gint pos)
{
	xmlNode *nptr;
	for(nptr = tracklist->children; nptr != NULL; nptr = nptr->next){
		if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"track")){
			add_file(nptr, filename, pos);
		}
	}
}

static void
find_audoptions(xmlNode *tracklist, const gchar *filename, gint pos)
{
	xmlNode *nptr;
	for(nptr = tracklist->children; nptr != NULL; nptr = nptr->next){
		if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"options")){
			xmlChar *opt = NULL;
			
			opt = xmlGetProp(nptr, (xmlChar *)"staticlist");
			if(!strcasecmp((char *)opt, "true")){
                override_mtime = TRUE;
			}
            xmlFree(opt);
            opt = NULL;
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

	xmlFree(base);
	base = NULL;

	// find trackList
	for(nptr = doc->children; nptr != NULL; nptr = nptr->next) {
		if(nptr->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr->name, (xmlChar *)"playlist")){
			base = (gchar *)xmlNodeGetBase(doc, nptr);
			if(!strcmp(base, filename)) {
			   xmlFree(base);
			   base = NULL;
			}

 			for(nptr2 = nptr->children; nptr2 != NULL; nptr2 = nptr2->next){

				if(nptr2->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr2->name, (xmlChar *)"extension")){
                    //check if application is audacious
                    xmlChar *app = NULL;
                    app = xmlGetProp(nptr2, (xmlChar *)"application");
                    if(!xmlStrcmp(app, (xmlChar *)"audacious")) {
                        find_audoptions(nptr2, filename, pos);
                    }
                    xmlFree(app);
				}

				if(nptr2->type == XML_ELEMENT_NODE && !xmlStrcmp(nptr2->name, (xmlChar *)"trackList")){
					find_track(nptr2, filename, pos);
				}

			}

		}
	}

	xmlFreeDoc(doc);

}

static void
playlist_save_xspf(const gchar *filename, gint pos)
{
	xmlDocPtr doc;
	xmlNodePtr rootnode, tmp, tracklist;
	GList *node;
        Playlist *playlist = playlist_get_active();

	doc = xmlNewDoc((xmlChar *)"1.0");

	doc->charset  = XML_CHAR_ENCODING_UTF8;
	doc->encoding = xmlStrdup((xmlChar *)"UTF-8");

	rootnode = xmlNewNode(NULL, (xmlChar *)XSPF_ROOT_NODE_NAME);
	xmlSetProp(rootnode, (xmlChar *)"xmlns", (xmlChar *)XSPF_XMLNS);
	xmlSetProp(rootnode, (xmlChar *)"version", (xmlChar *)"1");
	xmlDocSetRootElement(doc, rootnode);

	tmp = xmlNewNode(NULL, (xmlChar *)"creator");
	xmlAddChild(tmp, xmlNewText((xmlChar *)PACKAGE "-" VERSION));
	xmlAddChild(rootnode, tmp);

	tracklist = xmlNewNode(NULL, (xmlChar *)"trackList");
	xmlAddChild(rootnode, tracklist);

	PLAYLIST_LOCK(playlist->mutex);

	for (node = playlist->entries; node != NULL; node = g_list_next(node))
	{
		PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
		xmlNodePtr track, location;
		gchar *filename = NULL;

		track = xmlNewNode(NULL, (xmlChar *)"track");
		location = xmlNewNode(NULL, (xmlChar *)"location");

		/* uri escape entry->filename */
		if ( !strncasecmp("http://", entry->filename, 7)  ||
		     !strncasecmp("https://", entry->filename, 8) ||
		     !strncasecmp("mms://", entry->filename, 6) ) /* streaming */
		{
			gchar *tmp = (gchar *)xmlURIEscape((xmlChar *)entry->filename);
			filename = g_strdup(tmp ? tmp : entry->filename);
			g_free(tmp);
		}
		else /* local file */
		{
			gchar *tmp = (gchar *)audPathToURI((const xmlChar *)entry->filename);
			filename = g_strdup_printf("file://%s", tmp);
			g_free(tmp);
		}

		if(!g_utf8_validate(filename, -1, NULL))
			continue;

		xmlAddChild(location, xmlNewText((xmlChar *)filename));
		xmlAddChild(track, location);
		xmlAddChild(tracklist, track);

		/* do we have a tuple? */
		if (entry->tuple != NULL)
		{
			if (entry->tuple->track_name != NULL &&
			    g_utf8_validate(entry->tuple->track_name, -1, NULL))
			{
				tmp = xmlNewNode(NULL, (xmlChar *)"title");
				xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->track_name));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->performer != NULL &&
			    g_utf8_validate(entry->tuple->performer, -1, NULL))
			{
				tmp = xmlNewNode(NULL, (xmlChar *)"creator");
				xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->performer));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->comment != NULL &&
			    g_utf8_validate(entry->tuple->comment, -1, NULL))
			{
				tmp = xmlNewNode(NULL, (xmlChar *)"annotation");
				xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->comment));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->album_name != NULL &&
			    g_utf8_validate(entry->tuple->album_name, -1, NULL))
			{
				tmp = xmlNewNode(NULL, (xmlChar *)"album");
				xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->album_name));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->track_number != 0)
			{
				gchar *str;
				str = g_malloc(TMP_BUF_LEN);
				tmp = xmlNewNode(NULL, (xmlChar *)"trackNum");
				sprintf(str, "%d", entry->tuple->track_number);
				xmlAddChild(tmp, xmlNewText((xmlChar *)str));
				g_free(str);
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->length > 0)
			{
				gchar *str;
				str = g_malloc(TMP_BUF_LEN);
				tmp = xmlNewNode(NULL, (xmlChar *)"duration");
				sprintf(str, "%d", entry->tuple->length);
				xmlAddChild(tmp, xmlNewText((xmlChar *)str));
				g_free(str);
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
				tmp = xmlNewNode(NULL, (xmlChar *)"meta");
				xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"year");
				sprintf(str, "%d", entry->tuple->year);
				xmlAddChild(tmp, xmlNewText((xmlChar *)str));
				xmlAddChild(track, tmp);
				g_free(str);
			}

			if (entry->tuple->date != NULL &&
			    g_utf8_validate(entry->tuple->date, -1, NULL))
			{
				tmp = xmlNewNode(NULL, (xmlChar *)"meta");
				xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"date");
				xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->date));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->genre != NULL &&
			    g_utf8_validate(entry->tuple->genre, -1, NULL))
			{
				tmp = xmlNewNode(NULL, (xmlChar *)"meta");
				xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"genre");
				xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->genre));
				xmlAddChild(track, tmp);
			}

			if (entry->tuple->formatter != NULL &&
			    g_utf8_validate(entry->tuple->formatter, -1, NULL))
			{
				tmp = xmlNewNode(NULL, (xmlChar *)"meta");
				xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"formatter");
				xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->formatter));
				xmlAddChild(track, tmp);
			}

            // mtime: write mtime unconditionally for staticlist hack.
//			if (entry->tuple->mtime) {
				gchar *str;
				str = g_malloc(TMP_BUF_LEN);
				tmp = xmlNewNode(NULL, (xmlChar *)"meta");
				xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"mtime");
				sprintf(str, "%ld", (long) entry->tuple->mtime);
				xmlAddChild(tmp, xmlNewText((xmlChar *)str));
				xmlAddChild(track, tmp);
				g_free(str);
//			}

		}
		g_free(filename);
		filename = NULL;
	}

	PLAYLIST_UNLOCK(playlist->mutex);

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
