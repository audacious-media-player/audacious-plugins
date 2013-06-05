/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery, Yoshiki Yazawa
 *                    and Matti Hämäläinen.
 * Copyright (c) 2011 John Lindgren
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

#include <stdio.h>
#include <glib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/misc.h>
#include <libaudcore/audstrings.h>

typedef struct asx_entry 
{
    GString   *title;
    GString   *author;
    GString   *copyright;
    GString   *abstract;
    GPtrArray *refs;
} asx_entry_t;

static gint read_cb (void * file, gchar * buf, gint len)
{
    return vfs_fread (buf, 1, len, file);
}

static gint write_cb (void * file, const gchar * buf, gint len)
{
    return vfs_fwrite (buf, 1, len, file);
}

static gint close_cb (void * file)
{
    return 0;
}


void freeAsx(asx_entry_t* asx)
{
	if (asx->title)
		g_string_free(asx->title, FALSE);	

	if (asx->author)
		g_string_free(asx->author, FALSE);

	if (asx->copyright)
		g_string_free(asx->copyright, FALSE);

	if (asx->abstract)
		g_string_free(asx->abstract, FALSE);

	int count;
    for (count = 0; count < asx->refs->len; count ++)
	{
		GString* href = g_ptr_array_index(asx->refs, count);
		g_string_free(href, FALSE);
	}

	g_ptr_array_free (asx->refs, TRUE);
}

GPtrArray*
parseEntry (xmlNodePtr cur)
{
    xmlChar *key = NULL;
    cur = cur->xmlChildrenNode;
	GPtrArray* result = g_ptr_array_new();
	GString* str;
		
    while (cur != NULL)
    {
        if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"ref")))
        {
            key = xmlGetProp(cur, (const xmlChar*) "href");
			if (key != NULL)
			{
				/* should we copy key before calling xmlFree() ? */
				str = g_string_new((const gchar*) key);
				g_ptr_array_add(result, str);
	            xmlFree(key);
			}
			
        }
		cur = cur->next;
    }
	
    return result;
}

int checkRoot(xmlNode *root)
{
	xmlChar *version = NULL;
	
	/* See if the root node is "asx" */
	if (xmlStrcasecmp(root->name, (const xmlChar *) "asx")) 
	{
        fprintf(stderr, "Document ist not an asx file");
        return FALSE;
    }

	version = xmlGetProp(root, (const xmlChar *) "version");

	/* See if version is 3.0 */
	if (xmlStrcmp(version, (const xmlChar *) "3.0")) 
	{
		fprintf(stderr, "Only version 3.0 is supported - found %s", version);
		xmlFree(version);
		return FALSE;
	}

	xmlFree(version);

	return TRUE;
}

GString*
contentToString(xmlNode* node)
{
	xmlChar *content = xmlNodeGetContent (node->xmlChildrenNode);

	/* assume that xmlChar and gchar are equal */
	GString *gstr = g_string_new((const gchar*) content);
	
	/* should we copy content before calling xmlFree() ? */
	xmlFree(content);

	return gstr;
}

asx_entry_t*
parseDoc(xmlDoc* doc)
{
	xmlNode *root = xmlDocGetRootElement(doc);

	xmlNode *cur = root->xmlChildrenNode;

	if (!checkRoot(root))
		return NULL;

	asx_entry_t *result = g_malloc0(sizeof(asx_entry_t));
	
    while (cur != NULL)
	{
        if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"entry")))
		{
            result->refs = parseEntry (cur);
        }

		if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"title")))
		{
			result->title = contentToString(cur);
        }

		if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"author")))
		{
			result->author = contentToString(cur);
        }

		if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"copyright")))
		{
			result->copyright = contentToString(cur);
        }

		if ((!xmlStrcasecmp(cur->name, (const xmlChar *)"abstract")))
		{
			result->abstract = contentToString(cur);
        }

		cur = cur->next;
    }

	return result;
}


