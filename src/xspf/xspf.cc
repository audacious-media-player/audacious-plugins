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

#include <glib.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>

#define XSPF_ROOT_NODE_NAME "playlist"
#define XSPF_XMLNS "http://xspf.org/ns/0/"

typedef struct {
    int tupleField;
    const char *xspfName;
    TupleValueType type;
    bool isMeta;
} xspf_entry_t;


static const xspf_entry_t xspf_entries[] = {
    { FIELD_ARTIST,       "creator",      TUPLE_STRING,   false},
    { FIELD_TITLE,        "title",        TUPLE_STRING,   false},
    { FIELD_ALBUM,        "album",        TUPLE_STRING,   false},
    { FIELD_COMMENT,      "annotation",   TUPLE_STRING,   false},
    { FIELD_GENRE,        "genre",        TUPLE_STRING,   true},

    { FIELD_TRACK_NUMBER, "trackNum",     TUPLE_INT,      false},
    { FIELD_LENGTH,       "duration",     TUPLE_INT,      false},
    { FIELD_YEAR,         "year",         TUPLE_INT,      true},
    { FIELD_QUALITY,      "quality",      TUPLE_STRING,   true},

    { FIELD_CODEC,        "codec",        TUPLE_STRING,   true},

    { FIELD_ALBUM_ARTIST, "album-artist", TUPLE_STRING,   true},
    { FIELD_COMPOSER,     "composer",     TUPLE_STRING,   true},
    { FIELD_PERFORMER,    "performer",    TUPLE_STRING,   true},
    { FIELD_COPYRIGHT,    "copyright",    TUPLE_STRING,   true},
    { FIELD_DATE,         "date",         TUPLE_STRING,   true},

    { FIELD_SUBSONG_ID,   "subsong-id",   TUPLE_INT,      true},
    { FIELD_SUBSONG_NUM,  "subsong-num",  TUPLE_INT,      true},
    { FIELD_MIMETYPE,     "mime-type",    TUPLE_STRING,   true},
    { FIELD_BITRATE,      "bitrate",      TUPLE_INT,      true},
    { FIELD_SEGMENT_START,"seg-start",    TUPLE_INT,      true},
    { FIELD_SEGMENT_END,  "seg-end",      TUPLE_INT,      true},

    { FIELD_GAIN_ALBUM_GAIN, "gain-album-gain", TUPLE_INT, true},
    { FIELD_GAIN_ALBUM_PEAK, "gain-album-peak", TUPLE_INT, true},
    { FIELD_GAIN_TRACK_GAIN, "gain-track-gain", TUPLE_INT, true},
    { FIELD_GAIN_TRACK_PEAK, "gain-track-peak", TUPLE_INT, true},
    { FIELD_GAIN_GAIN_UNIT, "gain-gain-unit", TUPLE_INT,  true},
    { FIELD_GAIN_PEAK_UNIT, "gain-peak-unit", TUPLE_INT,  true},
};

static const char * const xspf_exts[] = {"xspf"};

class XSPFLoader : public PlaylistPlugin
{
public:
    static constexpr PluginInfo info = {N_("XML Shareable Playlists (XSPF)"), PACKAGE};

    constexpr XSPFLoader () : PlaylistPlugin (info, xspf_exts, true) {}

    bool load (const char * filename, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items);
    bool save (const char * filename, VFSFile & file, const char * title,
     const Index<PlaylistAddItem> & items);
};

EXPORT XSPFLoader aud_plugin_instance;

