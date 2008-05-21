
#include <string.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "streambrowser.h"
#include "shoutcast.h"


static streaminfo_t*		shoutcast_streaminfo_fetch(gchar *station_name, gchar *station_id);
static category_t*		shoutcast_category_fetch(gchar *category_name);


static streaminfo_t *shoutcast_streaminfo_fetch(gchar *station_name, gchar *station_id)
{
	gchar url[DEF_STRING_LEN];
	g_snprintf(url, DEF_STRING_LEN, SHOUTCAST_STREAMINFO_URL, station_id);

	streaminfo_t *streaminfo = streaminfo_new(station_name, url);

	// todo: read the .pls file fetched from the above url
	
	return streaminfo;
}

static category_t *shoutcast_category_fetch(gchar *category_name)
{
	category_t *category = category_new(category_name);

	gchar url[DEF_STRING_LEN];
	g_snprintf(url, DEF_STRING_LEN, SHOUTCAST_CATEGORY_URL, category_name);

	xmlDoc *doc = xmlReadFile(url, NULL, 0);
	if (doc == NULL) {
		error("    shoutcast: failed to read \"%s\" category file\n", category_name);
		return NULL;
	}
	
	debug("    shoutcast: category file fetched\n");

	xmlNode *root_node = xmlDocGetRootElement(doc);
	xmlNode *node;
	
	root_node = root_node->children;

	for (node = root_node; node != NULL; node = node->next) {
		if (node->type == XML_ELEMENT_NODE && !strcmp((char *) node->name, "station")) {
			gchar *station_name = (gchar*) xmlGetProp(node, (xmlChar *) "name");
			gchar *station_id = (gchar*) xmlGetProp(node, (xmlChar *) "id");

			debug("        shoutcast: fetching stream info for name = \"%s\" and id = %s\n", station_name, station_id);

			streaminfo_t *streaminfo = shoutcast_streaminfo_fetch(station_name, station_id);
			streaminfo_add(category, streaminfo);
	
			// todo: debug - print info about streaminfon urls		
			debug("        shoutcast: stream info added for name = \"%s\" and id = %s\n", station_name, station_id);
		}
	}
	
	return category;
}


streamdir_t* shoutcast_streamdir_fetch()
{
	streamdir_t *streamdir = streamdir_new("Shoutcast");
	
	debug("shoutcast: fetching streaming directory file\n");

	// todo: replace dummy filename with SHOUTCAST_DIRECTORY_URL
	xmlDoc *doc = xmlReadFile("shoutcast.xml", NULL, 0);
	if (doc == NULL) {
		error("shoutcast: failed to read stream directory file\n");
		return NULL;
	}
	
	debug("shoutcast: streaming directory file fetched\n");

	xmlNode *root_node = xmlDocGetRootElement(doc);
	xmlNode *node;
	
	root_node = root_node->children;

	for (node = root_node; node != NULL; node = node->next) {
		if (node->type == XML_ELEMENT_NODE) {
			gchar *category_name = (gchar*) xmlGetProp(node, (xmlChar *) "name");
			
			debug("    shoutcast: fetching category \"%s\"\n", category_name);

			category_t *category = shoutcast_category_fetch(category_name);
			category_add(streamdir, category);
			
			debug("    shoutcast: added category \"%s\"\n", category_name);
		}
	}
	
	exit(0);
}

