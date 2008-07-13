/*
 * Audacious Streambrowser Plugin
 *
 * Copyright (c) 2008 Calin Crisan <ccrisan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */


#include <string.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <audacious/plugin.h>

#include "streambrowser.h"
#include "xiph.h"


typedef struct {
	gchar name[DEF_STRING_LEN];
	gchar url[DEF_STRING_LEN];
	gchar current_song[DEF_STRING_LEN];
	gchar genre[DEF_STRING_LEN];
} xiph_entry_t;


static xiph_entry_t *xiph_entries = NULL;
static int xiph_entry_count = 0;
static gchar *categories[] = {
	// todo: complete this list
	"alternative",
	"dance",
	"techno",
	"rock",
	"pop"
};


void refresh_streamdir();



gboolean xiph_category_fetch(category_t *category)
{
	int entryno;
	
	/* see what entries match this category */
	for (entryno = 0; entryno < xiph_entry_count; entryno++) {

		if (mystrcasestr(xiph_entries[entryno].genre, category->name)) {
			streaminfo_t *streaminfo = streaminfo_new(xiph_entries[entryno].name, "", xiph_entries[entryno].url, xiph_entries[entryno].current_song);
			streaminfo_add(category, streaminfo);
		}
	
	}
	
	return TRUE;
}


streamdir_t* xiph_streamdir_fetch()
{
	streamdir_t *streamdir = streamdir_new(XIPH_NAME);
	int categno;
	
	refresh_streamdir();
	
	for (categno = 0; categno < sizeof(categories) / sizeof(gchar *); categno++) {
		category_t *category = category_new(categories[categno]);
		category_add(streamdir, category);
	}

	return streamdir;
}

void refresh_streamdir()
{
	/* free any previously fetched streamdir data */
	if (xiph_entries != NULL)
		free(xiph_entries);
	xiph_entry_count = 0;

	debug("xiph: fetching streaming directory file '%s'\n", XIPH_STREAMDIR_URL);
	if (!fetch_remote_to_local_file(XIPH_STREAMDIR_URL, XIPH_TEMP_FILENAME)) {
		failure("xiph: stream directory file '%s' could not be downloaded to '%s'\n", XIPH_STREAMDIR_URL, XIPH_TEMP_FILENAME);
		return;
	}
	debug("xiph: stream directory file '%s' successfuly downloaded to '%s'\n", XIPH_STREAMDIR_URL, XIPH_TEMP_FILENAME);

	xmlDoc *doc = xmlReadFile(XIPH_TEMP_FILENAME, NULL, 0);
	if (doc == NULL) {
		failure("xiph: failed to read stream directory file\n");
		return;
	}
	
	xmlNode *root_node = xmlDocGetRootElement(doc);
	xmlNode *node, *child;
	gchar *content;
	
	root_node = root_node->children;

	for (node = root_node; node != NULL; node = node->next) {
		if (node->type == XML_ELEMENT_NODE) {
			xiph_entries = realloc(xiph_entries, sizeof(xiph_entry_t) * (xiph_entry_count + 1));
		
			for (child = node->children; child != NULL; child = child->next) {
				if (strcmp((gchar *)child->name, "server_name") == 0) {
					content = (gchar *) xmlNodeGetContent(child);
					strcpy(xiph_entries[xiph_entry_count].name, content);
					xmlFree(content);
				}
				else if (strcmp((gchar *)child->name, "listen_url") == 0) {
					content = (gchar *) xmlNodeGetContent(child);
					strcpy(xiph_entries[xiph_entry_count].url, content);
					xmlFree(content);
				}
				else if (strcmp((gchar *)child->name, "current_song") == 0) {
					content = (gchar *) xmlNodeGetContent(child);
					strcpy(xiph_entries[xiph_entry_count].current_song, content);
					xmlFree(content);
				}
				else if (strcmp((gchar *)child->name, "genre") == 0) {
					content = (gchar *) xmlNodeGetContent(child);
					strcpy(xiph_entries[xiph_entry_count].genre, content);
					xmlFree(content);
				}
			}
			
			xiph_entry_count++;
		}
	}

	xmlFreeDoc(doc);
	
	debug("xiph: streaming directory successfuly loaded\n");
}