static void xspf_add_file (xmlNode * track, const char * filename,
 const char * base, Index<PlaylistAddItem> & items)
{
    xmlNode *nptr;
    String location;
    Tuple tuple;

    for (nptr = track->children; nptr != nullptr; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(nptr->name, (xmlChar *)"location")) {
                /* Location is a special case */
                char *str = (char *)xmlNodeGetContent(nptr);

                if (strstr (str, "://") != nullptr)
                    location = String (str);
                else if (str[0] == '/' && base != nullptr)
                {
                    const char * colon = strstr (base, "://");

                    if (colon != nullptr)
                        location = String (str_printf ("%.*s%s",
                         (int) (colon + 3 - base), base, str));
                }
                else if (base != nullptr)
                {
                    const char * slash = strrchr (base, '/');

                    if (slash != nullptr)
                        location = String (str_printf ("%.*s%s",
                         (int) (slash + 1 - base), base, str));
                }

                xmlFree(str);
            } else {
                /* Rest of the nodes are handled here */
                bool isMeta;
                xmlChar *findName;

                if (!xmlStrcmp(nptr->name, (xmlChar *)"meta")) {
                    isMeta = true;
                    findName = xmlGetProp(nptr, (xmlChar *)"rel");
                } else {
                    isMeta = false;
                    findName = xmlStrdup(nptr->name);
                }

                for (const xspf_entry_t & entry : xspf_entries)
                if ((entry.isMeta == isMeta) &&
                    !xmlStrcmp(findName, (xmlChar *)entry.xspfName)) {
                    xmlChar *str = xmlNodeGetContent(nptr);
                    switch (entry.type) {
                        case TUPLE_STRING:
                            tuple.set_str (entry.tupleField, (char *)str);
                            break;

                        case TUPLE_INT:
                            tuple.set_int (entry.tupleField, atol((char *)str));
                            break;

                        default:
                            break;
                    }
                    xmlFree(str);
                    break;
                }

                xmlFree(findName);
            }
        }
    }

    if (location != nullptr)
    {
        if (tuple)
            tuple.set_filename (location);

        items.append (location, std::move (tuple));
    }
}


static void xspf_find_track (xmlNode * tracklist, const char * filename,
 const char * base, Index<PlaylistAddItem> & items)
{
    xmlNode *nptr;

    for (nptr = tracklist->children; nptr != nullptr; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE &&
         ! xmlStrcmp (nptr->name, (xmlChar *) "track"))
            xspf_add_file (nptr, filename, base, items);
    }
}

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

bool XSPFLoader::load (const char * filename, VFSFile & file, String & title,
 Index<PlaylistAddItem> & items)
{
    xmlDoc * doc = xmlReadIO (read_cb, close_cb, & file, filename, nullptr, XML_PARSE_RECOVER);
    if (! doc)
        return false;

    xmlNode *nptr, *nptr2;

    // find trackList
    for (nptr = doc->children; nptr != nullptr; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE &&
            !xmlStrcmp(nptr->name, (xmlChar *)"playlist")) {
            char * base;

            base = (char *)xmlNodeGetBase(doc, nptr);

            for (nptr2 = nptr->children; nptr2; nptr2 = nptr2->next)
            {
                if (nptr2->type != XML_ELEMENT_NODE)
                    continue;

                if (! xmlStrcmp (nptr2->name, (xmlChar *) "title"))
                {
                    xmlChar * xml_title = xmlNodeGetContent (nptr2);
                    if (xml_title && xml_title[0])
                        title = String ((char *) xml_title);
                    xmlFree (xml_title);
                }
                else if (! xmlStrcmp (nptr2->name, (xmlChar *) "trackList"))
                    xspf_find_track (nptr2, filename, base, items);
            }

            xmlFree (base);
        }
    }

    xmlFreeDoc(doc);
    return true;
}


#define IS_VALID_CHAR(c) (((c) >= 0x20 && (c) <= 0xd7ff) || \
                          ((c) == 0x9) || \
                          ((c) == 0xa) || \
                          ((c) == 0xd) || \
                          ((c) >= 0xe000 && (c) <= 0xfffd) || \
                          ((c) >= 0x10000 && (c) <= 0x10ffff))

/* check for characters that are invalid in XML */
static bool is_valid_string (const char * s, char * * subst)
{
    bool valid = g_utf8_validate (s, -1, nullptr);

    if (valid)
    {
        for (const char * p = s; * p; p = g_utf8_next_char (p))
        {
            if (! IS_VALID_CHAR (g_utf8_get_char (p)))
            {
                valid = false;
                break;
            }
        }

        if (valid)
            return true;
    }

    int len = 0;

    const char * p = s;
    while (* p)
    {
        gunichar c = g_utf8_get_char_validated (p, -1);

        if (IS_VALID_CHAR (c))
        {
            len += g_unichar_to_utf8 (c, nullptr);
            p = g_utf8_next_char (p);
        }
        else
            p ++;
    }

    * subst = g_new (char, len + 1);
    char * w = * subst;

    p = s;
    while (* p)
    {
        gunichar c = g_utf8_get_char_validated (p, -1);

        if (IS_VALID_CHAR (c))
        {
            w += g_unichar_to_utf8 (c, w);
            p = g_utf8_next_char (p);
        }
        else
            p ++;
    }

    * w = 0;
    return false;
}


