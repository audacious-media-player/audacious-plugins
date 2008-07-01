
#include <string.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <audacious/plugin.h>

#include "streambrowser.h"
#include "shoutcast.h"


gboolean shoutcast_category_fetch(category_t *category)
{
	gchar url[DEF_STRING_LEN];
	g_snprintf(url, DEF_STRING_LEN, SHOUTCAST_CATEGORY_URL, category->name);

	/* generate a valid, temporary filename */
	char *temp_filename = tempnam(NULL, "aud_sb");
	if (temp_filename == NULL) {
		failure("shoutcast: failed to create a temporary file\n");
		return FALSE;
	}

	char temp_pathname[DEF_STRING_LEN];
	sprintf(temp_pathname, "file://%s", temp_filename);
	free(temp_filename);

	debug("shoutcast: fetching category file '%s'\n", url);
	if (!fetch_remote_to_local_file(url, temp_pathname))  {
		failure("shoutcast: category file '%s' could not be downloaded to '%s'\n", url, temp_pathname);
		return FALSE;
	}
	debug("shoutcast: category file '%s' successfuly downloaded to '%s'\n", url, temp_pathname);

	xmlDoc *doc = xmlReadFile(temp_pathname, NULL, 0);
	if (doc == NULL) {
		failure("shoutcast: failed to read '%s' category file\n", category->name);
		return FALSE;
	}
	
	xmlNode *root_node = xmlDocGetRootElement(doc);
	xmlNode *node;
	
	root_node = root_node->children;

	for (node = root_node; node != NULL; node = node->next) {
		if (node->type == XML_ELEMENT_NODE && !strcmp((char *) node->name, "station")) {
			gchar *streaminfo_name = (gchar*) xmlGetProp(node, (xmlChar *) "name");
			gchar *streaminfo_id = (gchar*) xmlGetProp(node, (xmlChar *) "id");
			gchar streaminfo_playlist_url[DEF_STRING_LEN];
			gchar *streaminfo_current_track = (gchar*) xmlGetProp(node, (xmlChar *) "ct");
			
			g_snprintf(streaminfo_playlist_url, DEF_STRING_LEN, SHOUTCAST_STREAMINFO_URL, streaminfo_id);

			debug("shoutcast: fetching stream info for '%s/%d' from '%s'\n", streaminfo_name, streaminfo_id, url);

			streaminfo_t *streaminfo = streaminfo_new(streaminfo_name, streaminfo_playlist_url, streaminfo_current_track);
			streaminfo_add(category, streaminfo);
			
			xmlFree(streaminfo_name);
			xmlFree(streaminfo_id);
			xmlFree(streaminfo_current_track);

			debug("shoutcast: stream info added\n");
		}
	}
	
	remove(temp_filename);
	// todo: free the mallocs()
	
	return TRUE;
}


streamdir_t* shoutcast_streamdir_fetch()
{
	streamdir_t *streamdir = streamdir_new(SHOUTCAST_NAME);
	
	/* generate a valid, temporary filename */
	char *temp_filename = tempnam(NULL, "aud_sb");
	if (temp_filename == NULL) {
		failure("shoutcast: failed to create a temporary file\n");
		return NULL;
	}
	
	char temp_pathname[DEF_STRING_LEN];
	sprintf(temp_pathname, "file://%s", temp_filename);
	free(temp_filename);
	
	debug("shoutcast: fetching streaming directory file '%s'\n", SHOUTCAST_STREAMDIR_URL);
	if (!fetch_remote_to_local_file(SHOUTCAST_STREAMDIR_URL, temp_pathname)) {
		failure("shoutcast: stream directory file '%s' could not be downloaded to '%s'\n", SHOUTCAST_STREAMDIR_URL, temp_pathname);
		return NULL;
	}
	debug("shoutcast: stream directory file '%s' successfuly downloaded to '%s'\n", SHOUTCAST_STREAMDIR_URL, temp_pathname);

	xmlDoc *doc = xmlReadFile(temp_pathname, NULL, 0);
	if (doc == NULL) {
		failure("shoutcast: failed to read stream directory file\n");
		return NULL;
	}
	
	xmlNode *root_node = xmlDocGetRootElement(doc);
	xmlNode *node;
	
	root_node = root_node->children;

	for (node = root_node; node != NULL; node = node->next) {
		if (node->type == XML_ELEMENT_NODE) {
			gchar *category_name = (gchar*) xmlGetProp(node, (xmlChar *) "name");
			
			debug("shoutcast: fetching category '%s'\n", category_name);

			category_t *category = category_new(category_name);
			category_add(streamdir, category);
			
			xmlFree(category_name);
			
			debug("shoutcast: category added\n", category_name);
		}
	}

	// todo: free the mallocs()
	
	remove(temp_filename);
	debug("shoutcast: streaming directory successfuly loaded\n");

	return streamdir;
}

