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

#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

#include <audacious/plugin.h>

#define XSPF_ROOT_NODE_NAME "playlist"
#define XSPF_XMLNS "http://xspf.org/ns/0/"

typedef struct {
    gint tupleField;
    gchar *xspfName;
    TupleValueType type;
    gboolean isMeta;
} xspf_entry_t;


static const xspf_entry_t xspf_entries[] = {
    { FIELD_ARTIST,       "creator",      TUPLE_STRING,   FALSE},
    { FIELD_TITLE,        "title",        TUPLE_STRING,   FALSE},
    { FIELD_ALBUM,        "album",        TUPLE_STRING,   FALSE},
    { FIELD_COMMENT,      "annotation",   TUPLE_STRING,   FALSE},
    { FIELD_GENRE,        "genre",        TUPLE_STRING,   TRUE},

    { FIELD_TRACK_NUMBER, "trackNum",     TUPLE_INT,      FALSE},
    { FIELD_LENGTH,       "duration",     TUPLE_INT,      FALSE},
    { FIELD_YEAR,         "year",         TUPLE_INT,      TRUE},
    { FIELD_QUALITY,      "quality",      TUPLE_STRING,   TRUE},

    { FIELD_CODEC,        "codec",        TUPLE_STRING,   TRUE},

    { FIELD_SONG_ARTIST,  "song-artist",  TUPLE_STRING,   TRUE},
    { FIELD_COMPOSER,     "composer",     TUPLE_STRING,   TRUE},
    { FIELD_PERFORMER,    "performer",    TUPLE_STRING,   TRUE},
    { FIELD_COPYRIGHT,    "copyright",    TUPLE_STRING,   TRUE},
    { FIELD_DATE,         "date",         TUPLE_STRING,   TRUE},

    { FIELD_SUBSONG_ID,   "subsong-id",   TUPLE_INT,      TRUE},
    { FIELD_SUBSONG_NUM,  "subsong-num",  TUPLE_INT,      TRUE},
    { FIELD_MIMETYPE,     "mime-type",    TUPLE_STRING,   TRUE},
    { FIELD_BITRATE,      "bitrate",      TUPLE_INT,      TRUE},
    { FIELD_SEGMENT_START,"seg-start",    TUPLE_INT,      TRUE},
    { FIELD_SEGMENT_END,  "seg-end",      TUPLE_INT,      TRUE},

    { FIELD_GAIN_ALBUM_GAIN, "gain-album-gain", TUPLE_INT, TRUE},
    { FIELD_GAIN_ALBUM_PEAK, "gain-album-peak", TUPLE_INT, TRUE},
    { FIELD_GAIN_TRACK_GAIN, "gain-track-gain", TUPLE_INT, TRUE},
    { FIELD_GAIN_TRACK_PEAK, "gain-track-peak", TUPLE_INT, TRUE},
    { FIELD_GAIN_GAIN_UNIT, "gain-gain-unit", TUPLE_INT,  TRUE},
    { FIELD_GAIN_PEAK_UNIT, "gain-peak-unit", TUPLE_INT,  TRUE},
};

static const gint xspf_nentries = (sizeof(xspf_entries) / sizeof(xspf_entries[0]));