xmlDoc* createXmlDoc(asx_entry_t* asx)
{
	xmlDoc* doc = xmlNewDoc((xmlChar *)"1.0");
    doc->charset = XML_CHAR_ENCODING_UTF8;
    doc->encoding = xmlStrdup((xmlChar *)"UTF-8");

    xmlNode* root = xmlNewNode(NULL, (const xmlChar *) "asx");
    xmlSetProp(root, (xmlChar *)"version", (const xmlChar *)"3.0");

    xmlDocSetRootElement(doc, root);

	xmlNode* entry = xmlNewNode(NULL, (const xmlChar *)"entry");
    xmlAddChild(root, entry);

	int count;
    for (count = 0; count < asx->refs->len; count ++)
	{
		xmlNode* ref = xmlNewNode(NULL, (const xmlChar*)"ref");
		xmlAddChild(entry, ref);

		GString* href = g_ptr_array_index(asx->refs, count);
		xmlSetProp(ref, (const xmlChar *) "href", (const xmlChar *) href->str);
	}

	if (asx->title != NULL && asx->title->len != 0)
	{
		const xmlChar *content = (const xmlChar *)asx->title->str;
		xmlNewTextChild (root, NULL, (const xmlChar *)"title", content);
	}

	if (asx->author != NULL && asx->author->len != 0)
	{
		const xmlChar *content = (const xmlChar *)asx->author->str;
		xmlNewTextChild (root, NULL, (const xmlChar *)"author", content);
	}

	if (asx->copyright != NULL && asx->copyright->len != 0)
	{
		const xmlChar *content = (const xmlChar *)asx->copyright->str;
		xmlNewTextChild (root, NULL, (const xmlChar *)"copyright", content);
	}
		
	if (asx->abstract != NULL && asx->abstract->len != 0)
	{
		const xmlChar *content = (const xmlChar *)asx->abstract->str;
		xmlNewTextChild (root, NULL, (const xmlChar *)"abstract", content);
	}

	return doc;
}

gboolean writeAsx(VFSFile * file, asx_entry_t* asx)
{
	xmlSaveCtxt* save;
	xmlDoc* doc = createXmlDoc(asx);

	save = xmlSaveToIO (write_cb, close_cb, file, NULL, XML_SAVE_FORMAT);
    if (!save)
	{
		xmlFreeDoc(doc);
		return FALSE;
	}

    if (xmlSaveDoc(save, doc) < 0 || xmlSaveClose(save) < 0)
	{
		xmlFreeDoc(doc);
        return FALSE;
	}

    xmlFreeDoc(doc);

	return TRUE;
}

gboolean playlist_load_asx3 (const gchar * filename, VFSFile * file,
 gchar * * title, Index * filenames, Index * tuples)
{
	/*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

    xmlDoc * doc = xmlReadIO (read_cb, close_cb, file, filename, NULL,
     XML_PARSE_RECOVER);
    if (! doc)
	{
        fprintf(stderr, "Failed to parse %s\n", filename);
        return FALSE;
	}

    asx_entry_t* asx = parseDoc(doc);

    * title = str_get(asx->title->str);
	
	int count;
    for (count = 0; count < asx->refs->len; count ++)
	{
		GString* href = g_ptr_array_index(asx->refs, count);

		gchar *uri = aud_construct_uri(href->str, filename);

		if (uri != NULL)
		{			
			index_append (filenames, str_get (uri));
			g_free (uri);
		}
	}

	freeAsx(asx);
	
    xmlFreeDoc(doc);

    xmlCleanupParser();

	/*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();

	return TRUE;
}

gboolean playlist_save_asx3 (const gchar * filename, VFSFile * file,
 const gchar * title, Index * filenames, Index * tuples)
{
	/*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

	gint entries = index_count (filenames);
	gint count;

	asx_entry_t* asx = g_malloc0(sizeof(asx_entry_t));

	asx->title = g_string_new(title);

	asx->refs = g_ptr_array_new();

    for (count = 0; count < entries; count ++)
    {
        const gchar * filename = index_get (filenames, count);
        gchar *fn;

        if (vfs_is_remote (filename))
            fn = g_strdup (filename);
        else
            fn = g_filename_from_uri (filename, NULL, NULL);

		g_ptr_array_add(asx->refs, g_string_new(fn));
	}

	gboolean retCode = writeAsx(file, asx);

	freeAsx(asx);

    xmlCleanupParser();

	/*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();

	return retCode;
}


