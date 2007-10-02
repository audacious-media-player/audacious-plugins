/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery, Yoshiki Yazawa
 *                    and Matti Hämäläinen.
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

#include <audacious/plugin.h>
#include <audacious/main.h>
#include <audacious/util.h>
#include <audacious/playlist.h>
#include <audacious/playlist_container.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

#define XSPF_ROOT_NODE_NAME "playlist"
#define XSPF_XMLNS "http://xspf.org/ns/0/"

enum {
    CMP_DEF = 0,
    CMP_GT,
    CMP_NULL
} xspf_compare;

typedef struct {
    gint tupleField;
    gchar *xspfName;
    TupleValueType type;
    gboolean isMeta;
    gint compare;
} xspf_entry_t;


static const xspf_entry_t xspf_entries[] = {
    { FIELD_TITLE,        "title",        TUPLE_STRING,   FALSE,  CMP_DEF },
    { FIELD_ARTIST,       "creator",      TUPLE_STRING,   FALSE,  CMP_DEF },
    { FIELD_COMMENT,      "annotation",   TUPLE_STRING,   FALSE,  CMP_DEF },
    { FIELD_ALBUM,        "album",        TUPLE_STRING,   FALSE,  CMP_DEF },
    { FIELD_TRACK_NUMBER, "trackNum",     TUPLE_INT,      FALSE,  CMP_DEF },
    { FIELD_LENGTH,       "duration",     TUPLE_INT,      FALSE,  CMP_GT },

    { FIELD_YEAR,         "year",         TUPLE_INT,      TRUE,   CMP_DEF },
    { FIELD_DATE,         "date",         TUPLE_STRING,   TRUE,   CMP_DEF },
    { FIELD_GENRE,        "genre",        TUPLE_STRING,   TRUE,   CMP_DEF },
    { FIELD_FORMATTER,    "formatter",    TUPLE_STRING,   TRUE,   CMP_DEF },
};

static const gint xspf_nentries = (sizeof(xspf_entries) / sizeof(xspf_entry_t));


#ifdef DEBUG
#  define XSDEBUG(...) { fprintf(stderr, "xspf[%s:%d]: ", __FUNCTION__, (int) __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
#  define XSDEBUG(...) /* stub */
#endif


static gboolean is_uri(gchar *uri)
{
    if (strstr(uri, "://"))
        return TRUE;
    else
        return FALSE;
}


/* This function is taken from libxml2-2.6.27.
 */
static xmlChar *xspf_path_to_uri(const xmlChar *path)
{
    xmlURIPtr uri;
    xmlURI temp;
    xmlChar *ret, *cal;

    if (path == NULL)
        return NULL;

    if ((uri = xmlParseURI((const char *)path)) != NULL) {
        xmlFreeURI(uri);
        return xmlStrdup(path);
    }

    cal = xmlCanonicPath(path);
    if (cal == NULL)
        return NULL;
    
    memset(&temp, 0, sizeof(temp));
    temp.path = (char *)cal;
    ret = xmlSaveUri(&temp);
    xmlFree(cal);
    
    return ret;
}