static void xspf_add_file (xmlNode * track, const gchar * filename, const gchar
 * base, struct index * filenames, struct index * tuples)
{
    xmlNode *nptr;
    gchar *location = NULL;
    Tuple * tuple = NULL;

    for (nptr = track->children; nptr != NULL; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(nptr->name, (xmlChar *)"location")) {
                /* Location is a special case */
                gchar *str = (gchar *)xmlNodeGetContent(nptr);

                if (strstr (str, "://") != NULL)
                    location = g_strdup (str);
                else if (str[0] == '/' && base != NULL)
                {
                    const gchar * colon = strstr (base, "://");

                    if (colon != NULL)
                        location = g_strdup_printf ("%.*s%s", (gint) (colon + 3
                         - base), base, str);
                }
                else if (base != NULL)
                {
                    const gchar * slash = strrchr (base, '/');

                    if (slash != NULL)
                        location = g_strdup_printf ("%.*s%s", (gint) (slash + 1
                         - base), base, str);
                }

                xmlFree(str);
            } else {
                /* Rest of the nodes are handled here */
                gint i;
                gboolean isMeta;
                xmlChar *findName;

                if (!xmlStrcmp(nptr->name, (xmlChar *)"meta")) {
                    isMeta = TRUE;
                    findName = xmlGetProp(nptr, (xmlChar *)"rel");
                } else {
                    isMeta = FALSE;
                    findName = xmlStrdup(nptr->name);
                }

                for (i = 0; i < xspf_nentries; i++)
                if ((xspf_entries[i].isMeta == isMeta) &&
                    !xmlStrcmp(findName, (xmlChar *)xspf_entries[i].xspfName)) {
                    xmlChar *str = xmlNodeGetContent(nptr);
                    switch (xspf_entries[i].type) {
                        case TUPLE_STRING:
                            if (! tuple)
                                tuple = tuple_new ();
                            tuple_associate_string(tuple, xspf_entries[i].tupleField, NULL, (gchar *)str);
                            break;

                        case TUPLE_INT:
                            if (! tuple)
                                tuple = tuple_new ();
                            tuple_associate_int(tuple, xspf_entries[i].tupleField, NULL, atol((char *)str));
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

    if (location != NULL)
    {
        if (tuple)
            tuple_set_filename (tuple, location);

        index_append(filenames, location);
        index_append(tuples, tuple);
    }
    else if (tuple)
        tuple_free (tuple);
}


static void xspf_find_track (xmlNode * tracklist, const gchar * filename, const
 gchar * base, struct index * filenames, struct index * tuples)
{
    xmlNode *nptr;

    for (nptr = tracklist->children; nptr != NULL; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE &&
         ! xmlStrcmp (nptr->name, (xmlChar *) "track"))
            xspf_add_file (nptr, filename, base, filenames, tuples);
    }
}

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

static gboolean xspf_playlist_load (const gchar * filename, VFSFile * file,
 gchar * * title, struct index * filenames, struct index * tuples)
{
    xmlDoc * doc = xmlReadIO (read_cb, close_cb, file, filename, NULL,
     XML_PARSE_RECOVER);
    if (! doc)
        return FALSE;

/* workaround for uninitialized libxml func pointers; see:
 * http://www.linuxquestions.org/questions/programming-9/[solved]using-libxml2-on-mingw-xmlfree-crashes-839802 */
#ifdef _WIN32
    if (! xmlFree)
        xmlMemGet (& xmlFree, & xmlMalloc, & xmlRealloc, NULL);
#endif

    * title = NULL;

    xmlNode *nptr, *nptr2;

    // find trackList
    for (nptr = doc->children; nptr != NULL; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE &&
            !xmlStrcmp(nptr->name, (xmlChar *)"playlist")) {
            gchar * base;

            base = (gchar *)xmlNodeGetBase(doc, nptr);

            for (nptr2 = nptr->children; nptr2; nptr2 = nptr2->next)
            {
                if (nptr2->type != XML_ELEMENT_NODE)
                    continue;

                if (! xmlStrcmp (nptr2->name, (xmlChar *) "title"))
                {
                    xmlChar * xml_title = xmlNodeGetContent (nptr2);
                    if (xml_title && xml_title[0])
                    {
                        g_free (* title);
                        * title = g_strdup ((gchar *) xml_title);
                    }
                    xmlFree (xml_title);
                }
                else if (! xmlStrcmp (nptr2->name, (xmlChar *) "trackList"))
                    xspf_find_track (nptr2, filename, base, filenames, tuples);
            }

            xmlFree (base);
        }
    }

    xmlFreeDoc(doc);
    return TRUE;
}


#define IS_VALID_CHAR(c) (((c) >= 0x20 && (c) <= 0xd7ff) || \
                          ((c) == 0x9) || \
                          ((c) == 0xa) || \
                          ((c) == 0xd) || \
                          ((c) >= 0xe000 && (c) <= 0xfffd) || \
                          ((c) >= 0x10000 && (c) <= 0x10ffff))

/* check for characters that are invalid in XML */
static gboolean is_valid_string (const gchar * s, gchar * * subst)
{
    if (! g_utf8_validate (s, -1, NULL))
        goto NOT_VALID;

    const gchar * p = s;
    while (* p)
    {
        gunichar c = g_utf8_get_char (p);

        if (IS_VALID_CHAR (c))
            p = g_utf8_next_char (p);
        else
            goto NOT_VALID;
    }

    return TRUE;

NOT_VALID:;
    gint len = 0;

    p = s;
    while (* p)
    {
        gunichar c = g_utf8_get_char_validated (p, -1);

        if (IS_VALID_CHAR (c))
        {
            len += g_unichar_to_utf8 (c, NULL);
            p = g_utf8_next_char (p);
        }
        else
            p ++;
    }

    * subst = g_malloc (len + 1);
    gchar * w = * subst;

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
    return FALSE;
}