static void xspf_add_node(xmlNodePtr node, TupleValueType type,
        bool isMeta, const char *xspfName, const char *strVal,
        const int intVal)
{
    xmlNodePtr tmp;

    if (isMeta) {
        tmp = xmlNewNode(nullptr, (xmlChar *) "meta");
        xmlSetProp(tmp, (xmlChar *) "rel", (xmlChar *) xspfName);
    } else
        tmp = xmlNewNode(nullptr, (xmlChar *) xspfName);

    switch (type) {
        case TUPLE_STRING:;
            char * subst;
            if (is_valid_string (strVal, & subst))
                xmlAddChild (tmp, xmlNewText ((xmlChar *) strVal));
            else
            {
                xmlAddChild (tmp, xmlNewText ((xmlChar *) subst));
                g_free (subst);
            }
            break;

        case TUPLE_INT:
            xmlAddChild (tmp, xmlNewText ((xmlChar *) (char *) int_to_str (intVal)));
            break;

        default:
            break;
    }

    xmlAddChild(node, tmp);
}


bool XSPFLoader::save (const char * filename, VFSFile & file,
 const char * title, const Index<PlaylistAddItem> & items)
{
    xmlDocPtr doc;
    xmlNodePtr rootnode, tracklist;

    doc = xmlNewDoc((xmlChar *)"1.0");
    doc->charset = XML_CHAR_ENCODING_UTF8;
    doc->encoding = xmlStrdup((xmlChar *)"UTF-8");

    rootnode = xmlNewNode(nullptr, (xmlChar *)XSPF_ROOT_NODE_NAME);
    xmlSetProp(rootnode, (xmlChar *)"version", (xmlChar *)"1");
    xmlSetProp(rootnode, (xmlChar *)"xmlns", (xmlChar *)XSPF_XMLNS);

    /* common */
    xmlDocSetRootElement(doc, rootnode);

    if (title)
        xspf_add_node (rootnode, TUPLE_STRING, false, "title", title, 0);

    tracklist = xmlNewNode(nullptr, (xmlChar *)"trackList");
    xmlAddChild(rootnode, tracklist);

    for (auto & item : items)
    {
        const char * filename = item.filename;
        const Tuple & tuple = item.tuple;
        xmlNodePtr track, location;
        String scratch;
        int scratchi = 0;

        track = xmlNewNode(nullptr, (xmlChar *)"track");
        location = xmlNewNode(nullptr, (xmlChar *)"location");

        xmlAddChild(location, xmlNewText((xmlChar *)filename));
        xmlAddChild(track, location);
        xmlAddChild(tracklist, track);

        if (tuple)
        {
            for (const xspf_entry_t & entry : xspf_entries)
            {
                bool isOK = (tuple.get_value_type (entry.tupleField) == entry.type);

                switch (entry.type) {
                    case TUPLE_STRING:
                        scratch = tuple.get_str (entry.tupleField);
                        if (! scratch)
                            isOK = false;
                        break;
                    case TUPLE_INT:
                        scratchi = tuple.get_int (entry.tupleField);
                        break;
                    default:
                        break;
                }

                if (isOK)
                    xspf_add_node (track, entry.type, entry.isMeta,
                     entry.xspfName, scratch, scratchi);
            }
        }
    }

    xmlSaveCtxt * save = xmlSaveToIO (write_cb, close_cb, & file, nullptr, XML_SAVE_FORMAT);
    if (! save)
        goto ERR;

    if (xmlSaveDoc (save, doc) < 0 || xmlSaveClose (save) < 0)
        goto ERR;

    xmlFreeDoc(doc);
    return true;

ERR:
    xmlFreeDoc (doc);
    return false;
}