static void xspf_add_file(xmlNode *track, const gchar *filename,
            gint pos, const gchar *base)
{
    xmlNode *nptr;
    Tuple *tuple;
    gchar *location = NULL;
    Playlist *playlist = playlist_get_active();


    tuple = tuple_new();
    tuple_associate_int(tuple, FIELD_LENGTH, NULL, -1);
    tuple_associate_int(tuple, FIELD_MTIME, NULL, -1);


    for (nptr = track->children; nptr != NULL; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(nptr->name, (xmlChar *)"location")) {
                /* Location is a special case */
                gchar *str = (gchar *)xmlNodeGetContent(nptr);

                location = g_strdup_printf("%s%s", base ? base : "", str);
                xmlFree(str);
                str = g_filename_from_uri(location, NULL, NULL);
                if (str) {
                    g_free(location);
                    location = g_strdup_printf("file://%s", str);
                }
            
                g_free(str);
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
                            tuple_associate_string(tuple, xspf_entries[i].tupleField, NULL, (gchar *)str);
                            break;
                        
                        case TUPLE_INT:
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

    if (location) {
        gchar *uri = NULL;
        gchar *scratch;

        scratch = g_path_get_basename(location);
        tuple_associate_string(tuple, FIELD_FILE_NAME, NULL, scratch);
        g_free(scratch);

        scratch = g_path_get_dirname(location);
        tuple_associate_string(tuple, FIELD_FILE_PATH, NULL, scratch);
        g_free(scratch);

        tuple_associate_string(tuple, FIELD_FILE_EXT, NULL, strrchr(location, '.'));

        XSDEBUG("tuple->file_name = %s\n", tuple_get_string(tuple, FIELD_FILE_NAME, NULL));
        XSDEBUG("tuple->file_path = %s\n", tuple_get_string(tuple, FIELD_FILE_PATH, NULL));

        // add file to playlist
        uri = g_filename_to_uri(location, NULL, NULL);
        // uri would be NULL if location is already uri. --yaz
        playlist_load_ins_file_tuple(playlist, uri ? uri: location, filename, pos, tuple);
        g_free(uri);
        pos++;
    }

    g_free(location);
}


static void xspf_find_track(xmlNode *tracklist, const gchar *filename,
            gint pos, const gchar *base)
{
    xmlNode *nptr;

    for (nptr = tracklist->children; nptr != NULL; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE &&
            !xmlStrcmp(nptr->name, (xmlChar *)"track")) {
            xspf_add_file(nptr, filename, pos, base);
        }
    }
}


static void xspf_find_audoptions(xmlNode *tracklist, const gchar *filename, gint pos)
{
    xmlNode *nptr;
    Playlist *playlist = playlist_get_active();

    for (nptr = tracklist->children; nptr != NULL; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE &&
            !xmlStrcmp(nptr->name, (xmlChar *)"options")) {
            xmlChar *opt = NULL;

            opt = xmlGetProp(nptr, (xmlChar *)"staticlist");
            if (!g_strcasecmp((char *)opt, "true"))
                playlist->attribute |= PLAYLIST_STATIC;
            else
                playlist->attribute ^= PLAYLIST_STATIC;

            xmlFree(opt);
        }
    }
}