static void xspf_add_node(xmlNodePtr node, TupleValueType type,
        gboolean isMeta, const gchar *xspfName, const gchar *strVal,
        const gint intVal)
{
    gchar tmps[64];
    xmlNodePtr tmp;

    if (isMeta) {
        tmp = xmlNewNode(NULL, (xmlChar *) "meta");
        xmlSetProp(tmp, (xmlChar *) "rel", (xmlChar *) xspfName);
    } else
        tmp = xmlNewNode(NULL, (xmlChar *) xspfName);

    switch (type) {
        case TUPLE_STRING:;
            gchar * subst;
            if (is_valid_string (strVal, & subst))
                xmlAddChild (tmp, xmlNewText ((xmlChar *) strVal));
            else
            {
                xmlAddChild (tmp, xmlNewText ((xmlChar *) subst));
                g_free (subst);
            }
            break;

        case TUPLE_INT:
            g_snprintf(tmps, sizeof(tmps), "%d", intVal);
            xmlAddChild(tmp, xmlNewText((xmlChar *) tmps));
            break;

        default:
            break;
    }

    xmlAddChild(node, tmp);
}


static gboolean xspf_playlist_save (const gchar * filename, VFSFile * file,
 const gchar * title, struct index * filenames, struct index * tuples)
{
    gint entries = index_count (filenames);
    xmlDocPtr doc;
    xmlNodePtr rootnode, tracklist;
    gint count;

    doc = xmlNewDoc((xmlChar *)"1.0");
    doc->charset = XML_CHAR_ENCODING_UTF8;
    doc->encoding = xmlStrdup((xmlChar *)"UTF-8");

    rootnode = xmlNewNode(NULL, (xmlChar *)XSPF_ROOT_NODE_NAME);
    xmlSetProp(rootnode, (xmlChar *)"version", (xmlChar *)"1");
    xmlSetProp(rootnode, (xmlChar *)"xmlns", (xmlChar *)XSPF_XMLNS);

    /* common */
    xmlDocSetRootElement(doc, rootnode);

    if (title)
        xspf_add_node (rootnode, TUPLE_STRING, FALSE, "title", title, 0);

    tracklist = xmlNewNode(NULL, (xmlChar *)"trackList");
    xmlAddChild(rootnode, tracklist);

    for (count = 0; count < entries; count ++)
    {
        const gchar * filename = index_get (filenames, count);
        const Tuple * tuple = index_get (tuples, count);
        xmlNodePtr track, location;
        const gchar *scratch = NULL;
        gint scratchi = 0;

        track = xmlNewNode(NULL, (xmlChar *)"track");
        location = xmlNewNode(NULL, (xmlChar *)"location");

        xmlAddChild(location, xmlNewText((xmlChar *)filename));
        xmlAddChild(track, location);
        xmlAddChild(tracklist, track);

        if (tuple != NULL)
        {
            gint i;
            for (i = 0; i < xspf_nentries; i++) {
                const xspf_entry_t *xs = &xspf_entries[i];
                gboolean isOK = (tuple_get_value_type (tuple, xs->tupleField,
                 NULL) == xs->type);

                switch (xs->type) {
                    case TUPLE_STRING:
                        scratch = tuple_get_string (tuple, xs->tupleField, NULL);
                        if (! scratch)
                            isOK = FALSE;
                        break;
                    case TUPLE_INT:
                        scratchi = tuple_get_int (tuple, xs->tupleField, NULL);
                        break;
                    default:
                        break;
                }

                if (isOK)
                    xspf_add_node(track, xs->type, xs->isMeta, xs->xspfName, scratch, scratchi);
            }
        }
    }

    xmlSaveCtxt * save = xmlSaveToIO (write_cb, close_cb, file, NULL,
     XML_SAVE_FORMAT);
    if (! save)
        goto ERR;

    if (xmlSaveDoc (save, doc) < 0 || xmlSaveClose (save) < 0)
        goto ERR;

    xmlFreeDoc(doc);
    return TRUE;

ERR:
    xmlFreeDoc (doc);
    return FALSE;
}

static const gchar * const xspf_exts[] = {"xspf", NULL};

AUD_PLAYLIST_PLUGIN
(
 .name = "XML Shareable Playlist Format",
 .extensions = xspf_exts,
 .load = xspf_playlist_load,
 .save = xspf_playlist_save
)
