/*
 * asx3.c
 * Copyright 2013 Martin Steiger and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

static const char * const asx3_exts[] = {"asx"};

class ASX3Loader : public PlaylistPlugin
{
public:
    static constexpr PluginInfo info = {N_("ASXv3 Playlists"), PACKAGE};

    constexpr ASX3Loader () : PlaylistPlugin (info, asx3_exts, true) {}

    bool load (const char * filename, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items);
    bool save (const char * filename, VFSFile & file, const char * title,
     const Index<PlaylistAddItem> & items);
};

EXPORT ASX3Loader aud_plugin_instance;

static int read_cb (void * file, char * buf, int len)
{
    return ((VFSFile *) file)->fread (buf, 1, len);
}

static int write_cb (void * file, const char * buf, int len)
{
    return ((VFSFile *) file)->fwrite (buf, 1, len);
}

static int close_cb (void * file)
{
    return 0;
}

static const char * get_content (const xmlNode * node)
{
    const xmlNode * child = node->xmlChildrenNode;
    if (child && child->type == XML_TEXT_NODE && child->content)
        return (const char *) child->content;

    return nullptr;
}

static const char * get_prop_nocase (const xmlNode * node, const char * name)
{
    for (const xmlAttr * prop = node->properties; prop; prop = prop->next)
    {
        if (! xmlStrcasecmp (prop->name, (const xmlChar *) name))
        {
            const xmlNode * child = prop->children;
            if (child && child->type == XML_TEXT_NODE && child->content)
                return (const char *) child->content;
        }
    }

    return nullptr;
}

static bool check_root (const xmlNode * root)
{
    if (xmlStrcasecmp (root->name, (const xmlChar *) "asx"))
    {
        AUDERR ("Not an ASX file\n");
        return false;
    }

    const char * version = get_prop_nocase (root, "version");

    if (! version)
    {
        AUDERR ("Unknown ASX version\n");
        return false;
    }

    if (strcmp (version, "3.0"))
    {
        AUDERR ("Unsupported ASX version (%s)\n", version);
        return false;
    }

    return true;
}

static void parse_entry (const xmlNode * entry, Index<PlaylistAddItem> & items)
{
    for (const xmlNode * node = entry->xmlChildrenNode; node; node = node->next)
    {
        if (! xmlStrcasecmp (node->name, (const xmlChar *) "ref"))
        {
            const char * uri = get_prop_nocase (node, "href");
            if (uri)
                items.append (String (uri));
        }
    }
}

bool ASX3Loader::load (const char * filename, VFSFile & file, String & title,
 Index<PlaylistAddItem> & items)
{
    xmlDoc * doc = xmlReadIO (read_cb, close_cb, & file, filename, nullptr, XML_PARSE_RECOVER);
    if (! doc)
        return false;

    xmlNode * root = xmlDocGetRootElement (doc);

    if (! root || ! check_root (root))
    {
        xmlFreeDoc(doc);
        return false;
    }

    for (xmlNode * node = root->xmlChildrenNode; node; node = node->next)
    {
        if (! xmlStrcasecmp (node->name, (const xmlChar *) "entry"))
            parse_entry (node, items);
        else if (! xmlStrcasecmp (node->name, (const xmlChar *) "title"))
        {
            if (! title)
                title = String (get_content (node));
        }
    }

    xmlFreeDoc(doc);
    return true;
}

bool ASX3Loader::save (const char * filename, VFSFile & file,
 const char * title, const Index<PlaylistAddItem> & items)
{
    xmlDoc * doc = xmlNewDoc ((const xmlChar *) "1.0");
    doc->charset = XML_CHAR_ENCODING_UTF8;
    doc->encoding = xmlStrdup ((const xmlChar *) "UTF-8");

    xmlNode * root = xmlNewNode (nullptr, (const xmlChar *) "asx");
    xmlSetProp (root, (const xmlChar *) "version", (const xmlChar *) "3.0");
    xmlDocSetRootElement (doc, root);

    if (title)
        xmlNewTextChild (root, nullptr, (const xmlChar *) "title", (const xmlChar *) title);

    for (auto & item : items)
    {
        xmlNode * entry = xmlNewNode (nullptr, (const xmlChar *) "entry");
        xmlNode * ref = xmlNewNode (nullptr, (const xmlChar *) "ref");
        xmlSetProp (ref, (const xmlChar *) "href", (const xmlChar *) (const char *) item.filename);
        xmlAddChild (entry, ref);
        xmlAddChild (root, entry);
    }

    xmlSaveCtxt * save = xmlSaveToIO (write_cb, close_cb, & file, nullptr, XML_SAVE_FORMAT);

    if (! save || xmlSaveDoc (save, doc) < 0 || xmlSaveClose (save) < 0)
    {
        xmlFreeDoc(doc);
        return false;
    }

    xmlFreeDoc (doc);
    return true;
}