static void xspf_playlist_load(const gchar *filename, gint pos)
{
    xmlDocPtr doc;
    xmlNode *nptr, *nptr2;

    g_return_if_fail(filename != NULL);

    XSDEBUG("filename='%s', pos=%d\n", filename, pos);

    doc = xmlRecoverFile(filename);
    if (doc == NULL)
        return;

    // find trackList
    for (nptr = doc->children; nptr != NULL; nptr = nptr->next) {
        if (nptr->type == XML_ELEMENT_NODE &&
            !xmlStrcmp(nptr->name, (xmlChar *)"playlist")) {
            gchar *tmp, *base;
            
            base = (gchar *)xmlNodeGetBase(doc, nptr);

            XSDEBUG("base @1 = %s\n", base);
            
            // if filename is specified as a base, ignore it.
            tmp = xmlURIUnescapeString(base, -1, NULL);
            if (tmp) {
                if (!strcmp(tmp, filename)) {   
                    xmlFree(base);
                    base = NULL;
                }
                g_free(tmp);
            }
            
            XSDEBUG("base @2 = %s\n", base);
            
            for (nptr2 = nptr->children; nptr2 != NULL; nptr2 = nptr2->next) {

                if (nptr2->type == XML_ELEMENT_NODE &&
                    !xmlStrcmp(nptr2->name, (xmlChar *)"extension")) {
                    //check if application is audacious
                    xmlChar *app = NULL;
                    app = xmlGetProp(nptr2, (xmlChar *)"application");
                    if (!xmlStrcmp(app, (xmlChar *)"audacious"))
                        xspf_find_audoptions(nptr2, filename, pos);
                    xmlFree(app);
                } else
                if (nptr2->type == XML_ELEMENT_NODE &&
                    !xmlStrcmp(nptr2->name, (xmlChar *)"title")) {
                    Playlist *plist = playlist_get_active();
                    xmlChar *title = xmlNodeGetContent(nptr2);
                    
                    if (title && *title) {
                        playlist_set_current_name(plist, (gchar*)title);
                    }
                    xmlFree(title);
                } else
                if (nptr2->type == XML_ELEMENT_NODE &&
                    !xmlStrcmp(nptr2->name, (xmlChar *)"trackList")) {
                    xspf_find_track(nptr2, filename, pos, base);
                }
            }
        }
    }
    xmlFreeDoc(doc);
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
        case TUPLE_STRING:
            xmlAddChild(tmp, xmlNewText((xmlChar *) strVal));
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


static void xspf_playlist_save(const gchar *filename, gint pos)
{
    xmlDocPtr doc;
    xmlNodePtr rootnode, tracklist;
    GList *node;
    gint baselen = 0;
    gchar *base = NULL;
    Playlist *playlist = playlist_get_active();

    XSDEBUG("filename='%s', pos=%d\n", filename, pos);

    doc = xmlNewDoc((xmlChar *)"1.0");
    doc->charset = XML_CHAR_ENCODING_UTF8;
    doc->encoding = xmlStrdup((xmlChar *)"UTF-8");

    rootnode = xmlNewNode(NULL, (xmlChar *)XSPF_ROOT_NODE_NAME);
    xmlSetProp(rootnode, (xmlChar *)"version", (xmlChar *)"1");
    xmlSetProp(rootnode, (xmlChar *)"xmlns", (xmlChar *)XSPF_XMLNS);

    PLAYLIST_LOCK(playlist);

    /* relative */
    if (playlist->attribute & PLAYLIST_USE_RELATIVE) {
        /* prescan to determine base uri */
        for (node = playlist->entries; node != NULL; node = g_list_next(node)) {
            gchar *ptr1, *ptr2;
            PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
            gchar *tmp;
            gint tmplen = 0;

            if (!is_uri(entry->filename)) { //obsolete
                gchar *tmp2;
                tmp2 = g_path_get_dirname(entry->filename);
                tmp = g_strdup_printf("%s/", tmp2);
                g_free(tmp2);
            } else {
                tmp = g_strdup(entry->filename);
            }

            if (!base) {
                base = strdup(tmp);
                baselen = strlen(base);
            }
            
            ptr1 = base;
            ptr2 = tmp;

            while(ptr1 && ptr2 && *ptr1 && *ptr2 && *ptr1 == *ptr2) {
                ptr1++;
                ptr2++;
            }
            
            *ptr2 = '\0';       //terminate
            tmplen = ptr2 - tmp;

            if (tmplen <= baselen) {
                g_free(base);
                base = tmp;
                baselen = tmplen;
                XSDEBUG("base='%s', baselen=%d\n", base, baselen);
            }
            else {
                g_free(tmp);
            }
        }
        
        /* set base URI */
        if (base) {
            gchar *tmp;
            if (!is_uri(base)) {
                tmp = (gchar *) xspf_path_to_uri((xmlChar *)base);
                if (tmp) {
                    g_free(base);
                    base = tmp;
                }
            }

            if (!is_uri(base)) {
                XSDEBUG("base is not uri. something is wrong.\n");
                tmp = g_strdup_printf("file://%s", base);
                xmlSetProp(rootnode, (xmlChar *)"xml:base", (xmlChar *)tmp);
                g_free(tmp);
            }
            else
                xmlSetProp(rootnode, (xmlChar *)"xml:base", (xmlChar *)base);
        }
    }                           /* USE_RELATIVE */

    /* common */
    xmlDocSetRootElement(doc, rootnode);
    xspf_add_node(rootnode, TUPLE_STRING, FALSE, "creator", PACKAGE "-" VERSION, 0);

    /* add staticlist marker */
    if (playlist->attribute & PLAYLIST_STATIC) {
        xmlNodePtr extension, options;

        extension = xmlNewNode(NULL, (xmlChar *)"extension");
        xmlSetProp(extension, (xmlChar *)"application", (xmlChar *)"audacious");

        options = xmlNewNode(NULL, (xmlChar *)"options");
        xmlSetProp(options, (xmlChar *)"staticlist", (xmlChar *)"true");

        xmlAddChild(extension, options);
        xmlAddChild(rootnode, extension);
    }

    /* save playlist title */
    if (playlist->title && playlist->title[0] &&
        g_utf8_validate(playlist->title, -1, NULL))
        xspf_add_node(rootnode, TUPLE_STRING, FALSE, "title", playlist->title, 0);


    tracklist = xmlNewNode(NULL, (xmlChar *)"trackList");
    xmlAddChild(rootnode, tracklist);

    for (node = playlist->entries; node != NULL; node = g_list_next(node)) {
        PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
        xmlNodePtr track, location;
        gchar *filename = NULL;
        const gchar *scratch = NULL;
        gint scratchi = 0;

        track = xmlNewNode(NULL, (xmlChar *)"track");
        location = xmlNewNode(NULL, (xmlChar *)"location");

        if (is_uri(entry->filename)) {   /* uri */
            XSDEBUG("filename is uri\n");
            filename = g_strdup(entry->filename + baselen); // entry->filename is always uri now.
        }
        else {                  /* local file (obsolete) */
            gchar *tmp = (gchar *) xspf_path_to_uri((const xmlChar *)entry->filename + baselen);
            if (base) { /* relative */
                filename = g_strdup_printf("%s", tmp);
            } else {
                XSDEBUG("absolute and local (obsolete)\n");
                filename = g_filename_to_uri(tmp, NULL, NULL);
            }
            g_free(tmp);
        } /* obsolete */

        if (!g_utf8_validate(filename, -1, NULL))
            continue;

        xmlAddChild(location, xmlNewText((xmlChar *)filename));
        xmlAddChild(track, location);
        xmlAddChild(tracklist, track);

        /* Do we have a tuple? */
        if (entry->tuple != NULL) {
            gint i;
            for (i = 0; i < xspf_nentries; i++) {
                const xspf_entry_t *xs = &xspf_entries[i];
                gboolean isOK = FALSE;
                
                switch (xs->type) {
                    case TUPLE_STRING:
                        scratch = tuple_get_string(entry->tuple, xs->tupleField, NULL);
                        switch (xs->compare) {
                            case CMP_DEF: isOK = (scratch != NULL); break;
                            case CMP_NULL: isOK = (scratch == NULL); break;
                        }
                        if (scratch != NULL && !g_utf8_validate(scratch, -1, NULL))
                            isOK = FALSE;
                        break;
                    
                    case TUPLE_INT:
                        scratchi = tuple_get_int(entry->tuple, xs->tupleField, NULL);
                        switch (xs->compare) {
                            case CMP_DEF: isOK = (scratchi != 0); break;
                            case CMP_GT:  isOK = (scratchi > 0); break;
                        }
                        break;
                        
                    default:
                        break;
                }
                
                if (isOK)
                    xspf_add_node(track, xs->type, xs->isMeta, xs->xspfName, scratch, scratchi);
            }

            /* Write mtime unconditionally to support staticlist */
            xspf_add_node(track, TUPLE_INT, TRUE, "mtime", NULL,
                tuple_get_int(entry->tuple, FIELD_MTIME, NULL));
        } else {

            if (entry->title != NULL && g_utf8_validate(entry->title, -1, NULL))
                xspf_add_node(track, TUPLE_STRING, FALSE, "title", entry->title, 0);

            if (entry->length > 0)
                xspf_add_node(track, TUPLE_INT, FALSE, "duration", NULL, entry->length);

            /* Add mtime of -1 */
            xspf_add_node(track, TUPLE_INT, TRUE, "mtime", NULL, -1);
        }

        g_free(filename);
    }

    PLAYLIST_UNLOCK(playlist);

    xmlSaveFormatFile(filename, doc, 1);
    xmlFreeDoc(doc);
    xmlFree(base);
}


PlaylistContainer plc_xspf = {
    .name = "XSPF Playlist Format",
    .ext = "xspf",
    .plc_read = xspf_playlist_load,
    .plc_write = xspf_playlist_save,
};


static void xspf_init(void)
{
    playlist_container_register(&plc_xspf);
}


static void xspf_cleanup(void)
{
    playlist_container_unregister(&plc_xspf);
}


DECLARE_PLUGIN(xspf, xspf_init, xspf_cleanup, NULL, NULL, NULL, NULL, NULL, NULL);
